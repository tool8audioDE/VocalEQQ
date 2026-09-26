#include "ResonanceEq.h"

#include <algorithm>
#include <cmath>

#include "DspCommon.h"

namespace vocaleqq
{

namespace
{
    constexpr float thresholdDb = 5.0f;
    constexpr float maxCutDb = 6.0f;
    constexpr float lowHz = 150.0f, highHz = 16000.0f;
    constexpr float highpassHz = 100.0f, highpassSlopeDbPerOctave = 12.0f;
    constexpr float lowShelfHz = 250.0f, lowShelfWidthOctaves = 1.5f;

    /** Fenstergrenzen der Bruchteil-Oktav-Glättung, wie
        octave_smooth_spectrogram: Bin i mittelt über [i/f, i·f] mit
        f = 2^(1/(2·fraction)). Die Bins liegen linear in der Frequenz, das
        geometrische Fenster lässt sich deshalb direkt im Index ausdrücken.
    */
    void smoothingWindows (int numBins, float fraction, std::vector<int>& lo, std::vector<int>& hi)
    {
        const double factor = std::pow (2.0, 1.0 / (2.0 * fraction));
        lo.resize (static_cast<size_t> (numBins));
        hi.resize (static_cast<size_t> (numBins));

        for (int i = 0; i < numBins; ++i)
        {
            lo[static_cast<size_t> (i)] = std::clamp (static_cast<int> (std::floor (i / factor)), 0, numBins);
            hi[static_cast<size_t> (i)] = std::clamp (static_cast<int> (std::floor (i * factor)) + 1, 0, numBins);
        }
    }

    void smooth (const std::vector<float>& cumulative, const std::vector<int>& lo, const std::vector<int>& hi,
                 std::vector<float>& out) noexcept
    {
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = (cumulative[static_cast<size_t> (hi[i])] - cumulative[static_cast<size_t> (lo[i])])
                     / static_cast<float> (hi[i] - lo[i]);
    }
}

void ResonanceEq::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    fftSize = scaledFftSize (4096, sampleRate);
    hop = fftSize / 4;
    numBins = fftSize / 2 + 1;
    activeChannels = std::clamp (numChannels, 1, maxChannels);

    fft = Fft (fftSize);
    window = periodicHann (fftSize);

    // Hann für Analyse und Synthese bei einem Viertel Sprung: Die Quadrate
    // der überlappenden Fenster summieren sich überall zu 1,5 (librosa.istft
    // teilt durch genau diese Summe).
    double overlapSum = 0.0;
    for (int offset = 0; offset < fftSize; offset += hop)
        overlapSum += static_cast<double> (window[static_cast<size_t> (offset)]) * window[static_cast<size_t> (offset)];
    synthesisScale = static_cast<float> (1.0 / std::max (overlapSum, 1.0e-9));

    binFrequencies.resize (static_cast<size_t> (numBins));
    for (int k = 0; k < numBins; ++k)
        binFrequencies[static_cast<size_t> (k)] = static_cast<float> (k * sampleRate / fftSize);

    const auto bins = static_cast<size_t> (numBins);
    magnitudeDb.assign (bins, 0.0f);
    detail.assign (bins, 0.0f);
    baseline.assign (bins, 0.0f);
    cumulative.assign (bins + 1, 0.0f);
    tonalDb.assign (bins, 0.0f);
    gainDb.assign (bins, 0.0f);
    spectrum.assign (static_cast<size_t> (fftSize), {});

    smoothingWindows (numBins, 6.0f, detailLo, detailHi);
    smoothingWindows (numBins, 1.0f, baselineLo, baselineHi);

    const double framesPerSecond = sampleRate / hop;
    alphaAttack = smoothingCoefficient (0.008, framesPerSecond);
    alphaRelease = smoothingCoefficient (0.080, framesPerSecond);

    for (auto& r : inputRing)  r.assign (static_cast<size_t> (fftSize), 0.0f);
    for (auto& r : outputRing) r.assign (static_cast<size_t> (fftSize), 0.0f);

    {
        const std::lock_guard<std::mutex> lock (displayLock);
        displaySpectrum.assign (bins, -120.0f);
        displayCorrection.assign (bins, 0.0f);
        displayTonal.assign (bins, 0.0f);
        displayValid = false;
    }

    tonalLowCutDb = -1.0f;
    reset();
}

void ResonanceEq::reset()
{
    for (auto& r : inputRing)  std::fill (r.begin(), r.end(), 0.0f);
    for (auto& r : outputRing) std::fill (r.begin(), r.end(), 0.0f);
    pos = 0;
    hopCounter = 0;

    // Das Vorbild beginnt die Glättung bei 0 dB und blendet die Tonbalance
    // damit über die ersten Rahmen ein. Hier steht sie sofort.
    updateTonalCurve();
    gainDb = enabled ? tonalDb : std::vector<float> (tonalDb.size(), 0.0f);
}

void ResonanceEq::updateTonalCurve() noexcept
{
    if (lowCutDb == tonalLowCutDb)
        return;

    tonalLowCutDb = lowCutDb;
    const float amountDb = -std::max (lowCutDb, 0.0f);

    for (int k = 0; k < numBins; ++k)
    {
        // Gleichanteil: Das Vorbild lässt Bin 0 beim Hochpass aus (f = 0).
        // Hier bekommt er die Absenkung des ersten Bins — Gleichspannung soll
        // ein Hochpass gerade entfernen.
        const float f = std::max (binFrequencies[static_cast<size_t> (k)], binFrequencies[1]);

        float g = 0.0f;
        if (f < highpassHz)
            g -= highpassSlopeDbPerOctave * std::log2 (highpassHz / f);

        const float x = std::log2 (f / lowShelfHz) / lowShelfWidthOctaves;
        g += amountDb * (1.0f - 1.0f / (1.0f + std::exp (-4.0f * x)));

        tonalDb[static_cast<size_t> (k)] = g;
    }
}

void ResonanceEq::processFrame() noexcept
{
    const int n = fftSize;

    // pos zeigt auf das älteste Sample des Rahmens.
    for (int i = 0; i < n; ++i)
    {
        const auto idx = static_cast<size_t> ((pos + i) % n);
        const float w = window[static_cast<size_t> (i)];
        const float left = inputRing[0][idx] * w;
        const float right = activeChannels > 1 ? inputRing[1][idx] * w : 0.0f;
        spectrum[static_cast<size_t> (i)] = { left, right };
    }

    fft.forward (spectrum.data());

    // Mono-Summe aus den beiden gemeinsam transformierten Kanälen:
    // X_L = (Z[k] + conj Z[N−k]) / 2, X_R = (Z[k] − conj Z[N−k]) / 2i.
    for (int k = 0; k < numBins; ++k)
    {
        const auto z = spectrum[static_cast<size_t> (k)];
        std::complex<float> mono;

        if (activeChannels > 1)
        {
            const auto zc = std::conj (spectrum[static_cast<size_t> ((n - k) % n)]);
            const auto left = 0.5f * (z + zc);
            const auto right = std::complex<float> (0.0f, -0.5f) * (z - zc);
            mono = 0.5f * (left + right);
        }
        else
        {
            mono = z;
        }

        magnitudeDb[static_cast<size_t> (k)] = 20.0f * std::log10 (std::abs (mono) + 1.0e-9f);
    }

    cumulative[0] = 0.0f;
    for (int k = 0; k < numBins; ++k)
        cumulative[static_cast<size_t> (k + 1)] = cumulative[static_cast<size_t> (k)] + magnitudeDb[static_cast<size_t> (k)];

    smooth (cumulative, detailLo, detailHi, detail);
    smooth (cumulative, baselineLo, baselineHi, baseline);

    updateTonalCurve();

    for (int k = 0; k < numBins; ++k)
    {
        const auto kk = static_cast<size_t> (k);
        float target = 0.0f;

        if (enabled)
        {
            const float f = binFrequencies[kk];
            if (f >= lowHz && f <= highHz)
            {
                const float over = std::max (detail[kk] - baseline[kk] - thresholdDb, 0.0f);
                target = -std::clamp (over * strength, 0.0f, maxCutDb);
            }
            target += tonalDb[kk];
        }

        // Tiefer = mehr Absenkung = Attack, wie _temporal_smooth.
        const float alpha = target < gainDb[kk] ? alphaAttack : alphaRelease;
        gainDb[kk] = alpha * gainDb[kk] + (1.0f - alpha) * target;

        const float g = dbToGain (gainDb[kk]);
        spectrum[kk] *= g;
        if (k > 0 && k < n - k)
            spectrum[static_cast<size_t> (n - k)] *= g;
    }

    fft.inverse (spectrum.data());

    for (int i = 0; i < n; ++i)
    {
        const auto idx = static_cast<size_t> ((pos + i) % n);
        const float w = window[static_cast<size_t> (i)] * synthesisScale;
        const auto y = spectrum[static_cast<size_t> (i)];
        outputRing[0][idx] += y.real() * w;
        if (activeChannels > 1)
            outputRing[1][idx] += y.imag() * w;
    }

    // Anzeige: nie warten. Ist die Oberfläche gerade am Lesen, fällt dieser
    // Rahmen eben aus — der nächste kommt 23 ms später.
    if (displayLock.try_lock())
    {
        std::copy (detail.begin(), detail.end(), displaySpectrum.begin());
        std::copy (gainDb.begin(), gainDb.end(), displayCorrection.begin());
        if (enabled)
            std::copy (tonalDb.begin(), tonalDb.end(), displayTonal.begin());
        else
            std::fill (displayTonal.begin(), displayTonal.end(), 0.0f);
        displayValid = true;
        displayLock.unlock();
    }

    frameCount.fetch_add (1, std::memory_order_relaxed);
}

void ResonanceEq::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int chans = std::min (numChannels, activeChannels);
    if (chans <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto p = static_cast<size_t> (pos);

        for (int ch = 0; ch < chans; ++ch)
        {
            auto& in = inputRing[static_cast<size_t> (ch)];
            auto& out = outputRing[static_cast<size_t> (ch)];
            in[p] = channels[ch][i];
            channels[ch][i] = out[p];
            out[p] = 0.0f;
        }

        pos = (pos + 1) % fftSize;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

bool ResonanceEq::copyDisplay (std::vector<float>& spectrumDb, std::vector<float>& correctionDb, std::vector<float>& tonalDbOut)
{
    const std::lock_guard<std::mutex> lock (displayLock);
    spectrumDb = displaySpectrum;
    correctionDb = displayCorrection;
    tonalDbOut = displayTonal;
    return displayValid;
}

} // namespace vocaleqq
