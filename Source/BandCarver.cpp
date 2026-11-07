#include "BandCarver.h"

namespace
{
    constexpr float cutQ = 3.2f;      // overlapping bells: neighbouring bands blend smoothly
    constexpr float analysisQ = 4.3f; // about one band wide
}

void BandCarver::prepare (double sampleRate)
{
    sr = sampleRate;
    const float nyquistSafe = (float) sr * 0.45f;

    for (int b = 0; b < numBands; ++b)
    {
        const float hz = juce::jmin (bandFrequency (b), nyquistSafe);
        const float w0 = juce::MathConstants<float>::twoPi * hz / (float) sr;
        cosW[(size_t) b] = std::cos (w0);
        alphaCut[(size_t) b] = std::sin (w0) / (2.0f * cutQ);

        // Constant 0 dB peak band-pass for measuring each band's level
        const float alpha = std::sin (w0) / (2.0f * analysisQ);
        const float a0 = 1.0f + alpha;
        for (auto* f : { &keyBand[(size_t) b], &keyBand2[(size_t) b], &beatBand[(size_t) b] })
        {
            f->b0 = alpha / a0;
            f->b1 = 0.0f;
            f->b2 = -alpha / a0;
            f->a1 = -2.0f * cosW[(size_t) b] / a0;
            f->a2 = (1.0f - alpha) / a0;
        }
    }

    meterCoeff = std::exp (-1.0f / (0.15f * (float) sr));
    smoothCoeff = std::exp (-(float) subBlock / (0.002f * (float) sr));
    setParams (amount, 10.0f, 150.0f, lowCut, highCut, stereo);
    reset();
}

void BandCarver::reset()
{
    for (int b = 0; b < numBands; ++b)
    {
        cut[(size_t) b][0].clear();
        cut[(size_t) b][1].clear();
        keyBand[(size_t) b].clear();
        keyBand2[(size_t) b].clear();
        beatBand[(size_t) b].clear();
    }
    keyEnv.fill (0.0f);
    beatEnv.fill (0.0f);
    gainDb.fill (0.0f);
    updateGains();
    subBlockCounter = 0;
}

void BandCarver::setParams (float amount01, float attackMs, float releaseMs, float lowCutHz, float highCutHz, Stereo mode)
{
    amount = amount01;
    lowCut = lowCutHz;
    highCut = juce::jmax (highCutHz, lowCutHz * 1.1f);
    attackCoeff  = std::exp (-1.0f / (juce::jmax (0.1f, attackMs) * 0.001f * (float) sr));
    releaseCoeff = std::exp (-1.0f / (juce::jmax (1.0f, releaseMs) * 0.001f * (float) sr));
    if (mode != stereo)
    {
        for (auto& band : cut) { band[0].clear(); band[1].clear(); }
        stereo = mode;
    }
}

// 1 between Low Cut and High Cut, fading to 0 over half an octave outside them
float BandCarver::rangeWeight (float hz) const
{
    float w = 1.0f;
    if (hz < lowCut)  w = 1.0f - 2.0f * std::log2 (lowCut / hz);
    if (hz > highCut) w = 1.0f - 2.0f * std::log2 (hz / highCut);
    return juce::jlimit (0.0f, 1.0f, w);
}

void BandCarver::updateGains()
{
    std::array<float, numBands> level {};
    float loudest = -120.0f;
    for (int b = 0; b < numBands; ++b)
    {
        level[(size_t) b] = juce::Decibels::gainToDecibels (keyEnv[(size_t) b], -120.0f);
        loudest = juce::jmax (loudest, level[(size_t) b]);
    }

    for (int b = 0; b < numBands; ++b)
    {
        // Vocal band level -54 dBFS = no cut, -12 dBFS or louder = full cut.
        // Bands far below the vocal's strongest band are ignored, so a note at
        // 1 kHz doesn't spill into the bass.
        const float lv = level[(size_t) b];
        const float loudness = juce::jlimit (0.0f, 1.0f, (lv + 54.0f) / 42.0f);
        const float relative = juce::jlimit (0.0f, 1.0f, (lv - (loudest - 40.0f)) / 20.0f);
        const float strength = loudness * relative;
        const float target = -amount * maxDepthDb * strength * rangeWeight (bandFrequency (b));
        auto& g = gainDb[(size_t) b];
        g = smoothCoeff * g + (1.0f - smoothCoeff) * target;

        // Peaking bell: exactly flat when the gain is 0 dB
        const float A = std::pow (10.0f, g / 40.0f);
        const float alpha = alphaCut[(size_t) b], c = cosW[(size_t) b];
        const float a0 = 1.0f + alpha / A;
        for (auto& f : cut[(size_t) b])
        {
            f.b0 = (1.0f + alpha * A) / a0;
            f.b1 = -2.0f * c / a0;
            f.b2 = (1.0f - alpha * A) / a0;
            f.a1 = -2.0f * c / a0;
            f.a2 = (1.0f - alpha / A) / a0;
        }
    }
}

void BandCarver::process (juce::AudioBuffer<float>& beat, int numChannels, const float* key, int numSamples)
{
    numChannels = juce::jlimit (1, 2, juce::jmin (numChannels, beat.getNumChannels()));
    const bool stereoIn = numChannels == 2;
    auto* left = beat.getWritePointer (0);
    auto* right = stereoIn ? beat.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Follow the vocal's level in every band, and the beat's for the display
        const float k = key[i];
        const float monoBeat = stereoIn ? 0.5f * (left[i] + right[i]) : left[i];
        for (int b = 0; b < numBands; ++b)
        {
            const float lv = std::abs (keyBand2[(size_t) b].process (keyBand[(size_t) b].process (k)));
            auto& e = keyEnv[(size_t) b];
            const float c = lv > e ? attackCoeff : releaseCoeff;
            e = c * e + (1.0f - c) * lv;

            const float bv = std::abs (beatBand[(size_t) b].process (monoBeat));
            auto& be = beatEnv[(size_t) b];
            be = bv > be ? bv : meterCoeff * be;
        }

        if (++subBlockCounter == subBlock)
        {
            subBlockCounter = 0;
            updateGains();
        }

        // 2. Run the beat through the 32 bells
        auto runBank = [this] (float x, int ch)
        {
            for (int b = 0; b < numBands; ++b)
                x = cut[(size_t) b][(size_t) ch].process (x);
            return x;
        };

        if (! stereoIn)
            left[i] = runBank (left[i], 0);
        else if (stereo == Stereo::both)
        {
            left[i] = runBank (left[i], 0);
            right[i] = runBank (right[i], 1);
        }
        else
        {
            float mid = 0.5f * (left[i] + right[i]), side = 0.5f * (left[i] - right[i]);
            if (stereo == Stereo::mid) mid = runBank (mid, 0);
            else                       side = runBank (side, 0);
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }

    // 3. Publish band levels for the editor
    displayCounter += numSamples;
    if (displayCounter >= 256)
    {
        displayCounter = 0;
        for (int b = 0; b < numBands; ++b)
        {
            display.beatDb[(size_t) b]  = juce::Decibels::gainToDecibels (beatEnv[(size_t) b], -100.0f);
            display.vocalDb[(size_t) b] = juce::Decibels::gainToDecibels (keyEnv[(size_t) b], -100.0f);
            display.cutDb[(size_t) b]   = gainDb[(size_t) b];
        }
    }
}
