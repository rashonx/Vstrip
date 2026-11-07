#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Spectrum data shared with the editor. Written by the audio thread, read by the UI.
struct CarveDisplayData
{
    static constexpr int numPoints = 160;
    static constexpr float minHz = 20.0f, maxHz = 20000.0f;

    static float frequencyForPoint (int i)
    {
        return minHz * std::pow (maxHz / minHz, (float) i / (float) (numPoints - 1));
    }

    std::array<std::atomic<float>, numPoints> beatIn, beatOut, vocal, reductionDb;

    CarveDisplayData()
    {
        for (int i = 0; i < numPoints; ++i)
        {
            beatIn[(size_t) i] = -120.0f;
            beatOut[(size_t) i] = -120.0f;
            vocal[(size_t) i] = -120.0f;
            reductionDb[(size_t) i] = 0.0f;
        }
    }
};

// Turns the beat down only at the frequencies where the vocal currently has energy,
// so the vocal gets its own space. Works on overlapping FFT frames, which adds
// fftSize samples of latency.
class SpectralCarver
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int numBands = 40;
    static constexpr float maxDepthDb = 18.0f;

    void prepare (double sampleRate, int maxBeatChannels);
    void reset();
    void setParams (float amount01, float lowCutHz, float highCutHz, float releaseMs);

    // Carves `beat` in place using `vocal` (mono) as the key. Output is delayed by getLatency().
    void process (juce::AudioBuffer<float>& beat, int numBeatChannels, const float* vocal, int numSamples);

    static constexpr int getLatency() { return fftSize; }

    CarveDisplayData display;

private:
    void processFrame (int numBeatChannels);
    float rangeWeight (float hz) const;

    juce::dsp::FFT fft { fftOrder };
    double sr = 44100.0;
    std::vector<float> window, vocalFifo, frame, vocalPower, beatPowerIn, beatPowerOut, binGain;
    std::vector<std::vector<float>> inFifo, outFifo;
    int pos = 0, hopCounter = 0;

    std::array<int, numBands + 1> bandEdges {};
    std::array<float, numBands> bandCentreHz {}, bandGainDb {};
    std::vector<int> binBand;
    std::vector<float> binFrac;
    std::array<int, CarveDisplayData::numPoints + 1> pointEdges {};

    float amount = 0.5f, lowCut = 150.0f, highCut = 12000.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float refDb = 0.0f; // converts FFT magnitudes to roughly dBFS
};
