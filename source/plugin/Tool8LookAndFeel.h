#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vocaleqq
{

/** Die Farben des tooL8-Designs.

    Dieselben Werte wie theme.py der Python-Fassung (ursprünglich aus
    OffMusic/server/static/css/variables.css): dunkel #121212, Akzent
    #3B8ED0. Keyy soll dieselbe Palette bekommen — deshalb hängt hier
    nichts am Vocal EQQ, die Datei lässt sich so übernehmen.
*/
namespace Tool8Colours
{
    inline const juce::Colour bg         { 0xff121212 };
    inline const juce::Colour surface    { 0xff1c1c1c };
    inline const juce::Colour surfaceHi  { 0xff2a2a2a };
    inline const juce::Colour border     { 0xff333333 };
    inline const juce::Colour text       { 0xffffffff };
    inline const juce::Colour muted      { 0xff808080 };
    inline const juce::Colour accent     { 0xff3b8ed0 };
    inline const juce::Colour accentHi   { 0xff4da3e0 };
    inline const juce::Colour accentDeep { 0xff1e5f8a };
    inline const juce::Colour boost      { 0xff2ecc71 };
    inline const juce::Colour cut        { 0xffec4899 };
}

/** Schriftgrößen — jede Größe im Fenster stammt aus dieser Tabelle, wie die
    Typo-Skala in theme.py.
*/
namespace Tool8Fonts
{
    juce::Font title();     ///< Produktname
    juce::Font section();   ///< Abschnitts-Überschrift in Versalien
    juce::Font stage();     ///< Stufenname
    juce::Font label();     ///< Regler-Beschriftung
    juce::Font value();     ///< Zahlenwert
    juce::Font small();     ///< Statuszeilen
    juce::Font tiny();      ///< Achsen im Spektrum
}

/** Flache Bedienelemente im tooL8-Stil: Pillen-Schalter, Schiebe-Regler mit
    gefüllter Spur, abgerundete Knöpfe. Nachgebaut nach widgets.py der
    Python-Fassung, damit beide Werkzeuge gleich aussehen.
*/
class Tool8LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Tool8LookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    /** Pillen-Schalter, auch von außen benutzbar (Stufen-Köpfe). */
    static void drawPill (juce::Graphics&, juce::Rectangle<float> bounds, bool on, bool hover);
};

} // namespace vocaleqq
