#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

// Per-band levels shared with the editor. Written by the audio thread, read by the UI.
struct CarveDisplayData
{
    static constexpr int numBands = 32;
    std::array<std::atomic<float>, numBands> beatDb, vocalDb, cutDb;

    CarveDisplayData()
    {
        for (int b = 0; b < numBands; ++b)
        {
            beatDb[(size_t) b] = -100.0f;
            vocalDb[(size_t) b] = -100.0f;
            cutDb[(size_t) b] = 0.0f;
        }
    }
};

// Zero-latency track spacer: a bank of 32 bell filters across the spectrum. Each band
// dips the beat by how loud the vocal (sidechain) currently is in that same band.
class BandCarver
{
public:
    static constexpr int numBands = CarveDisplayData::numBands;
    static constexpr float maxDepthDb = 24.0f;
    enum class Stereo { both = 0, mid, side };

    // Band centres from 20 Hz to 20 kHz, evenly spaced in octaves
    static float bandFrequency (int band) { return 20.0f * std::pow (1000.0f, ((float) band + 0.5f) / (float) numBands); }

    void prepare (double sampleRate);
    void reset();
    void setParams (float amount01, float attackMs, float releaseMs, float lowCutHz, float highCutHz, Stereo mode);

    // Carves `beat` in place (1 or 2 channels) using `key` (mono vocal) as the sidechain.
    void process (juce::AudioBuffer<float>& beat, int numChannels, const float* key, int numSamples);

    CarveDisplayData display;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.0f; }
    };

    void updateGains();
    float rangeWeight (float hz) const;

    double sr = 44100.0;
    std::array<std::array<Biquad, 2>, numBands> cut;   // the filters that dip the beat
    std::array<Biquad, numBands> keyBand, keyBand2, beatBand; // analysis band-passes (vocal uses two in a row for sharper bands)
    std::array<float, numBands> keyEnv {}, beatEnv {}, gainDb {}, cosW {}, alphaCut {};
    std::array<bool, numBands> active {}; // bands with nothing to cut are skipped, so the beat passes through untouched
    float amount = 0.5f, lowCut = 20.0f, highCut = 20000.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f, meterCoeff = 0.0f, smoothCoeff = 0.0f;
    Stereo stereo = Stereo::both;
    int subBlockCounter = 0, displayCounter = 0;
    static constexpr int subBlock = 16;
};
