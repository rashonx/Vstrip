// Runs a beat (main input) and vocal (sidechain) through the plugin and checks the carving.
#include "../Source/PluginProcessor.h"

namespace
{
    int failures = 0;
    void check (bool ok, const char* what, float value)
    {
        std::printf ("%s %-50s %8.2f\n", ok ? "  ok  " : "  FAIL", what, value);
        if (! ok) ++failures;
    }

    void set (VocalSitterProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    struct Result { float inRms, outRms, maxDiff; };

    // beatL/beatR/vocal are generator functions of time. Measures the last second.
    template <typename L, typename R, typename V>
    Result run (VocalSitterProcessor& p, double rate, float seconds, L beatL, R beatR, V vocal)
    {
        const int block = 480;
        juce::AudioBuffer<float> buf (4, block);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * rate) / block;
        double inSum = 0, outSum = 0;
        float maxDiff = 0.0f;
        long count = 0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            float inL[480], inR[480];
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (b * block + i) / rate;
                inL[i] = beatL (t); inR[i] = beatR (t);
                buf.setSample (0, i, inL[i]); buf.setSample (1, i, inR[i]);
                buf.setSample (2, i, vocal (t)); buf.setSample (3, i, vocal (t));
            }
            p.processBlock (buf, midi);
            const bool measure = b >= blocks - (int) rate / block;
            for (int i = 0; i < block; ++i)
            {
                const float oL = buf.getSample (0, i), oR = buf.getSample (1, i);
                finite = finite && std::isfinite (oL) && std::isfinite (oR);
                if (measure)
                {
                    inSum += (double) (inL[i] * inL[i] + inR[i] * inR[i]);
                    outSum += (double) (oL * oL + oR * oR);
                    maxDiff = juce::jmax (maxDiff, std::abs (oL - inL[i]), std::abs (oR - inR[i]));
                    count += 2;
                }
            }
        }
        check (finite, "output is finite", 0.0f);
        return { (float) std::sqrt (inSum / (double) count), (float) std::sqrt (outSum / (double) count), maxDiff };
    }

    float cutNear (const VocalSitterProcessor& p, float hz)
    {
        int best = 0;
        for (int b = 0; b < BandCarver::numBands; ++b)
            if (std::abs (std::log (BandCarver::bandFrequency (b) / hz)) < std::abs (std::log (BandCarver::bandFrequency (best) / hz)))
                best = b;
        return p.getDisplay().cutDb[(size_t) best].load();
    }

    float db (float ratio) { return juce::Decibels::gainToDecibels (ratio); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto twoPi = juce::MathConstants<double>::twoPi;
    auto silence = [] (double) { return 0.0f; };
    auto tone1k = [&] (double t) { return 0.3f * (float) std::sin (twoPi * 1000.0 * t); };
    auto beat = [&] (double t) { return 0.2f * (float) (std::sin (twoPi * 200.0 * t) + std::sin (twoPi * 1000.0 * t)); };
    auto beatOnly1k = [&] (double t) { return 0.3f * (float) std::sin (twoPi * 1000.0 * t); };
    auto beatOnly200 = [&] (double t) { return 0.3f * (float) std::sin (twoPi * 200.0 * t); };
    auto sideOnlyR = [&] (double t) { return -0.3f * (float) std::sin (twoPi * 1000.0 * t); };

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("%.0f Hz\n", rate);
        VocalSitterProcessor p;
        p.setRateAndBufferSizeDetails (rate, 480);
        p.prepareToPlay (rate, 480);
        check (p.getLatencySamples() == 0, "zero latency (samples)", (float) p.getLatencySamples());

        // 1. Silent vocal: beat passes through untouched (filters are exactly flat at 0 dB)
        auto r1 = run (p, rate, 1.0f, beat, beat, silence);
        check (r1.maxDiff < 1.0e-4f, "silent vocal leaves the beat untouched (max diff)", r1.maxDiff);

        // 2. Amount 0 with a vocal: still untouched
        set (p, "amount", 0.0f);
        auto r2 = run (p, rate, 1.0f, beat, beat, tone1k);
        check (r2.maxDiff < 1.0e-4f, "0% amount leaves the beat untouched (max diff)", r2.maxDiff);

        // 3. Full amount: the beat's 1 kHz is cut, its 200 Hz is not
        set (p, "amount", 100.0f);
        auto r3a = run (p, rate, 1.0f, beatOnly1k, beatOnly1k, tone1k);
        auto r3b = run (p, rate, 1.0f, beatOnly200, beatOnly200, tone1k);
        check (db (r3a.outRms / r3a.inRms) < -12.0f, "beat at 1 kHz cut where vocal is (dB)", db (r3a.outRms / r3a.inRms));
        check (db (r3b.outRms / r3b.inRms) > -1.5f, "beat at 200 Hz mostly untouched (dB)", db (r3b.outRms / r3b.inRms));
        check (cutNear (p, 1000.0f) < -12.0f, "display shows the cut at 1 kHz (dB)", cutNear (p, 1000.0f));

        // 4. Low Cut above the vocal: no carving
        set (p, "lowCut", 2000.0f);
        auto r4 = run (p, rate, 1.0f, beatOnly1k, beatOnly1k, tone1k);
        check (db (r4.outRms / r4.inRms) > -1.0f, "nothing carved below Low Cut (dB)", db (r4.outRms / r4.inRms));
        set (p, "lowCut", 20.0f);

        // 5. Mid mode leaves a side-only beat alone; Side mode carves it
        set (p, "stereo", 1.0f);
        auto r5 = run (p, rate, 1.0f, beatOnly1k, sideOnlyR, tone1k);
        check (db (r5.outRms / r5.inRms) > -0.5f, "Mid mode ignores side-only beat (dB)", db (r5.outRms / r5.inRms));
        set (p, "stereo", 2.0f);
        auto r6 = run (p, rate, 1.0f, beatOnly1k, sideOnlyR, tone1k);
        check (db (r6.outRms / r6.inRms) < -12.0f, "Side mode carves side-only beat (dB)", db (r6.outRms / r6.inRms));
        set (p, "stereo", 0.0f);

        // 6. Auto gain brings the carved beat back to its input level
        set (p, "autoGain", 1.0f);
        auto r7 = run (p, rate, 4.0f, beat, beat, tone1k);
        check (std::abs (db (r7.outRms / r7.inRms)) < 1.0f, "auto gain matches input level (dB)", db (r7.outRms / r7.inRms));
        set (p, "autoGain", 0.0f);

        // 7. Listen to carved part: silent vocal = silence; carved part + carved beat = original
        set (p, "listen", 1.0f);
        auto r9 = run (p, rate, 2.0f, beat, beat, silence); // 2 s: let the release from the last test finish
        check (r9.outRms < 1.0e-5f, "listen mode is silent when nothing is carved (RMS)", r9.outRms);
        auto r10 = run (p, rate, 1.0f, beatOnly1k, beatOnly1k, tone1k);
        set (p, "listen", 0.0f);
        auto r11 = run (p, rate, 1.0f, beatOnly1k, beatOnly1k, tone1k);
        check (db (r10.outRms / r10.inRms) > -3.0f, "listen mode plays what was carved (dB vs input)", db (r10.outRms / r10.inRms));
        check (db (r11.outRms / r11.inRms) < -12.0f, "normal mode plays the carved beat (dB)", db (r11.outRms / r11.inRms));

        // 8. No sidechain connected
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::disabled();
        p.setBusesLayout (layout);
        p.prepareToPlay (rate, 480);
        auto r8 = run (p, rate, 1.0f, beat, beat, silence);
        check (r8.maxDiff < 1.0e-4f, "works with no sidechain connected (max diff)", r8.maxDiff);
    }

    VocalSitterProcessor a, b;
    set (a, "amount", 73.0f);
    set (a, "stereo", 2.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);
    b.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (b.apvts.getRawParameterValue ("amount")->load() - 73.0f) < 0.6f, "settings restore (amount)", b.apvts.getRawParameterValue ("amount")->load());
    check (juce::roundToInt (b.apvts.getRawParameterValue ("stereo")->load()) == 2, "settings restore (stereo mode)", b.apvts.getRawParameterValue ("stereo")->load());

    std::printf (failures == 0 ? "PASS\n" : "FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
