#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "DspCommon.h"

namespace vocaleqq
{

/** Biquad in transponierter Direktform II. */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    float process (float x) noexcept
    {
        const double in = x;
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return static_cast<float> (out);
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    /** Tief- oder Hochpass zweiter Ordnung (RBJ-Kochbuch). */
    void setPass (bool highPass, double frequency, double q, double sampleRate) noexcept
    {
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosW = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        if (highPass)
        {
            b0 = (1.0 + cosW) / 2.0 / a0;
            b1 = -(1.0 + cosW) / a0;
        }
        else
        {
            b0 = (1.0 - cosW) / 2.0 / a0;
            b1 = (1.0 - cosW) / a0;
        }

        b2 = b0;
        a1 = -2.0 * cosW / a0;
        a2 = (1.0 - alpha) / a0;
    }
};

/** Linkwitz-Riley-Weiche 8. Ordnung: je Band ein Butterworth 4. Ordnung,
    zweimal hintereinander.

    Das Python-Vorbild trennt mit Butterworth 4. Ordnung vorwärts und
    rückwärts (filtfilt). Das ergibt je Band denselben Betragsgang wie hier —
    nur ohne Vorausschau, die ein Plugin nicht hat. Tief plus Hoch ergibt
    einen Allpass: Der Betrag bleibt flach, nur die Phase dreht sich um die
    Trennfrequenz. Deshalb läuft derselbe Allpass auch im trockenen Zweig
    (siehe VocalChain), sonst kämmte der Mix-Regler um 4 kHz.
*/
class Crossover
{
public:
    void prepare (double frequency, double sampleRate)
    {
        // Butterworth 4. Ordnung = zwei Biquads mit diesen Güten.
        constexpr double q[] { 0.54119610014619701, 1.3065629648763764 };

        for (int stage = 0; stage < 4; ++stage)
        {
            lows[static_cast<size_t> (stage)].setPass (false, frequency, q[stage % 2], sampleRate);
            highs[static_cast<size_t> (stage)].setPass (true, frequency, q[stage % 2], sampleRate);
        }

        reset();
    }

    void reset() noexcept
    {
        for (auto& f : lows)  f.reset();
        for (auto& f : highs) f.reset();
    }

    void process (float x, float& low, float& high) noexcept
    {
        low = x;
        high = x;
        for (auto& f : lows)  low = f.process (low);
        for (auto& f : highs) high = f.process (high);
    }

private:
    std::array<Biquad, 4> lows, highs;
};

/** Verzögerung um eine feste Zahl Samples. */
class DelayLine
{
public:
    void prepare (int delaySamples)
    {
        buffer.assign (static_cast<size_t> (std::max (delaySamples, 0) + 1), 0.0f);
        delay = std::max (delaySamples, 0);
        pos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        pos = 0;
    }

    float process (float x) noexcept
    {
        if (delay == 0)
            return x;

        buffer[static_cast<size_t> (pos)] = x;
        pos = (pos + 1) % static_cast<int> (buffer.size());
        return buffer[static_cast<size_t> (pos)];
    }

    int getDelay() const noexcept { return delay; }

private:
    std::vector<float> buffer { 0.0f };
    int delay = 0;
    int pos = 0;
};

/** Schätzt ein Quantil eines Datenstroms, ohne ihn zu speichern.

    Das Python-Vorbild rechnet Perzentile über die ganze Datei (np.percentile).
    Ein Plugin kennt die Datei nicht, nur das, was bisher durchlief. Dieser
    Schätzer wandert bei jedem Wert ein kleines Stück: nach oben, wenn der
    Wert darüber liegt, sonst nach unten — im Verhältnis p zu (1 − p). Im
    Gleichgewicht liegt dann genau der Anteil p der Werte darunter.
*/
class QuantileTracker
{
public:
    void reset (float initial) noexcept { value = initial; }

    void update (float x, float quantile, float step) noexcept
    {
        if (x > value)
            value += step * quantile;
        else
            value -= step * (1.0f - quantile);
    }

    float get() const noexcept { return value; }

private:
    float value = 0.0f;
};

} // namespace vocaleqq
