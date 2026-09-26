#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace vocaleqq
{

/** Spektrum des Eingangs und die gerade wirksame EQ-Korrektur.

    Wie in der Python-Oberfläche: oben das Spektrum als gefüllte Fläche,
    unten die Absenkungen in Pink. Anders als dort läuft es live mit — man
    sieht, wie eine Resonanz nur dann gezähmt wird, wenn sie gerade
    überschießt.
*/
class SpectrumView : public juce::Component
{
public:
    SpectrumView();

    /** @param spectrumDb   Betragsspektrum je FFT-Bin (unnormiert, dB)
        @param correctionDb Korrektur je Bin (dB, ≤ 0 = Absenkung)
    */
    void update (const std::vector<float>& spectrumDb, const std::vector<float>& correctionDb,
                 double sampleRate, int fftSize);

    /** Nichts mehr anzeigen (kein Signal oder Plugin gerade vorbereitet). */
    void clear();

    void paint (juce::Graphics&) override;

private:
    float xForFrequency (float frequency, float width) const noexcept;

    std::vector<float> spectrum, correction;   // je Pixelspalte, geglättet
    std::vector<float> columnFrequencies;
    bool hasData = false;
    int lastWidth = 0;
};

} // namespace vocaleqq
