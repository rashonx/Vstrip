#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// De-esser

void DeEsser::prepare (const juce::dsp::ProcessSpec& spec)
{
    crossover.prepare (spec);
    envelope.assign (spec.numChannels, 0.0f);
    attackCoeff  = std::exp (-1.0f / (0.001f * (float) spec.sampleRate)); // 1 ms
    releaseCoeff = std::exp (-1.0f / (0.060f * (float) spec.sampleRate)); // 60 ms
}

void DeEsser::reset()
{
    crossover.reset();
    std::fill (envelope.begin(), envelope.end(), 0.0f);
    reductionDb = 0.0f;
}

void DeEsser::setParams (float frequencyHz, float thresholdDb)
{
    if (! juce::exactlyEqual (crossover.getCutoffFrequency(), frequencyHz))
        crossover.setCutoffFrequency (frequencyHz);
    threshold = thresholdDb;
}

void DeEsser::process (juce::AudioBuffer<float>& buffer)
{
    constexpr float slope = 0.75f;     // 4:1 ratio on the sibilant band
    constexpr float maxCutDb = -24.0f;
    float deepest = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& env = envelope[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float low = 0.0f, high = 0.0f;
            crossover.processSample (ch, data[i], low, high);

            const float level = std::abs (high);
            const float coeff = level > env ? attackCoeff : releaseCoeff;
            env = coeff * env + (1.0f - coeff) * level;

            const float over = juce::Decibels::gainToDecibels (env, -120.0f) - threshold;
            const float cutDb = over > 0.0f ? juce::jmax (maxCutDb, -over * slope) : 0.0f;
            deepest = juce::jmin (deepest, cutDb);

            data[i] = low + high * juce::Decibels::decibelsToGain (cutDb);
        }
    }
    reductionDb = deepest;
}

//==============================================================================
// Parameters

juce::AudioProcessorValueTreeState::ParameterLayout VocalStripProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto ratio = [] (float v, int) { return String (v, 1) + ":1"; };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    addFloat ("inGain", "Input", { -24.0f, 24.0f, 0.1f }, 0.0f, db);
    addFloat ("hpFreq", "High-Pass", skewed (20.0f, 400.0f, 100.0f), 80.0f, hz);

    addBool  ("gateOn", "Gate On", false);
    addFloat ("gateThresh", "Gate Threshold", { -80.0f, 0.0f, 0.1f }, -50.0f, db);

    addFloat ("lowGain", "Body (200 Hz)", { -12.0f, 12.0f, 0.1f }, 0.0f, db);
    addFloat ("midFreq", "Presence Freq", skewed (300.0f, 6000.0f, 2000.0f), 3000.0f, hz);
    addFloat ("midGain", "Presence", { -12.0f, 12.0f, 0.1f }, 0.0f, db);
    addFloat ("airGain", "Air (10 kHz)", { -12.0f, 12.0f, 0.1f }, 0.0f, db);

    addBool  ("deEssOn", "De-Ess On", true);
    addFloat ("deEssFreq", "De-Ess Freq", skewed (3000.0f, 12000.0f, 6000.0f), 6000.0f, hz);
    addFloat ("deEssThresh", "De-Ess Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, db);

    addBool  ("compOn", "Comp On", true);
    addFloat ("compThresh", "Comp Threshold", { -60.0f, 0.0f, 0.1f }, -18.0f, db);
    addFloat ("compRatio", "Ratio", skewed (1.0f, 20.0f, 4.0f), 4.0f, ratio);
    addFloat ("compAttack", "Attack", skewed (0.1f, 100.0f, 10.0f), 10.0f, ms);
    addFloat ("compRelease", "Release", skewed (10.0f, 1000.0f, 120.0f), 120.0f, ms);
    addFloat ("compMakeup", "Makeup", { 0.0f, 24.0f, 0.1f }, 3.0f, db);

    addBool  ("satOn", "Saturation On", false);
    addFloat ("satDrive", "Drive", { 0.0f, 24.0f, 0.1f }, 6.0f, db);
    addFloat ("satMix", "Saturation Mix", { 0.0f, 100.0f, 1.0f }, 50.0f, pct);

    addBool  ("delayOn", "Delay On", false);
    addFloat ("delayTime", "Delay Time", skewed (20.0f, 1500.0f, 300.0f), 250.0f, ms);
    addFloat ("delayFeedback", "Feedback", { 0.0f, 90.0f, 1.0f }, 30.0f, pct);
    addFloat ("delayMix", "Delay Mix", { 0.0f, 100.0f, 1.0f }, 20.0f, pct);

    addBool  ("reverbOn", "Reverb On", false);
    addFloat ("reverbSize", "Room Size", { 0.0f, 100.0f, 1.0f }, 50.0f, pct);
    addFloat ("reverbDamp", "Damping", { 0.0f, 100.0f, 1.0f }, 50.0f, pct);
    addFloat ("reverbMix", "Reverb Mix", { 0.0f, 100.0f, 1.0f }, 15.0f, pct);

    addFloat ("outGain", "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, db);

    return { p.begin(), p.end() };
}

//==============================================================================

VocalStripProcessor::VocalStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool VocalStripProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void VocalStripProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                        (juce::uint32) getTotalNumOutputChannels() };

    for (auto* g : { &inputGain, &outputGain, &makeupGain })
    {
        g->prepare (spec);
        g->setRampDurationSeconds (0.05);
    }

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    highPass.state = Coeffs::makeHighPass (sampleRate, 80.0f);
    lowShelf.state = Coeffs::makeLowShelf (sampleRate, 200.0f, 0.707f, 1.0f);
    midPeak.state  = Coeffs::makePeakFilter (sampleRate, 3000.0f, 1.0f, 1.0f);
    airShelf.state = Coeffs::makeHighShelf (sampleRate, 10000.0f, 0.707f, 1.0f);
    for (auto* f : { &highPass, &lowShelf, &midPeak, &airShelf })
        f->prepare (spec);
    lastEq.fill (-1.0f);
    updateEq();

    gate.prepare (spec);
    gate.setRatio (10.0f);
    gate.setAttack (1.0f);
    gate.setRelease (100.0f);

    deEsser.prepare (spec);
    compressor.prepare (spec);

    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 1.6) + 1);
    delayTimeSamples.reset (sampleRate, 0.1);
    delayTimeSamples.setCurrentAndTargetValue (param ("delayTime") * 0.001f * (float) sampleRate);

    reverb.prepare (spec);
}

void VocalStripProcessor::updateEq()
{
    const std::array<float, 8> now { param ("hpFreq"), param ("lowGain"), param ("midFreq"),
                                     param ("midGain"), param ("airGain"), 0, 0, 0 };
    if (now == lastEq)
        return;
    lastEq = now;

    using A = juce::dsp::IIR::ArrayCoefficients<float>;
    const auto g = [] (float dB) { return juce::Decibels::decibelsToGain (dB); };
    *highPass.state = A::makeHighPass (sr, now[0]);
    *lowShelf.state = A::makeLowShelf (sr, 200.0f, 0.707f, g (now[1]));
    *midPeak.state  = A::makePeakFilter (sr, now[2], 1.0f, g (now[3]));
    *airShelf.state = A::makeHighShelf (sr, 10000.0f, 0.707f, g (now[4]));
}

void VocalStripProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    delayTimeSamples.setTargetValue (param ("delayTime") * 0.001f * (float) sr);
    const float feedback = param ("delayFeedback") * 0.01f;
    const float mix = param ("delayMix") * 0.01f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float time = delayTimeSamples.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float echo = delayLine.popSample (ch, time);
            delayLine.pushSample (ch, data[i] + echo * feedback);
            data[i] += echo * mix;
        }
    }
}

static float peakDb (const juce::AudioBuffer<float>& b)
{
    float peak = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, b.getNumSamples()));
    return juce::Decibels::gainToDecibels (peak, -100.0f);
}

void VocalStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 1. Input level
    inputGain.setGainDecibels (param ("inGain"));
    inputGain.process (ctx);
    inputLevelDb = peakDb (buffer);

    // 2. High-pass to remove rumble and mic-stand thumps
    updateEq();
    highPass.process (ctx);

    // 3. Noise gate
    if (on ("gateOn"))
    {
        gate.setThreshold (param ("gateThresh"));
        gate.process (ctx);
    }

    // 4. Tone EQ: body, presence, air
    lowShelf.process (ctx);
    midPeak.process (ctx);
    airShelf.process (ctx);

    // 5. De-esser
    if (on ("deEssOn"))
    {
        deEsser.setParams (param ("deEssFreq"), param ("deEssThresh"));
        deEsser.process (buffer);
    }

    // 6. Compressor
    if (on ("compOn"))
    {
        compressor.setThreshold (param ("compThresh"));
        compressor.setRatio (param ("compRatio"));
        compressor.setAttack (param ("compAttack"));
        compressor.setRelease (param ("compRelease"));

        const float before = peakDb (buffer);
        compressor.process (ctx);
        const float after = peakDb (buffer);
        compReductionDb = before > -90.0f ? juce::jmin (0.0f, after - before) : 0.0f;

        makeupGain.setGainDecibels (param ("compMakeup"));
        makeupGain.process (ctx);
    }
    else
    {
        compReductionDb = 0.0f;
    }

    // 7. Saturation (soft clipping, blended with the clean signal)
    if (on ("satOn"))
    {
        const float drive = juce::Decibels::decibelsToGain (param ("satDrive"));
        const float mix = param ("satMix") * 0.01f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float wet = std::tanh (data[i] * drive) / drive;
                data[i] += mix * (wet - data[i]);
            }
        }
    }

    // 8. Delay
    const bool delayOn = on ("delayOn");
    if (delayOn && ! delayWasOn)
        delayLine.reset();
    delayWasOn = delayOn;
    if (delayOn)
        processDelay (buffer);

    // 9. Reverb
    const bool reverbOn = on ("reverbOn");
    if (reverbOn && ! reverbWasOn)
        reverb.reset();
    reverbWasOn = reverbOn;
    if (reverbOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = param ("reverbSize") * 0.01f;
        rp.damping  = param ("reverbDamp") * 0.01f;
        rp.wetLevel = param ("reverbMix") * 0.01f;
        rp.dryLevel = 1.0f - 0.5f * rp.wetLevel;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
    }

    // 10. Output level
    outputGain.setGainDecibels (param ("outGain"));
    outputGain.process (ctx);
    outputLevelDb = peakDb (buffer);
}

//==============================================================================

void VocalStripProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalStripProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocalStripProcessor::createEditor()
{
    return new VocalStripEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalStripProcessor();
}
