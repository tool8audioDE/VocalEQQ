/** vocaleqq-snapshot — rendert die Oberfläche als PNG, ohne Fenster.

    Für Screenshots (Website, README) und um das Layout zu prüfen, ohne das
    Plugin in einer DAW zu öffnen. Speist vorher etwas Rauschen mit einer
    Resonanz durch die Kette, damit Spektrum und Anzeigen etwas zeigen.

      vocaleqq-snapshot ausgabe.png [--bypass]
*/

#include <random>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/ParameterIDs.h"
#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    const bool bypass = argc > 2 && juce::String (argv[2]) == "--bypass";

    vocaleqq::VocalEqqAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    if (bypass)
        processor.apvts.getParameter (vocaleqq::ParamID::bypass)->setValueNotifyingHost (1.0f);

    // Stimmähnliches Signal: rosa gefärbtes Rauschen, Grundton, Resonanz bei 2,8 kHz.
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    float lp = 0.0f, res1 = 0.0f, res2 = 0.0f;
    double phase = 0.0;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    for (int block = 0; block < 900; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float n = dist (rng);
            lp = 0.97f * lp + 0.03f * n;
            // einfacher Resonator bei 2,8 kHz
            const float w = 2.0f * 3.14159265f * 2800.0f / 44100.0f;
            const float r = 0.995f;
            const float y = n + 2.0f * r * std::cos (w) * res1 - r * r * res2;
            res2 = res1; res1 = y;
            phase += 2.0 * 3.14159265 * 180.0 / 44100.0;
            const float s = 0.25f * static_cast<float> (std::sin (phase)) + 0.6f * lp + 0.004f * y + 0.02f * n;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }
        processor.processBlock (buffer, midi);

        // Den Timer der Oberfläche von Hand antreiben: ohne Nachrichtenschleife.
        if (block % 10 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    }

    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        return 1;

    std::printf ("%s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
