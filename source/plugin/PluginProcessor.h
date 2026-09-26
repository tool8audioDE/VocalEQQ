#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/VocalChain.h"

namespace vocaleqq
{

/** Vocal EQQ: Channel-Strip für Gesang — De-Ess → Resonanz-EQ → Kompressor.

    Die ganze Signalverarbeitung sitzt in VocalChain (ohne JUCE, getestet).
    Der Prozessor liest nur die Parameter, reicht sie weiter und meldet der
    DAW die Latenz, damit sie die Spur passend vorzieht.
*/
class VocalEqqAudioProcessor : public juce::AudioProcessor
{
public:
    VocalEqqAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Der Bypass-Schalter der DAW landet auf unserem eigenen Parameter —
        der blendet über und hält die Latenz, statt hart umzuschalten.
    */
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    VocalChain& getChain() noexcept { return chain; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    VocalChain chain;

    std::atomic<float>* deEssOn = nullptr;
    std::atomic<float>* deEssReduction = nullptr;
    std::atomic<float>* deEssSensitivity = nullptr;
    std::atomic<float>* eqOn = nullptr;
    std::atomic<float>* eqStrength = nullptr;
    std::atomic<float>* eqLowCut = nullptr;
    std::atomic<float>* compOn = nullptr;
    std::atomic<float>* compAmount = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* autoGain = nullptr;
    std::atomic<float>* bypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEqqAudioProcessor)
};

} // namespace vocaleqq
