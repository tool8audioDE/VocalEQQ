#include "PluginEditor.h"

#include "ParameterIDs.h"

namespace vocaleqq
{

namespace
{
    juce::String ui (const char* text) { return juce::String::fromUTF8 (text); }

    juce::String formatDb (float db)
    {
        // Minuszeichen statt Bindestrich, wie in der Python-Oberfläche.
        if (std::abs (db) < 0.05f)
            return "0.0 dB";
        return (db < 0.0f ? ui ("−") : juce::String ("+")) + juce::String (std::abs (db), 1) + " dB";
    }

    constexpr int editorWidth = 860, editorHeight = 580;
    constexpr int pad = 18;
}

// ---------------------------------------------------------------------------
// SliderRow
// ---------------------------------------------------------------------------

SliderRow::SliderRow (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& nameToUse)
    : parameter (apvts.getParameter (paramId)), name (nameToUse)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                                ? parameter->convertFrom0to1 (parameter->getDefaultValue())
                                                : 0.0);
    slider.onValueChange = [this] { refreshValue(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void SliderRow::refreshValue()
{
    repaint (getLocalBounds().removeFromTop (18));
}

void SliderRow::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (18);
    slider.setBounds (area.reduced (0, 1));
}

void SliderRow::paint (juce::Graphics& g)
{
    auto head = getLocalBounds().removeFromTop (18).toFloat();

    g.setFont (Tool8Fonts::label());
    g.setColour (Tool8Colours::muted);
    g.drawText (name, head, juce::Justification::centredLeft);

    g.setFont (Tool8Fonts::value());
    g.setColour (Tool8Colours::accentHi);
    g.drawText (parameter != nullptr ? parameter->getCurrentValueAsText() : juce::String(), head,
                juce::Justification::centredRight);
}

// ---------------------------------------------------------------------------
// StageCard
// ---------------------------------------------------------------------------

StageCard::StageCard (juce::AudioProcessorValueTreeState& apvts, const char* onParamId, int numberToUse,
                      const juce::String& titleToUse)
    : number (numberToUse), title (titleToUse)
{
    onButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    onButton.setTooltip ("Switch " + title + " on or off");
    onButton.onStateChange = [this]
    {
        // Ausgeschaltet: Regler gedämpft, aber weiter bedienbar — man kann
        // eine Stufe vorbereiten, bevor man sie zuschaltet.
        for (auto& row : rows)
            row->setAlpha (onButton.getToggleState() ? 1.0f : 0.45f);
        repaint();
    };
    addAndMakeVisible (onButton);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, onParamId, onButton);
}

SliderRow& StageCard::addRow (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& name)
{
    rows.push_back (std::make_unique<SliderRow> (apvts, paramId, name));
    addAndMakeVisible (*rows.back());
    rows.back()->setAlpha (onButton.getToggleState() ? 1.0f : 0.45f);
    return *rows.back();
}

void StageCard::setMeterText (const juce::String& text)
{
    if (text != meterText)
    {
        meterText = text;
        repaint (getLocalBounds().removeFromTop (48));
    }
}

void StageCard::setFootnote (const juce::String& text)
{
    if (text != footnote)
    {
        footnote = text;
        repaint (getLocalBounds().removeFromBottom (30));
    }
}

void StageCard::resized()
{
    auto area = getLocalBounds().reduced (14, 12);

    auto header = area.removeFromTop (26);
    onButton.setBounds (header.removeFromLeft (44));

    area.removeFromTop (12);
    for (auto& row : rows)
    {
        row->setBounds (area.removeFromTop (42));
        area.removeFromTop (8);
    }
}

void StageCard::paint (juce::Graphics& g)
{
    using namespace Tool8Colours;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (surface);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    const bool on = onButton.getToggleState();
    auto header = getLocalBounds().reduced (14, 12).removeFromTop (26).toFloat();
    header.removeFromLeft (54.0f);

    // Nummer der Stufe in der Kette, wie in der Python-Oberfläche.
    const auto badge = header.removeFromLeft (20.0f).withSizeKeepingCentre (20.0f, 20.0f);
    g.setColour (on ? accentDeep : surfaceHi);
    g.fillEllipse (badge);
    g.setColour (on ? text : muted);
    g.setFont (Tool8Fonts::small().boldened());
    g.drawText (juce::String (number), badge, juce::Justification::centred);

    header.removeFromLeft (8.0f);
    g.setFont (Tool8Fonts::stage());
    g.setColour (on ? text : muted);
    g.drawText (title, header, juce::Justification::centredLeft);

    g.setFont (Tool8Fonts::value());
    g.setColour (on ? cut : muted);
    g.drawText (on ? meterText : ui ("off"), header, juce::Justification::centredRight);

    if (footnote.isNotEmpty())
    {
        g.setFont (Tool8Fonts::small());
        g.setColour (muted);
        g.drawText (footnote, getLocalBounds().reduced (14, 10).removeFromBottom (16).toFloat(),
                    juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

VocalEqqAudioProcessorEditor::VocalEqqAudioProcessorEditor (VocalEqqAudioProcessor& processorToUse)
    : AudioProcessorEditor (processorToUse),
      owner (processorToUse),
      deEssCard (processorToUse.apvts, ParamID::deEssOn, 1, "De-Esser"),
      eqCard (processorToUse.apvts, ParamID::eqOn, 2, "Resonance EQ"),
      compCard (processorToUse.apvts, ParamID::compOn, 3, "Compressor"),
      mixRow (processorToUse.apvts, ParamID::mix, "Mix (Dry / Wet)")
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = owner.apvts;

    deEssCard.addRow (apvts, ParamID::deEssReduction, "Reduction");
    deEssCard.addRow (apvts, ParamID::deEssSensitivity, "Sensitivity");
    deEssCard.setFootnote (ui ("Tames S and T sounds above 4 kHz"));

    eqCard.addRow (apvts, ParamID::eqStrength, "Strength");
    eqCard.addRow (apvts, ParamID::eqLowCut, "Low Cut");
    eqCard.setFootnote (ui ("Dynamic · high-pass 100 Hz · shelf 250 Hz"));

    compCard.addRow (apvts, ParamID::compAmount, "Amount");

    for (auto* c : { &deEssCard, &eqCard, &compCard })
        addAndMakeVisible (c);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (mixRow);

    autoGainButton.setButtonText ("Auto Gain");
    autoGainButton.setTooltip ("Match the output loudness to the input, for a fair A/B");
    autoGainButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamID::autoGain, autoGainButton);

    bypassButton.setButtonText ("Bypass");
    bypassButton.setTooltip ("Compare with the original (same latency, no click)");
    bypassButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    bypassButton.onStateChange = [this] { repaint(); };
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamID::bypass, bypassButton);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

VocalEqqAudioProcessorEditor::~VocalEqqAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalEqqAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace Tool8Colours;

    g.fillAll (bg);

    // Kopfzeile: Produktname mit dem doppelten Q im Akzent, darunter die Kette.
    auto header = getLocalBounds().reduced (pad, 0).withHeight (64).withTrimmedTop (14);

    g.setFont (Tool8Fonts::title());
    const juce::String first = "Vocal ", second = "EQQ";
    const int firstWidth = juce::GlyphArrangement::getStringWidthInt (Tool8Fonts::title(), first);
    const int secondWidth = juce::GlyphArrangement::getStringWidthInt (Tool8Fonts::title(), second);
    g.setColour (text);
    g.drawText (first, header.getX(), header.getY(), firstWidth + 2, 28, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (second, header.getX() + firstWidth, header.getY(), secondWidth + 4, 28, juce::Justification::centredLeft);

    g.setFont (Tool8Fonts::small());
    g.setColour (muted);
    g.drawText (ui ("Vocal channel strip  ·  De-Ess → Resonance EQ → Compressor"),
                header.getX() + firstWidth + secondWidth + 16, header.getY() + 4, 420, 24,
                juce::Justification::centredLeft);

    // Hersteller rechts oben, wie ein Typenschild — in seiner eigenen
    // Schreibweise, deshalb nicht in Versalien.
    g.setFont (Tool8Fonts::stage());
    g.drawText ("tooL8", header.removeFromRight (60).withHeight (28), juce::Justification::centredRight);

    // Fußzeile als Karte.
    g.setColour (surface);
    g.fillRoundedRectangle (footerArea.toFloat(), 10.0f);
    g.setColour (border);
    g.drawRoundedRectangle (footerArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto footer = footerArea.reduced (16, 0);
    const double sampleRate = owner.getSampleRate() > 0.0 ? owner.getSampleRate() : 44100.0;
    const int latency = owner.getLatencySamples();

    g.setFont (Tool8Fonts::small());
    g.setColour (muted);
    g.drawText ("Latency " + juce::String (juce::roundToInt (1000.0 * latency / sampleRate)) + " ms  ·  "
                    + ui ("v") + JucePlugin_VersionString,
                footer.removeFromRight (170), juce::Justification::centredRight);

    // Auto-Gain-Wert neben seinem Schalter.
    if (autoGainButton.getToggleState())
    {
        g.setFont (Tool8Fonts::value());
        g.setColour (accentHi);
        g.drawText (formatDb (owner.getChain().getAutoGainDb()),
                    autoGainButton.getBounds().translated (autoGainButton.getWidth() + 4, 0).withWidth (70),
                    juce::Justification::centredLeft);
    }

    if (bypassButton.getToggleState())
    {
        // Im Bypass deutlich sichtbar: man hört gerade das Original.
        g.setColour (cut);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 12.0f, 2.0f);
    }
}

void VocalEqqAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (pad);

    // Auf der Höhe des Titels (paint: Oberkante 14, 28 hoch).
    auto header = area.removeFromTop (46);
    header.removeFromRight (80);
    bypassButton.setBounds (header.removeFromRight (110).withY (14).withHeight (28));

    footerArea = area.removeFromBottom (64);
    area.removeFromBottom (12);

    auto footer = footerArea.reduced (16, 0);
    mixRow.setBounds (footer.removeFromLeft (340).withSizeKeepingCentre (340, 42));
    footer.removeFromLeft (36);
    autoGainButton.setBounds (footer.removeFromLeft (132).withSizeKeepingCentre (132, 26));

    auto cards = area.removeFromBottom (196);
    area.removeFromBottom (12);

    const int gap = 12;
    const int cardWidth = (cards.getWidth() - 2 * gap) / 3;
    deEssCard.setBounds (cards.removeFromLeft (cardWidth));
    cards.removeFromLeft (gap);
    eqCard.setBounds (cards.removeFromLeft (cardWidth));
    cards.removeFromLeft (gap);
    compCard.setBounds (cards);

    spectrum.setBounds (area);
}

void VocalEqqAudioProcessorEditor::timerCallback()
{
    auto& chain = owner.getChain();
    auto& eq = chain.getEq();

    // Spektrum nur, wenn tatsächlich neue Rahmen gerechnet wurden; eine
    // Viertelsekunde ohne neuen Rahmen heißt: kein Audio, Anzeige aus.
    const unsigned frame = eq.getFrameCount();
    if (frame != lastFrame)
    {
        lastFrame = frame;
        idleTicks = 0;
        if (eq.copyDisplay (spectrumDb, correctionDb, tonalDb))
            spectrum.update (spectrumDb, correctionDb, eq.getSampleRate(), eq.getFftSize());
    }
    else if (++idleTicks == 8)
    {
        spectrum.clear();
    }

    deEssCard.setMeterText (formatDb (chain.getDeEssReductionDb()));
    compCard.setMeterText (formatDb (chain.getCompReductionDb()));
    compCard.setFootnote (ui ("Auto  ·  threshold ") + formatDb (chain.getCompThresholdDb())
                          + ui ("  ·  ratio ") + juce::String (chain.getCompRatio(), 1) + ":1");

    // Der EQ zeigt seine tiefste Resonanz-Absenkung — ohne den festen
    // Anteil aus Hochpass und Low-Shelf, der stünde sonst immer da.
    float deepest = 0.0f;
    if (! correctionDb.empty() && tonalDb.size() == correctionDb.size())
    {
        const double binHz = eq.getSampleRate() / eq.getFftSize();
        for (size_t k = static_cast<size_t> (150.0 / binHz); k < correctionDb.size() && static_cast<double> (k) * binHz <= 16000.0; ++k)
            deepest = std::min (deepest, correctionDb[k] - tonalDb[k]);
    }
    eqCard.setMeterText (formatDb (deepest));

    repaint (footerArea);
}

} // namespace vocaleqq
