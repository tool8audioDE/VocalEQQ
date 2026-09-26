#include "AutoCompressor.h"

#include <algorithm>
#include <cmath>

namespace vocaleqq
{

namespace
{
    constexpr float gateDb = -50.0f;
    constexpr float kneeDb = 6.0f;

    // Die Schätzer laufen alle 32 Samples, also rund 1400-mal pro Sekunde.
    // Je träger, desto näher am Vorbild, das die Perzentile über die ganze
    // Datei rechnet. An zwei Gesangsspuren (je 111 s) gemessen, mittlere
    // Abweichung der Absenkung vom Vorbild je 10 ms: 0,05 dB Schritt 2,1 bis
    // 2,6 dB, 0,01 dB 1,1 bis 1,3 dB, 0,005 dB 0,8 bis 1,1 dB. Der Median
    // wandert damit noch 3,4 dB pro Sekunde — nach einem Einsatz ist die
    // Schwelle in wenigen Sekunden da.
    constexpr float statsStepDb = 0.005f;
}

void AutoCompressor::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    window = std::max (static_cast<int> (0.020 * sampleRate), 1);
    squares.assign (static_cast<size_t> (window), 0.0);

    alphaAttack = smoothingCoefficient (0.010, sampleRate);
    alphaRelease = smoothingCoefficient (0.150, sampleRate);

    for (int ch = 0; ch < std::min (numChannels, maxChannels); ++ch)
        delays[static_cast<size_t> (ch)].prepare (getLatencySamples());

    reset();
}

void AutoCompressor::reset()
{
    std::fill (squares.begin(), squares.end(), 0.0);
    sumSquares = 0.0;
    squarePos = recomputeCounter = statsCounter = 0;

    // Anfangswerte für eine typische Gesangsaufnahme; die Schätzer laufen
    // von hier aus zur tatsächlichen Verteilung.
    p10.reset (-36.0f);
    p50.reset (-24.0f);
    p90.reset (-16.0f);
    updateSettings();

    reductionDb = 0.0f;
    for (auto& d : delays) d.reset();
}

void AutoCompressor::updateSettings() noexcept
{
    // auto_settings aus dem Vorbild.
    const float a = std::clamp (amount, 0.0f, 1.0f);
    const float span = std::max (p90.get() - p50.get(), 0.0f);
    thresholdDb = p50.get() - a * 0.3f * span;

    const float r = std::clamp (2.0f + (p90.get() - p10.get()) / 5.0f, 2.0f, 4.0f);
    ratio = 1.0f + (r - 1.0f) * (0.5f + 0.5f * a);
}

void AutoCompressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);
    if (numChannels <= 0)
        return;

    float slope = 1.0f / ratio - 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (channels[ch][i]));

        // Laufende Summe der Quadrate über das Detektorfenster. Alle paar
        // Sekunden neu aufsummiert, damit sich keine Rundungsfehler sammeln.
        const double sq = static_cast<double> (peak) * peak;
        auto& slot = squares[static_cast<size_t> (squarePos)];
        sumSquares += sq - slot;
        slot = sq;
        squarePos = (squarePos + 1) % window;

        if (++recomputeCounter >= window * 200)
        {
            recomputeCounter = 0;
            sumSquares = 0.0;
            for (double v : squares)
                sumSquares += v;
        }

        const float envelope = static_cast<float> (std::sqrt (std::max (sumSquares, 0.0) / window));
        const float levelDb = 20.0f * std::log10 (envelope + 1.0e-9f);

        if (++statsCounter >= statsInterval)
        {
            statsCounter = 0;
            if (levelDb > gateDb)
            {
                p10.update (levelDb, 0.10f, statsStepDb);
                p50.update (levelDb, 0.50f, statsStepDb);
                p90.update (levelDb, 0.90f, statsStepDb);
            }
            updateSettings();
            slope = 1.0f / ratio - 1.0f;
        }

        // Soft-Knee wie _gain_curve im Vorbild.
        float target = 0.0f;
        if (enabled)
        {
            const float d = levelDb - thresholdDb;
            if (2.0f * d < -kneeDb)
                target = 0.0f;
            else if (2.0f * std::abs (d) <= kneeDb)
            {
                const float t = d + kneeDb / 2.0f;
                target = slope * t * t / (2.0f * kneeDb);
            }
            else
                target = slope * d;
        }

        const float alpha = target < reductionDb ? alphaAttack : alphaRelease;
        reductionDb = alpha * reductionDb + (1.0f - alpha) * target;

        const float gain = dbToGain (reductionDb);
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = delays[static_cast<size_t> (ch)].process (channels[ch][i]) * gain;
    }
}

} // namespace vocaleqq
