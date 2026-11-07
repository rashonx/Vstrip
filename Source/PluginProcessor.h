#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Splits the signal into low/high bands and turns down only the high band
// when sibilance ("s", "sh", "t" sounds) crosses the threshold.
class DeEsser
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (float frequencyHz, float thresholdDb);
    void process (juce::AudioBuffer<float>& buffer);
    float getReductionDb() const { return reductionDb.load(); }

private:
    juce::dsp::LinkwitzRileyFilter<float> crossover;
    std::vector<float> envelope;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f, threshold = -30.0f;
    std::atomic<float> reductionDb { 0.0f };
};

class VocalStripProcessor : public juce::AudioProcessor
{
public:
    VocalStripProcessor();
    ~VocalStripProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Values read by the editor's meters
    std::atomic<float> inputLevelDb { -100.0f }, outputLevelDb { -100.0f }, compReductionDb { 0.0f };
    float getDeEssReductionDb() const { return deEsser.getReductionDb(); }

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return param (id) > 0.5f; }
    void updateEq();
    void processDelay (juce::AudioBuffer<float>& buffer);

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    double sr = 44100.0;
    juce::dsp::Gain<float> inputGain, outputGain, makeupGain;
    Filter highPass, lowShelf, midPeak, airShelf;
    juce::dsp::NoiseGate<float> gate;
    DeEsser deEsser;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 192000 * 2 };
    juce::SmoothedValue<float> delayTimeSamples;
    juce::dsp::Reverb reverb;
    bool delayWasOn = false, reverbWasOn = false;

    std::array<float, 8> lastEq {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalStripProcessor)
};
