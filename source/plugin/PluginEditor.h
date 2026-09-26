#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "Tool8LookAndFeel.h"

namespace vocaleqq
{

/** Beschriftung links, Wert rechts, darunter der Regler — SliderRow aus
    widgets.py der Python-Fassung.
*/
class SliderRow : public juce::Component
{
public:
    SliderRow (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshValue();

    juce::RangedAudioParameter* parameter = nullptr;
    juce::String name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Eine Stufe der Kette: Karte mit Kopfzeile (Schalter, Nummer, Name,
    Messwert) und ihren Reglern. Ausgeschaltet wird der Inhalt gedämpft,
    bleibt aber einstellbar.
*/
class StageCard : public juce::Component
{
public:
    StageCard (juce::AudioProcessorValueTreeState&, const char* onParamId, int number, const juce::String& title);

    SliderRow& addRow (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name);

    void setMeterText (const juce::String& text);
    void setFootnote (const juce::String& text);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int number;
    juce::String title, meterText, footnote;
    juce::ToggleButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::vector<std::unique_ptr<SliderRow>> rows;
};

/** Oberfläche im tooL8-Design: dunkel, Blau-Akzent, Karten.

    Oben das Live-Spektrum mit der EQ-Korrektur, darunter die drei Stufen
    nebeneinander in der Reihenfolge der Kette, unten was die ganze Kette
    betrifft. Die Oberfläche hält keinen eigenen Zustand: Alles hängt an
    Parametern, die Anzeigen liest ein Timer aus dem Prozessor.
*/
class VocalEqqAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit VocalEqqAudioProcessorEditor (VocalEqqAudioProcessor&);
    ~VocalEqqAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VocalEqqAudioProcessor& owner;
    Tool8LookAndFeel lookAndFeel;

    SpectrumView spectrum;
    StageCard deEssCard, eqCard, compCard;
    SliderRow mixRow;
    juce::ToggleButton autoGainButton, bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment, bypassAttachment;

    std::vector<float> spectrumDb, correctionDb, tonalDb;
    unsigned lastFrame = 0;
    int idleTicks = 0;

    juce::Rectangle<int> footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEqqAudioProcessorEditor)
};

} // namespace vocaleqq
