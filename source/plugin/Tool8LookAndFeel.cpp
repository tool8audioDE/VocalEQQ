#include "Tool8LookAndFeel.h"

namespace vocaleqq
{

namespace
{
    // Segoe UI wie in der Python-Fassung. Fehlt sie (anderes System), nimmt
    // JUCE die Standardschrift.
    juce::Font make (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Segoe UI", height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

namespace Tool8Fonts
{
    juce::Font title()   { return make (22.0f, true); }
    juce::Font section() { return make (11.0f, true).withExtraKerningFactor (0.08f); }
    juce::Font stage()   { return make (14.5f, true); }
    juce::Font label()   { return make (13.0f); }
    juce::Font value()   { return make (13.0f, true); }
    juce::Font small()   { return make (11.5f); }
    juce::Font tiny()    { return make (10.0f); }
}

Tool8LookAndFeel::Tool8LookAndFeel()
{
    using namespace Tool8Colours;

    setDefaultSansSerifTypefaceName ("Segoe UI");

    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::DocumentWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, surfaceHi);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::ToggleButton::textColourId, muted);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, surfaceHi);
    setColour (juce::PopupMenu::backgroundColourId, surface);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);

    // Standalone-Fenster: Einstellungsdialog und Titelleiste im selben Ton.
    setColour (juce::ComboBox::backgroundColourId, surfaceHi);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ListBox::backgroundColourId, surface);
    setColour (juce::AlertWindow::backgroundColourId, surface);
    setColour (juce::AlertWindow::textColourId, text);
}

void Tool8LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float, juce::Slider::SliderStyle,
                                         juce::Slider& slider)
{
    using namespace Tool8Colours;

    const bool enabled = slider.isEnabled();
    const bool hover = slider.isMouseOverOrDragging() && enabled;

    const float trackHeight = 4.0f;
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const auto track = juce::Rectangle<float> (static_cast<float> (x), cy - trackHeight * 0.5f,
                                               static_cast<float> (width), trackHeight);

    g.setColour (surfaceHi);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    auto filled = track.withRight (sliderPos);
    g.setColour (enabled ? (hover ? accentHi : accent) : border);
    g.fillRoundedRectangle (filled, trackHeight * 0.5f);

    const float thumb = hover ? 14.0f : 12.0f;
    g.setColour (enabled ? text : muted);
    g.fillEllipse (juce::Rectangle<float> (thumb, thumb).withCentre ({ sliderPos, cy }));
}

void Tool8LookAndFeel::drawPill (juce::Graphics& g, juce::Rectangle<float> bounds, bool on, bool hover)
{
    using namespace Tool8Colours;

    const auto pill = bounds.withSizeKeepingCentre (40.0f, 22.0f);

    g.setColour (on ? (hover ? accentHi : accent) : (hover ? border : surfaceHi));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

    const float knob = pill.getHeight() - 8.0f;
    const float kx = on ? pill.getRight() - 4.0f - knob : pill.getX() + 4.0f;
    g.setColour (on ? text : muted);
    g.fillEllipse (kx, pill.getY() + 4.0f, knob, knob);
}

void Tool8LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    drawPill (g, bounds.withWidth (44.0f), on, highlighted && button.isEnabled());

    if (button.getButtonText().isNotEmpty())
    {
        g.setColour (on ? Tool8Colours::text : Tool8Colours::muted);
        g.setFont (Tool8Fonts::label());
        g.drawText (button.getButtonText(), bounds.withTrimmedLeft (52.0f), juce::Justification::centredLeft);
    }
}

void Tool8LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    using namespace Tool8Colours;

    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    juce::Colour fill = on ? accent : surfaceHi;
    if (highlighted || down)
        fill = on ? accentHi : border;
    if (! button.isEnabled())
        fill = surfaceHi.withAlpha (0.5f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);
}

juce::Font Tool8LookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return Tool8Fonts::label();
}

} // namespace vocaleqq
