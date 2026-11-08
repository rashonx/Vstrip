#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// A sound preset: which engine plus starting values for the tone and effect controls.
struct SoundPreset
{
    const char* name;
    int engine;          // 0 = electric piano (tine), 1 = tape piano
    float bell, bark, decay, tone;
    float tremRate, tremDepth, chorus;
    float wobble, age, hiss, reverb;
};

const std::vector<SoundPreset>& soundPresets();

// Values the voices read when a note starts
struct VoiceSettings
{
    int engine = 0;
    float bell = 0.5f, bark = 0.4f, decay = 1.0f;
};

class KeysSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class KeysVoice : public juce::SynthesiserVoice
{
public:
    explicit KeysVoice (const VoiceSettings& s) : settings (s) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<KeysSound*> (s) != nullptr; }
    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    float renderTine();
    float renderPiano();

    const VoiceSettings& settings;
    int engine = 0;
    double sr = 44100.0;
    float vel = 0.0f;
    bool released = false;
    float amp = 0.0f, ampCoef = 1.0f, releaseCoef = 1.0f, attack = 0.0f, attackStep = 0.0f;
    float pan = 0.5f;

    // Tine electric piano (FM)
    double phase = 0.0, modPhase = 0.0, tinePhase = 0.0, inc = 0.0, tineInc = 0.0;
    float modIndex = 0.0f, modCoef = 1.0f, tineAmp = 0.0f, tineCoef = 1.0f, drive = 1.0f, bell = 0.5f;

    // Piano (additive partials as rotating phasors)
    static constexpr int maxPartials = 26;
    int numPartials = 0, renorm = 0;
    std::array<float, maxPartials> re {}, im {}, rotC {}, rotS {}, slowAmp {}, slowCoef {}, fastAmp {}, fastCoef {};
    float hammer = 0.0f, hammerCoef = 1.0f, noiseState = 0.0f;
    juce::Random rng;
};

struct FxSettings
{
    float tone = 0.7f, tremRate = 4.5f, tremDepth = 0.0f, chorus = 0.0f;
    float wobble = 0.0f, age = 0.0f, hiss = 0.0f, reverb = 0.2f, volume = 0.0f;
    bool notesActive = true; // hiss fades out when nothing is playing
};

// Tone, tremolo, chorus, tape (wow & flutter, saturation, hiss) and reverb
class FxChain
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s);

private:
    double sr = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> tapeDelay { 4096 };
    juce::dsp::Reverb reverb;
    double tremPhase = 0.0, wowPhase = 0.0, flutterPhase = 0.0;
    std::array<float, 2> ageLp {}, hissLp {};
    juce::Random rng;
    juce::SmoothedValue<float> gain;
    float hissLevel = 0.0f;
};
