#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Chords.h"
#include "SoundEngine.h"

// One key plays a whole neo soul chord. White keys = the preset's chords,
// black keys = related chords that lead into the next white key.
class SoulKeysProcessor : public juce::AudioProcessor
{
public:
    SoulKeysProcessor();
    ~SoulKeysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // Chord sets: the 50 presets plus one slot filled from a MIDI or audio file
    int numChordSets() const { return ChordPresets::count() + 1; }
    int userSlot() const { return ChordPresets::count(); }
    ChordSet getChordSet (int index) const;
    ChordSet currentChordSet() const;
    void setUserChords (const ChordSet& set);
    bool hasUserChords() const;

    // The chord a key would play right now (with extensions applied), for the display
    Chord chordForKey (int pitchClass) const;

    // What's playing, for the window
    std::atomic<int> lastKeyPc { -1 };
    std::atomic<bool> keyHeld[12] {};

    void applySoundPreset (int index);

private:
    struct Pending { double time; int note; float velocity; int sourceKey; };

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    Chord resolveChord (int key, std::vector<int>& previousUpper) const;
    void handleKeyOn (int key, float velocity, int samplePos);
    void handleKeyOff (int key, int samplePos);
    void emitDue (juce::MidiBuffer& out, int numSamples);

    juce::Synthesiser synth;
    VoiceSettings voiceSettings;
    FxChain fx;
    FxSettings fxSettings;

    mutable juce::CriticalSection chordLock;
    ChordSet userChords;
    mutable int cachedSetIndex = -1;
    mutable ChordSet cachedSet;

    // Chord triggering
    std::vector<Pending> pending;
    std::array<std::vector<int>, 128> notesFromKey;  // output notes started by each input key
    std::array<int, 128> noteRefs {};                 // how many held keys share each output note
    std::vector<int> lastUpper;                       // for voice leading
    double blockStart = 0.0;
    bool strumUp = true;
    juce::Random rng;
    juce::MidiBuffer expanded;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKeysProcessor)
};
