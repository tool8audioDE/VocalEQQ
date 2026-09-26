#include "PluginProcessor.h"

#include "ParameterIDs.h"
#include "PluginEditor.h"

namespace vocaleqq
{

namespace
{
    juce::String percent (float value, int)  { return juce::String (juce::roundToInt (value)) + " %"; }
    juce::String decibels (float value, int) { return juce::String (value, 1) + " dB"; }

    // Low Cut ist eine Absenkung: als negative Zahl anzeigen, wie in der
    // Python-Oberfläche („−3.0 dB“).
    juce::String cut (float value, int)
    {
        return (value > 0.0f ? juce::String::fromUTF8 ("−") : juce::String()) + juce::String (value, 1) + " dB";
    }
}

VocalEqqAudioProcessor::VocalEqqAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    deEssOn          = apvts.getRawParameterValue (ParamID::deEssOn);
    deEssReduction   = apvts.getRawParameterValue (ParamID::deEssReduction);
    deEssSensitivity = apvts.getRawParameterValue (ParamID::deEssSensitivity);
    eqOn             = apvts.getRawParameterValue (ParamID::eqOn);
    eqStrength       = apvts.getRawParameterValue (ParamID::eqStrength);
    eqLowCut         = apvts.getRawParameterValue (ParamID::eqLowCut);
    compOn           = apvts.getRawParameterValue (ParamID::compOn);
    compAmount       = apvts.getRawParameterValue (ParamID::compAmount);
    mix              = apvts.getRawParameterValue (ParamID::mix);
    autoGain         = apvts.getRawParameterValue (ParamID::autoGain);
    bypass           = apvts.getRawParameterValue (ParamID::bypass);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalEqqAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto floatParam = [&layout] (const char* id, const char* name, float lo, float hi, float step,
                                 float def, String (*format) (float, int))
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> { lo, hi, step }, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (format)));
    };

    auto boolParam = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // Bereiche und Vorgaben wie die Regler der Python-Oberfläche.
    boolParam  (ParamID::deEssOn, "De-Esser On", true);
    floatParam (ParamID::deEssReduction, "De-Ess Reduction", 0.0f, 12.0f, 0.1f, 4.5f, decibels);
    floatParam (ParamID::deEssSensitivity, "De-Ess Sensitivity", 0.0f, 100.0f, 1.0f, 50.0f, percent);

    boolParam  (ParamID::eqOn, "Resonance EQ On", true);
    floatParam (ParamID::eqStrength, "EQ Strength", 0.0f, 100.0f, 1.0f, 70.0f, percent);
    floatParam (ParamID::eqLowCut, "Low Cut", 0.0f, 8.0f, 0.1f, 3.0f, cut);

    boolParam  (ParamID::compOn, "Compressor On", true);
    floatParam (ParamID::compAmount, "Comp Amount", 0.0f, 100.0f, 1.0f, 70.0f, percent);

    floatParam (ParamID::mix, "Mix", 0.0f, 100.0f, 1.0f, 100.0f, percent);
    boolParam  (ParamID::autoGain, "Auto Gain", true);
    boolParam  (ParamID::bypass, "Bypass", false);

    return layout;
}

void VocalEqqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (chain.getLatencySamples());
}

bool VocalEqqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void VocalEqqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ChainSettings s;
    s.deEssEnabled     = deEssOn->load() > 0.5f;
    s.deEssReductionDb = deEssReduction->load();
    s.deEssSensitivity = deEssSensitivity->load() / 100.0f;
    s.eqEnabled        = eqOn->load() > 0.5f;
    s.eqStrength       = eqStrength->load() / 100.0f;
    s.eqLowCutDb       = eqLowCut->load();
    s.compEnabled      = compOn->load() > 0.5f;
    s.compAmount       = compAmount->load() / 100.0f;
    s.mix              = mix->load() / 100.0f;
    s.autoGain         = autoGain->load() > 0.5f;
    s.bypass           = bypass->load() > 0.5f;

    chain.setSettings (s);
    chain.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorParameter* VocalEqqAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamID::bypass);
}

juce::AudioProcessorEditor* VocalEqqAudioProcessor::createEditor()
{
    return new VocalEqqAudioProcessorEditor (*this);
}

void VocalEqqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalEqqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace vocaleqq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vocaleqq::VocalEqqAudioProcessor();
}
