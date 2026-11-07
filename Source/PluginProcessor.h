#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Chords.h"
#include "SoundEngine.h"

// One key plays a whole neo soul chord. White keys = the chord set's chords,
// black keys = related chords that lead into the next white key.
// Split mode: keys below the split point use zone A, keys above use zone B,
// each with its own chord set, key, scale, extensions, octave and sound.
class SoulKeysProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numZones = 2;
    static constexpr int scaleSlot = 50;  // chord-set index of "Key & Scale"
    static constexpr int fileSlot = 51;   // chord-set index of "From file"

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

    // Parameter id for a zone: zone A uses the plain id, zone B adds "B"
    static juce::String pid (int zone, const char* base) { return zone == 0 ? juce::String (base) : juce::String (base) + "B"; }

    int numChordSets() const { return fileSlot + 1; }
    juce::String chordSetName (int zone, int index) const;
    ChordSet getChordSet (int zone, int index) const;   // as written, before key transposition
    void setUserChords (int zone, const ChordSet& set); // also selects "From file" for that zone
    bool hasUserChords (int zone) const;

    bool splitOn() const { return param (splitParam) > 0.5f; }
    int splitPoint() const { return juce::roundToInt (param (splitPointParam)); }
    int zoneOf (int key) const { return splitOn() && key >= splitPoint() ? 1 : 0; }

    // The chord a key would play right now, for the display
    Chord chordForKey (int pitchClass, int zone) const;

    void applySoundPreset (int zone, int index);
    void swapZones();

    std::atomic<int> lastKeyPc[numZones] { -1, -1 };
    std::atomic<bool> keyHeld[numZones][12] {};

private:
    struct Pending { double time; int note; float velocity; int sourceKey; int zone; };

    struct ZoneParams
    {
        std::atomic<float> *chordSet, *extensions, *octave, *key, *scale, *sound;
        std::atomic<float> *bell, *bark, *decay, *tone, *tremRate, *tremDepth, *chorus, *wobble, *age, *hiss, *reverb, *volume;
    };

    struct Zone
    {
        juce::Synthesiser synth;
        VoiceSettings voice;
        FxChain fx;
        FxSettings fxSettings;
        ChordSet user;            // chords pulled from a file
        ChordSet scaleSet;        // cached "Key & Scale" chords (audio thread only)
        int scaleKey = -1, scaleIndex = -1;
        std::array<int, 128> refs {};
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer;
        ZoneParams p {};
    };

    float param (std::atomic<float>* p) const { return p->load(); }
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    Chord pickChord (int zone, int pc, bool forDisplay) const;
    Chord resolveChord (int key, int zone, std::vector<int>* previousUpper, bool forDisplay) const;
    void handleKeyOn (int key, float velocity, int samplePos);
    void handleKeyOff (int key, int samplePos);
    void emitDue (int numSamples);

    std::array<Zone, numZones> zones;
    std::atomic<float> *splitParam = nullptr, *splitPointParam = nullptr;
    mutable juce::CriticalSection chordLock;

    std::vector<Pending> pending;
    std::array<std::vector<int>, 128> notesFromKey;
    std::array<int, 128> keyZone {};
    std::vector<int> lastUpper; // shared by both zones, so moves between them stay smooth
    double blockStart = 0.0;
    bool strumUp = true;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKeysProcessor)
};
