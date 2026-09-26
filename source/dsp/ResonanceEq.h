#pragma once

#include <array>
#include <atomic>
#include <complex>
#include <mutex>
#include <vector>

#include "Fft.h"

namespace vocaleqq
{

/** Dynamischer Resonanz-EQ (Soothe-artig) plus feste Tonbalance.

    Portiert aus vocaleq/processor.py (apply_dynamic_eq) und equalizer.py.
    Je Rahmen (4096 Punkte, Sprung 1024 bei 44,1 kHz):

      1. Betragsspektrum der Mono-Summe in dB.
      2. Zwei Glättungen über Bruchteil-Oktaven: Detail (1/6) zeigt einzelne
         Resonanzspitzen, Basis (1) die glatte Tonbalance.
      3. Überschuss = Detail − Basis. Was 5 dB übersteigt, wird im Verhältnis
         `strength` abgesenkt, höchstens 6 dB, nur zwischen 150 Hz und 16 kHz.
      4. Dazu die feste Tonbalance: Hochpass 12 dB/Oktave unter 100 Hz und ein
         weiches Low-Shelf um 250 Hz (Regler „Low Cut“).
      5. Attack 8 ms / Release 80 ms je Bin, dann Betrag skalieren, Phase
         behalten, zurücktransformieren.

    Das ist dieselbe Rechnung wie im Vorbild — dort über die ganze Datei auf
    einmal, hier Rahmen für Rahmen im Overlap-Add. Die Verstärkung ist
    reell und je Bin symmetrisch, der EQ bleibt also nullphasig.

    DER STATISCHE MODUS FEHLT BEWUSST
    Er braucht das Langzeitspektrum der ganzen Aufnahme, das ein Plugin nie
    kennt. In der Python-Oberfläche war ohnehin „Dynamic“ die Vorgabe und
    wurde praktisch ausschließlich benutzt.

    STEREO
    Erkannt wird auf der Mono-Summe, die Verstärkung gilt für beide Kanäle
    (erhält das Stereobild, wie im Vorbild). Beide Kanäle laufen gemeinsam
    durch eine komplexe FFT (links reell, rechts imaginär): Weil die
    Verstärkung reell und symmetrisch ist, trennen sie sich bei der
    Rücktransformation wieder sauber.

    LATENZ: eine Rahmenlänge, 4096 Samples bei 44,1 kHz (93 ms).
*/
class ResonanceEq
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setEnabled (bool shouldBeEnabled) noexcept  { enabled = shouldBeEnabled; }
    /** 0..1, skaliert nur die Resonanz-Absenkung. */
    void setStrength (float amount) noexcept         { strength = amount; }
    /** Absenkung des Low-Shelfs in dB, als positive Zahl (0..8). */
    void setLowCutDb (float db) noexcept             { lowCutDb = db; }

    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept        { return fftSize; }
    double getSampleRate() const noexcept  { return sampleRate; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Für die Anzeige: Eingangsspektrum (1/6 Oktave geglättet, dB), die
        aktuell wirksame Korrektur (dB) und darin der feste Anteil aus
        Hochpass und Low-Shelf (dB), je Bin. Aus dem Nachrichten-Thread
        aufrufen; der Audio-Thread wartet nie darauf.

        @returns false, solange noch kein Rahmen gerechnet wurde
    */
    bool copyDisplay (std::vector<float>& spectrumDb, std::vector<float>& correctionDb, std::vector<float>& tonalDbOut);

    /** Zählt die gerechneten Rahmen. Steht er still, läuft kein Audio —
        die Oberfläche blendet das Spektrum dann aus, statt ein altes
        stehen zu lassen.
    */
    unsigned getFrameCount() const noexcept { return frameCount.load (std::memory_order_relaxed); }

private:
    void processFrame() noexcept;
    void updateTonalCurve() noexcept;

    double sampleRate = 44100.0;
    int fftSize = 4096, hop = 1024, numBins = 2049;
    int activeChannels = 1;

    bool enabled = true;
    float strength = 0.7f;
    float lowCutDb = 3.0f, tonalLowCutDb = -1.0f;

    Fft fft;
    std::vector<float> window, binFrequencies, magnitudeDb, detail, baseline, cumulative;
    std::vector<int> detailLo, detailHi, baselineLo, baselineHi;
    std::vector<float> tonalDb, gainDb;
    std::vector<std::complex<float>> spectrum;
    float alphaAttack = 0.0f, alphaRelease = 0.0f, synthesisScale = 1.0f;

    static constexpr int maxChannels = 2;
    std::array<std::vector<float>, maxChannels> inputRing, outputRing;
    int pos = 0, hopCounter = 0;

    std::mutex displayLock;
    std::vector<float> displaySpectrum, displayCorrection, displayTonal;
    bool displayValid = false;
    std::atomic<unsigned> frameCount { 0 };
};

} // namespace vocaleqq
