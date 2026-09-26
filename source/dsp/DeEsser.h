#pragma once

#include <array>
#include <complex>
#include <vector>

#include "Fft.h"
#include "Filters.h"

namespace vocaleqq
{

/** De-Esser: erkennt Zischlaute und senkt nur dann das Band über 4 kHz ab.

    Portiert aus vocaleq/deess.py. Die Erkennung ist dieselbe: Je Rahmen
    (2048 Punkte, Sprung 256) vier Merkmale — Hochton-Anteil, Schwerpunkt,
    Nulldurchgänge, Hochton-Flux. Ein Rahmen gilt als Zischlaut, wenn der
    Hochton-Anteil über seiner Schwelle liegt und mindestens eines der
    drei anderen Merkmale zustimmt.

    UNTERSCHIED ZUM VORBILD
    Die Schwellen sind Perzentile. Python kennt die ganze Datei und rechnet
    sie einmal aus; hier schätzt je Merkmal ein QuantileTracker das Perzentil
    aus dem bisherigen Signal. Er lernt nur aus Rahmen über −50 dBFS — also
    aus denen, die überhaupt als Zischlaut in Frage kommen. Lange Stille vor
    dem Gesang verschiebt die Schwellen damit nicht.

    LATENZ
    Ein Rahmen beschreibt seine Mitte. Damit die Absenkung genau dort
    ankommt, wo das Merkmal gemessen wurde, läuft das Audiosignal um einen
    halben Rahmen plus einen Sprung verzögert (bei 44,1 kHz 1280 Samples).
*/
class DeEsser
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setEnabled (bool shouldBeEnabled) noexcept     { enabled = shouldBeEnabled; }
    void setReductionDb (float db) noexcept             { reductionDb = db; }
    /** 0..1 — Aggressivität des Vorbilds: verschiebt alle Perzentile um ±12. */
    void setSensitivity (float amount) noexcept         { sensitivity = amount; }

    int getLatencySamples() const noexcept { return frameSize / 2 + hop; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Momentane Absenkung des Hochtonbands in dB (≤ 0), für die Anzeige. */
    float getCurrentReductionDb() const noexcept { return currentReductionDb; }

private:
    void analyseFrame() noexcept;

    double sampleRate = 44100.0;
    int frameSize = 2048, hop = 256;

    bool enabled = true;
    float reductionDb = 4.5f;
    float sensitivity = 0.5f;

    Fft fft;
    std::vector<float> window, ring, frame, magnitudes, previousHigh, binFrequencies;
    std::vector<std::complex<float>> spectrum;
    int ringPos = 0, hopCounter = 0;
    int highFirstBin = 0, highLastBin = 0;

    QuantileTracker highRatioThreshold, centroidThreshold, zcrThreshold, fluxThreshold;
    float fluxPeak = 0.0f, fluxPeakDecay = 1.0f;

    // Maske (0..1) nach Attack/Release und die Rampe zwischen zwei Rahmen —
    // np.interp im Vorbild.
    float maskSmoothed = 0.0f, alphaAttack = 0.0f, alphaRelease = 0.0f;
    float rampValue = 0.0f, rampStep = 0.0f;
    float currentReductionDb = 0.0f;

    static constexpr int maxChannels = 2;
    std::array<Crossover, maxChannels> crossovers;
    std::array<DelayLine, maxChannels> delays;
};

} // namespace vocaleqq
