#include "DeEsser.h"

#include <algorithm>
#include <cmath>

namespace vocaleqq
{

namespace
{
    constexpr float crossoverHz = 4000.0f;
    constexpr float highBandLowHz = 4000.0f, highBandHighHz = 12000.0f;
    constexpr float energyGateDb = -50.0f;

    // Klammern der Schwellen, wie _adaptive_thresholds im Vorbild. Sie halten
    // die Erkennung auch dann vernünftig, wenn das Perzentil selbst
    // danebenliegt, etwa kurz nach dem Start.
    constexpr float highRatioMin = 0.30f, highRatioMax = 0.75f;
    constexpr float centroidMin = 4500.0f, centroidMax = 9000.0f;
    constexpr float zcrMin = 0.12f, zcrMax = 0.40f;
    constexpr float fluxMin = 0.20f, fluxMax = 0.70f;

    float percentileToQuantile (float basePercent, float sensitivity) noexcept
    {
        const float shift = (0.5f - sensitivity) * 24.0f;
        return std::clamp (basePercent + shift, 50.0f, 99.0f) / 100.0f;
    }
}

void DeEsser::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    frameSize = scaledFftSize (2048, sampleRate);
    hop = frameSize / 8;

    fft = Fft (frameSize);
    window = periodicHann (frameSize);
    ring.assign (static_cast<size_t> (frameSize), 0.0f);
    frame.assign (static_cast<size_t> (frameSize), 0.0f);
    spectrum.assign (static_cast<size_t> (frameSize), {});

    const int numBins = frameSize / 2 + 1;
    magnitudes.assign (static_cast<size_t> (numBins), 0.0f);
    binFrequencies.resize (static_cast<size_t> (numBins));
    for (int k = 0; k < numBins; ++k)
        binFrequencies[static_cast<size_t> (k)] = static_cast<float> (k * sampleRate / frameSize);

    highFirstBin = static_cast<int> (std::ceil (highBandLowHz * frameSize / sampleRate));
    highLastBin = std::min (numBins - 1, static_cast<int> (std::floor (highBandHighHz * frameSize / sampleRate)));
    previousHigh.assign (static_cast<size_t> (std::max (highLastBin - highFirstBin + 1, 1)), 0.0f);

    const double framesPerSecond = sampleRate / hop;
    alphaAttack = smoothingCoefficient (0.002, framesPerSecond);
    alphaRelease = smoothingCoefficient (0.060, framesPerSecond);

    // Das Vorbild normiert den Flux auf das Maximum der Datei. Hier: ein
    // Spitzenwert, der in rund zehn Sekunden auf ein Drittel abklingt.
    fluxPeakDecay = smoothingCoefficient (10.0, framesPerSecond);

    for (int ch = 0; ch < std::min (numChannels, maxChannels); ++ch)
    {
        crossovers[static_cast<size_t> (ch)].prepare (crossoverHz, sampleRate);
        delays[static_cast<size_t> (ch)].prepare (getLatencySamples());
    }

    reset();
}

void DeEsser::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (previousHigh.begin(), previousHigh.end(), 0.0f);
    ringPos = 0;
    hopCounter = 0;

    // Start in der Mitte der Klammern: vorsichtig, bis genug Signal da war.
    highRatioThreshold.reset (0.5f * (highRatioMin + highRatioMax));
    centroidThreshold.reset (0.5f * (centroidMin + centroidMax));
    zcrThreshold.reset (0.5f * (zcrMin + zcrMax));
    fluxThreshold.reset (0.5f * (fluxMin + fluxMax));
    fluxPeak = 0.0f;

    maskSmoothed = rampValue = rampStep = 0.0f;
    currentReductionDb = 0.0f;

    for (auto& c : crossovers) c.reset();
    for (auto& d : delays)     d.reset();
}

void DeEsser::analyseFrame() noexcept
{
    // Rahmen in zeitlicher Reihenfolge: ringPos zeigt auf das älteste Sample.
    for (int i = 0; i < frameSize; ++i)
        frame[static_cast<size_t> (i)] = ring[static_cast<size_t> ((ringPos + i) % frameSize)];

    double sumSquares = 0.0;
    int crossings = 0;
    for (int i = 0; i < frameSize; ++i)
    {
        const float x = frame[static_cast<size_t> (i)];
        sumSquares += static_cast<double> (x) * x;

        // librosa.zero_crossings: Werte unter 1e-10 gelten als 0, und 0 als positiv.
        if (i > 0)
        {
            const float prev = frame[static_cast<size_t> (i - 1)];
            const bool negNow = x < -1.0e-10f;
            const bool negPrev = prev < -1.0e-10f;
            crossings += negNow != negPrev ? 1 : 0;
        }
    }

    const float rmsDb = 20.0f * std::log10 (static_cast<float> (std::sqrt (sumSquares / frameSize)) + 1.0e-10f);
    const float zcr = static_cast<float> (crossings) / static_cast<float> (frameSize);

    for (int i = 0; i < frameSize; ++i)
        spectrum[static_cast<size_t> (i)] = { frame[static_cast<size_t> (i)] * window[static_cast<size_t> (i)], 0.0f };

    fft.forward (spectrum.data());

    const int numBins = frameSize / 2 + 1;
    double total = 1.0e-10, high = 0.0, weighted = 0.0, magSum = 0.0, flux = 0.0;

    for (int k = 0; k < numBins; ++k)
    {
        const float mag = std::abs (spectrum[static_cast<size_t> (k)]);
        magnitudes[static_cast<size_t> (k)] = mag;

        const double power = static_cast<double> (mag) * mag;
        total += power;
        weighted += static_cast<double> (binFrequencies[static_cast<size_t> (k)]) * mag;
        magSum += mag;

        if (k >= highFirstBin && k <= highLastBin)
        {
            high += power;

            auto& prev = previousHigh[static_cast<size_t> (k - highFirstBin)];
            const float rise = std::max (mag - prev, 0.0f);
            flux += static_cast<double> (rise) * rise;
            prev = mag;
        }
    }

    const float highRatio = static_cast<float> (high / total);
    const float centroid = magSum > 0.0 ? static_cast<float> (weighted / magSum) : 0.0f;
    const int highBins = std::max (highLastBin - highFirstBin + 1, 1);
    const float fluxRaw = static_cast<float> (std::sqrt (flux / highBins));

    fluxPeak = std::max (fluxRaw, fluxPeak * fluxPeakDecay);
    const float fluxNorm = fluxRaw / (fluxPeak + 1.0e-10f);

    const bool voiced = rmsDb >= energyGateDb;

    const float qHighRatio = percentileToQuantile (90.0f, sensitivity);
    const float qCentroid  = percentileToQuantile (85.0f, sensitivity);
    const float qZcr       = percentileToQuantile (85.0f, sensitivity);
    const float qFlux      = percentileToQuantile (88.0f, sensitivity);

    if (voiced)
    {
        // Schrittweiten: rund ein Prozent der Klammerbreite je Rahmen. Bei
        // 172 Rahmen pro Sekunde sitzt eine Schwelle nach wenigen Sekunden
        // Gesang, zittert danach aber nur um einen Bruchteil ihrer Breite.
        highRatioThreshold.update (highRatio, qHighRatio, 0.005f);
        centroidThreshold.update (centroid, qCentroid, 40.0f);
        zcrThreshold.update (zcr, qZcr, 0.003f);
        fluxThreshold.update (fluxNorm, qFlux, 0.005f);
    }

    const float tHighRatio = std::clamp (highRatioThreshold.get(), highRatioMin, highRatioMax);
    const float tCentroid  = std::clamp (centroidThreshold.get(), centroidMin, centroidMax);
    const float tZcr       = std::clamp (zcrThreshold.get(), zcrMin, zcrMax);
    const float tFlux      = std::clamp (fluxThreshold.get(), fluxMin, fluxMax);

    // min_votes = 2 im Vorbild: der Hochton-Anteil plus mindestens eine Stimme.
    const int support = (centroid > tCentroid ? 1 : 0) + (zcr > tZcr ? 1 : 0) + (fluxNorm > tFlux ? 1 : 0);
    const float mask = (voiced && enabled && highRatio > tHighRatio && support >= 1) ? 1.0f : 0.0f;

    const float alpha = mask > maskSmoothed ? alphaAttack : alphaRelease;
    maskSmoothed = alpha * maskSmoothed + (1.0f - alpha) * mask;

    // Lineare Rampe bis zum nächsten Rahmen, wie np.interp zwischen den
    // Rahmenzeiten im Vorbild.
    rampStep = (maskSmoothed - rampValue) / static_cast<float> (hop);
}

void DeEsser::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);
    if (numChannels <= 0)
        return;

    const float depth = 1.0f - dbToGain (-std::max (reductionDb, 0.0f));
    const float channelScale = 1.0f / static_cast<float> (numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += channels[ch][i];

        ring[static_cast<size_t> (ringPos)] = mono * channelScale;
        ringPos = (ringPos + 1) % frameSize;

        rampValue += rampStep;
        const float multiplier = 1.0f - std::clamp (rampValue, 0.0f, 1.0f) * depth;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Weiche immer, auch ausgeschaltet: So bleibt die Phase gleich,
            // und Ein-/Ausschalten knackt nicht — die Maske klingt einfach aus.
            float low = 0.0f, highBand = 0.0f;
            crossovers[static_cast<size_t> (ch)].process (delays[static_cast<size_t> (ch)].process (channels[ch][i]), low, highBand);
            channels[ch][i] = low + highBand * multiplier;
        }

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            analyseFrame();
        }
    }

    currentReductionDb = gainToDb (1.0f - std::clamp (rampValue, 0.0f, 1.0f) * depth);
}

} // namespace vocaleqq
