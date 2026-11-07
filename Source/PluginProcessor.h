#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SpectralCarver.h"

// The 7-band graphic EQ for the vocal
struct VocalEq
{
    static constexpr int numBands = 7;
    static constexpr std::array<float, numBands> frequencies { 63.0f, 160.0f, 400.0f, 1000.0f, 2500.0f, 6300.0f, 16000.0f };
    static std::array<float, 6> coefficients (int band, float gainDb, double sampleRate);
};

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
    double getCurrentSampleRate() const { return sr; }

    // Values read by the editor's meters
    std::atomic<float> vocalLevelDb { -100.0f }, beatLevelDb { -100.0f }, outputLevelDb { -100.0f }, autoGainDb { 0.0f };

private:
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateEq();

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    double sr = 44100.0;
    std::array<Filter, VocalEq::numBands> eq;
    std::array<float, VocalEq::numBands> lastEqGains {};
    SpectralCarver carver;

    juce::AudioBuffer<float> beatBuffer, vocalDelay;
    std::vector<float> vocalKey;
    int delayPos = 0;

    juce::SmoothedValue<float> vocalGain, beatGain, outGain, autoGain;
    float refPower = 0.0f, outPower = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSitterProcessor)
};
