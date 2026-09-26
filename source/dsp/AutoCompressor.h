#pragma once

#include <array>
#include <vector>

#include "Filters.h"

namespace vocaleqq
{

/** Kompressor, der seine Schwelle und Ratio aus der Dynamik der Stimme
    ableitet — ein Regler („Amount“) statt fünf.

    Portiert aus vocaleq/compressor.py. Detektor: RMS über 20 ms aus dem
    Spitzenwert beider Kanäle (Stereo-gekoppelt). Aus seiner Verteilung
    (10., 50. und 90. Perzentil in dBFS, nur Pegel über −50 dBFS):

      Schwelle = p50 − amount · 0,3 · (p90 − p50)
      Ratio    = clamp(2 + (p90 − p10) / 5, 2, 4), mit amount auf 1..Ratio skaliert
      Attack 10 ms, Release 150 ms, Soft-Knee 6 dB

    Das Vorbild rechnet die Perzentile einmal über die ganze Datei; hier
    schätzen drei QuantileTracker sie laufend. Nach einigen Sekunden Gesang
    stehen sie, und sie folgen, wenn der Sänger lauter oder leiser wird.

    Das Makeup des Vorbilds (RMS-Abgleich) fehlt hier: Die Kette gleicht am
    Ende ohnehin die Lautheit auf den Eingang an (AutoGain), so wie
    run_chain in Python das auch tut.

    LATENZ: 10 ms (halbes Detektorfenster). Das Vorbild mittelt zentriert
    (np.convolve, mode "same"); die Verzögerung stellt das nach.
*/
class AutoCompressor
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
    void setAmount (float newAmount) noexcept       { amount = newAmount; }

    int getLatencySamples() const noexcept { return window / 2; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    float getCurrentReductionDb() const noexcept { return reductionDb; }
    float getThresholdDb() const noexcept        { return thresholdDb; }
    float getRatio() const noexcept              { return ratio; }

private:
    void updateSettings() noexcept;

    double sampleRate = 44100.0;
    int window = 882;
    bool enabled = true;
    float amount = 0.7f;

    std::vector<double> squares;
    double sumSquares = 0.0;
    int squarePos = 0, recomputeCounter = 0;

    QuantileTracker p10, p50, p90;
    int statsCounter = 0;
    static constexpr int statsInterval = 32;

    float thresholdDb = -24.0f, ratio = 2.0f;
    float alphaAttack = 0.0f, alphaRelease = 0.0f;
    float reductionDb = 0.0f;

    static constexpr int maxChannels = 2;
    std::array<DelayLine, maxChannels> delays;
};

} // namespace vocaleqq
