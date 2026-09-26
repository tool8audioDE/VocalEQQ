#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace vocaleqq
{

inline constexpr double pi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float gain) noexcept { return 20.0f * std::log10 (gain + 1.0e-9f); }

/** Glättungsbeiwert eines Einpolfilters für eine Zeitkonstante, gemessen in
    Aufrufen pro Sekunde — dieselbe Formel wie im Python-Vorbild
    (exp(-1 / (t * rate))), damit Attack und Release gleich wirken.
*/
inline float smoothingCoefficient (double seconds, double callsPerSecond) noexcept
{
    return static_cast<float> (std::exp (-1.0 / std::max (seconds * callsPerSecond, 1.0e-6)));
}

/** Größe einer Analyse, die bei 44,1 und 48 kHz `baseSize` Punkte hat.

    Bei höheren Raten wächst sie mit, damit Frequenzauflösung und Zeitraster
    ungefähr gleich bleiben — sonst hätte die 1/6-Oktave bei 96 kHz im
    Tiefton nur halb so viele Bins und das Plugin klänge je nach
    Projekt-Samplerate anders.
*/
inline int scaledFftSize (int baseSize, double sampleRate) noexcept
{
    int size = baseSize;
    for (double rate = 64000.0; sampleRate > rate && size < baseSize * 8; rate *= 2.0)
        size *= 2;
    return size;
}

/** Periodisches Hann-Fenster, wie scipy.signal.get_window ("hann", n) —
    so benutzt es librosa.stft, das Python-Vorbild.
*/
inline std::vector<float> periodicHann (int size)
{
    std::vector<float> window (static_cast<size_t> (size));
    for (int i = 0; i < size; ++i)
        window[static_cast<size_t> (i)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * i / size));
    return window;
}

} // namespace vocaleqq
