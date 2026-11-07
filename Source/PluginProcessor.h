#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "BandCarver.h"

// Goes on the BEAT track. The vocal feeds the sidechain; the output is the beat
// with space carved out wherever the vocal is.
class VocalSitterProcessor : public juce::AudioProcessor
{
public:
    VocalSitterProcessor();
    ~VocalSitterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    const CarveDisplayData& getDisplay() const { return carver.display; }

    // Values read by the editor's meters
    std::atomic<float> beatLevelDb { -100.0f }, vocalLevelDb { -100.0f }, outputLevelDb { -100.0f }, autoGainDb { 0.0f };

private:
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    double sr = 44100.0;
    BandCarver carver;
    std::vector<float> vocalKey;
    juce::SmoothedValue<float> outGain, autoGain;
    float refPower = 0.0f, outPower = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSitterProcessor)
};
