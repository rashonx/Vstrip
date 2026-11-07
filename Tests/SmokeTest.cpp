// Runs audio through the plugin with every module switched on and checks the output stays sane.
#include "../Source/PluginProcessor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    VocalStripProcessor proc;
    for (auto id : { "gateOn", "deEssOn", "compOn", "satOn", "delayOn", "reverbOn" })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter ("midGain")->setValueNotifyingHost (0.8f);

    int failures = 0;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const int block = 512;
        proc.setRateAndBufferSizeDetails (rate, block);
        proc.prepareToPlay (rate, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        juce::Random rng (1);
        float peak = 0.0f;
        double phase = 0.0;
        const int blocks = (int) (rate * 5.0) / block;

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                // Tone plus hissy noise bursts (like "s" sounds), silent for the last second
                const bool silent = b > blocks - (int) rate / block;
                const float noise = (b / 20) % 2 ? (rng.nextFloat() * 2.0f - 1.0f) * 0.3f : 0.0f;
                const float s = silent ? 0.0f : 0.5f * (float) std::sin (phase) + noise;
                phase += juce::MathConstants<double>::twoPi * 220.0 / rate;
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) { ++failures; break; }
                    peak = juce::jmax (peak, std::abs (v));
                }
        }
        std::printf ("%6.0f Hz: output peak %.2f dB, comp GR %.1f dB\n", rate,
                     juce::Decibels::gainToDecibels (peak), proc.compReductionDb.load());
        if (peak > 4.0f || peak < 0.01f) ++failures;
    }

    // Save and restore settings
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    VocalStripProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    if (other.apvts.getRawParameterValue ("reverbOn")->load() < 0.5f) ++failures;

    std::printf (failures == 0 ? "PASS\n" : "FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
