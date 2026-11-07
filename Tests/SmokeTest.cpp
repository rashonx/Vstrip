// Runs vocal + beat through the plugin and checks carving, level matching and stability.
#include "../Source/PluginProcessor.h"

namespace
{
    int failures = 0;
    void check (bool ok, const char* what, float value)
    {
        std::printf ("%s %-46s %8.2f\n", ok ? "  ok  " : "  FAIL", what, value);
        if (! ok) ++failures;
    }

    void set (VocalSitterProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Plays `seconds` of audio; vocal and beat are generator functions of time.
    // Returns the RMS of input (vocal + beat) and output over the last second.
    template <typename V, typename B>
    std::pair<float, float> run (VocalSitterProcessor& p, double rate, float seconds, V vocal, B beat)
    {
        const int block = 480;
        juce::AudioBuffer<float> buf (4, block);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * rate) / block;
        double inSum = 0, outSum = 0;
        long count = 0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (b * block + i) / rate;
                const float v = vocal (t), m = beat (t);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
                buf.setSample (2, i, m); buf.setSample (3, i, m);
                if (b >= blocks - (int) rate / block) inSum += (double) ((v + m) * (v + m));
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                const float o = buf.getSample (0, i);
                finite = finite && std::isfinite (o);
                if (b >= blocks - (int) rate / block) { outSum += (double) (o * o); ++count; }
            }
        }
        check (finite, "output is finite", 0.0f);
        return { (float) std::sqrt (inSum / (double) count), (float) std::sqrt (outSum / (double) count) };
    }

    float displayReductionAt (const VocalSitterProcessor& p, float hz)
    {
        int best = 0;
        for (int i = 0; i < CarveDisplayData::numPoints; ++i)
            if (std::abs (std::log (CarveDisplayData::frequencyForPoint (i) / hz)) < std::abs (std::log (CarveDisplayData::frequencyForPoint (best) / hz)))
                best = i;
        return p.getDisplay().reductionDb[(size_t) best].load();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto twoPi = juce::MathConstants<double>::twoPi;
    auto silence = [] (double) { return 0.0f; };
    auto vocalTone = [&] (double t) { return 0.3f * (float) std::sin (twoPi * 1000.0 * t); };
    auto beatTones = [&] (double t) { return 0.2f * (float) (std::sin (twoPi * 200.0 * t) + std::sin (twoPi * 1000.0 * t)); };

    for (double rate : { 44100.0, 48000.0 })
    {
        std::printf ("%.0f Hz\n", rate);
        VocalSitterProcessor p;
        p.setRateAndBufferSizeDetails (rate, 480);
        p.prepareToPlay (rate, 480);
        check (p.getLatencySamples() == SpectralCarver::getLatency(), "reports latency (samples)", (float) p.getLatencySamples());

        // 1. With no carving and auto gain off, the beat passes through untouched
        set (p, "autoGain", 0.0f);
        set (p, "carveAmount", 0.0f);
        auto [in1, out1] = run (p, rate, 2.0f, silence, beatTones);
        const float passDb = juce::Decibels::gainToDecibels (out1 / in1);
        check (std::abs (passDb) < 0.3f, "beat passes unchanged at 0% carve (dB)", passDb);

        // 2. Full carve: the 1 kHz part of the beat is cut, 200 Hz is left alone
        set (p, "carveAmount", 100.0f);
        set (p, "carveLow", 400.0f);
        run (p, rate, 2.0f, vocalTone, beatTones);
        const float at1k = displayReductionAt (p, 1000.0f), at200 = displayReductionAt (p, 200.0f);
        check (at1k < -12.0f, "carve depth at 1 kHz, where the vocal is (dB)", at1k);
        check (at200 > -1.0f, "carve depth at 200 Hz, below Low Cut (dB)", at200);

        // 3. Auto gain keeps the output level matched to the input
        set (p, "autoGain", 1.0f);
        set (p, "vocalLevel", -6.0f);
        auto [in3, out3] = run (p, rate, 4.0f, vocalTone, beatTones);
        const float matchDb = juce::Decibels::gainToDecibels (out3 / in3);
        check (std::abs (matchDb) < 1.0f, "auto gain output vs input level (dB)", matchDb);

        // 4. Vocal only, sidechain switched off
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::disabled();
        p.setBusesLayout (layout);
        p.prepareToPlay (rate, 480);
        auto [in4, out4] = run (p, rate, 1.0f, vocalTone, silence);
        check (out4 > 0.01f, "works with no sidechain connected (RMS)", out4);
    }

    // Settings survive save and reload
    VocalSitterProcessor a, b;
    set (a, "carveAmount", 73.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);
    b.setStateInformation (state.getData(), (int) state.getSize());
    const float restored = b.apvts.getRawParameterValue ("carveAmount")->load();
    check (std::abs (restored - 73.0f) < 0.6f, "settings restore", restored);

    std::printf (failures == 0 ? "PASS\n" : "FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
