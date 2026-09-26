#pragma once

#include <cmath>
#include <random>
#include <vector>

#include "dsp/DspCommon.h"
#include "dsp/Filters.h"

namespace vocaleqq::test
{

inline std::vector<float> sine (double frequency, double sampleRate, int numSamples, float amplitude = 0.5f)
{
    std::vector<float> out (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t> (i)] = amplitude * static_cast<float> (std::sin (2.0 * pi * frequency * i / sampleRate));
    return out;
}

inline std::vector<float> noise (int numSamples, float amplitude, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    std::vector<float> out (static_cast<size_t> (numSamples));
    for (auto& v : out)
        v = dist (rng);
    return out;
}

/** Effektivwert eines Ausschnitts in dB. */
inline float rmsDb (const std::vector<float>& x, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i)
        sum += static_cast<double> (x[static_cast<size_t> (i)]) * x[static_cast<size_t> (i)];
    return 10.0f * static_cast<float> (std::log10 (sum / std::max (end - start, 1) + 1.0e-20));
}

/** Pegel einer Frequenz in einem Ausschnitt (Goertzel-artige Projektion), dB. */
inline float toneDb (const std::vector<float>& x, int start, int end, double frequency, double sampleRate)
{
    double re = 0.0, im = 0.0;
    for (int i = start; i < end; ++i)
    {
        const double phase = 2.0 * pi * frequency * i / sampleRate;
        re += x[static_cast<size_t> (i)] * std::cos (phase);
        im += x[static_cast<size_t> (i)] * std::sin (phase);
    }
    const double amplitude = 2.0 * std::sqrt (re * re + im * im) / std::max (end - start, 1);
    return 20.0f * static_cast<float> (std::log10 (amplitude + 1.0e-20));
}

/** Bandpass zweiter Ordnung (RBJ, konstante Spitzenverstärkung 0 dB). */
inline std::vector<float> bandpass (const std::vector<float>& x, double frequency, double q, double sampleRate)
{
    const double w0 = 2.0 * pi * frequency / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;

    Biquad f;
    f.b0 = alpha / a0;
    f.b1 = 0.0;
    f.b2 = -alpha / a0;
    f.a1 = -2.0 * std::cos (w0) / a0;
    f.a2 = (1.0 - alpha) / a0;

    std::vector<float> out (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        out[i] = f.process (x[i]);
    return out;
}

/** Verarbeitet ein Mono-Signal blockweise mit einem Prozessor, der
    process (float* const*, int, int) anbietet.
*/
template <typename Processor>
void run (Processor& p, std::vector<float>& signal, int blockSize = 480)
{
    for (int start = 0; start < static_cast<int> (signal.size()); start += blockSize)
    {
        float* ptr = signal.data() + start;
        p.process (&ptr, 1, std::min (blockSize, static_cast<int> (signal.size()) - start));
    }
}

} // namespace vocaleqq::test
