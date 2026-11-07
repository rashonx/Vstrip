#include "PluginProcessor.h"
#include "PluginEditor.h"

std::array<float, 6> VocalEq::coefficients (int band, float gainDb, double sampleRate)
{
    using A = juce::dsp::IIR::ArrayCoefficients<float>;
    const float gain = juce::Decibels::decibelsToGain (gainDb);
    const float hz = frequencies[(size_t) band];
    if (band == 0)
        return A::makeLowShelf (sampleRate, hz, 0.707f, gain);
    if (band == numBands - 1)
        return A::makeHighShelf (sampleRate, juce::jmin (hz, (float) sampleRate * 0.45f), 0.707f, gain);
    return A::makePeakFilter (sampleRate, hz, 1.1f, gain);
}

//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout VocalSitterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    addFloat ("carveAmount", "Carve Amount", { 0.0f, 100.0f, 1.0f }, 50.0f, pct);
    addFloat ("carveLow", "Carve Low Cut", skewed (20.0f, 2000.0f, 200.0f), 150.0f, hz);
    addFloat ("carveHigh", "Carve High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f, hz);
    addFloat ("carveRelease", "Carve Release", skewed (20.0f, 1000.0f, 150.0f), 150.0f, ms);

    for (int b = 0; b < VocalEq::numBands; ++b)
    {
        const float f = VocalEq::frequencies[(size_t) b];
        const String label = f >= 1000.0f ? String (f / 1000.0f, 1) + "k" : String (roundToInt (f));
        addFloat ("eq" + String (b), "EQ " + label, { -12.0f, 12.0f, 0.1f }, 0.0f, db);
    }

    addFloat ("vocalLevel", "Vocal Level", { -24.0f, 12.0f, 0.1f }, 0.0f, db);
    addFloat ("beatLevel", "Beat Level", { -24.0f, 12.0f, 0.1f }, 0.0f, db);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "beatToOutput", 1 }, "Beat To Output", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autoGain", 1 }, "Auto Gain", true));
    addFloat ("outGain", "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, db);

    return { p.begin(), p.end() };
}

//==============================================================================

VocalSitterProcessor::VocalSitterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Vocal", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain (Beat)", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool VocalSitterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }
    return true;
}

void VocalSitterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int channels = getMainBusNumOutputChannels();
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) channels };

    for (int b = 0; b < VocalEq::numBands; ++b)
    {
        eq[(size_t) b].state = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        eq[(size_t) b].prepare (spec);
    }
    lastEqGains.fill (-100.0f);
    updateEq();

    carver.prepare (sampleRate, channels);
    setLatencySamples (SpectralCarver::getLatency());

    beatBuffer.setSize (channels, samplesPerBlock);
    vocalDelay.setSize (channels, SpectralCarver::getLatency());
    vocalDelay.clear();
    vocalKey.assign ((size_t) samplesPerBlock, 0.0f);
    delayPos = 0;

    for (auto* s : { &vocalGain, &beatGain, &outGain })
        s->reset (sampleRate, 0.05);
    vocalGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("vocalLevel")));
    beatGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("beatLevel")));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("outGain")));
    autoGain.reset (sampleRate, 0.2);
    autoGain.setCurrentAndTargetValue (1.0f);
    refPower = outPower = 0.0f;
}

void VocalSitterProcessor::updateEq()
{
    for (int b = 0; b < VocalEq::numBands; ++b)
    {
        const float g = param ("eq" + juce::String (b));
        if (juce::exactlyEqual (g, lastEqGains[(size_t) b]))
            continue;
        lastEqGains[(size_t) b] = g;
        *eq[(size_t) b].state = VocalEq::coefficients (b, g, sr);
    }
}

static float peakDb (const juce::AudioBuffer<float>& b, int channels, int samples)
{
    float peak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, samples));
    return juce::Decibels::gainToDecibels (peak, -100.0f);
}

void VocalSitterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto vocal = getBusBuffer (buffer, true, 0);
    const int channels = vocal.getNumChannels();
    if (channels == 0 || n == 0)
        return;

    if (beatBuffer.getNumSamples() < n || beatBuffer.getNumChannels() < channels)
        beatBuffer.setSize (juce::jmax (channels, beatBuffer.getNumChannels()), n, false, false, true);
    if ((int) vocalKey.size() < n)
        vocalKey.resize ((size_t) n);

    // 1. Pick up the beat from the sidechain, matched to the vocal's channel count
    int sideChannels = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        sideChannels = side.getNumChannels();
        for (int ch = 0; ch < channels && sideChannels > 0; ++ch)
        {
            if (channels == 1 && sideChannels == 2)
            {
                // Stereo beat into a mono vocal track: mix it down
                beatBuffer.clear (0, 0, n);
                beatBuffer.addFrom (0, 0, side, 0, 0, n, 0.5f);
                beatBuffer.addFrom (0, 0, side, 1, 0, n, 0.5f);
            }
            else
            {
                beatBuffer.copyFrom (ch, 0, side, juce::jmin (ch, sideChannels - 1), 0, n);
            }
        }
    }
    const int beatChannels = sideChannels > 0 ? channels : 0;
    const bool beatOn = beatChannels > 0 && param ("beatToOutput") > 0.5f;

    // Reference level for auto gain: what the vocal + beat sounded like coming in
    double refSum = 0.0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* v = vocal.getReadPointer (ch);
        const auto* b = beatBuffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float s = v[i] + (beatOn ? b[i] : 0.0f);
            refSum += (double) (s * s);
        }
    }
    vocalLevelDb = peakDb (vocal, channels, n);
    beatLevelDb = beatChannels > 0 ? peakDb (beatBuffer, beatChannels, n) : -100.0f;

    // 2. Vocal EQ
    updateEq();
    {
        juce::dsp::AudioBlock<float> block (vocal);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        for (auto& band : eq)
            band.process (ctx);
    }

    // 3. Carve the beat wherever the (EQ'd) vocal is
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            s += vocal.getSample (ch, i);
        vocalKey[(size_t) i] = s / (float) channels;
    }
    carver.setParams (param ("carveAmount") * 0.01f, param ("carveLow"), param ("carveHigh"), param ("carveRelease"));
    carver.process (beatBuffer, beatChannels, vocalKey.data(), n);

    // 4. Delay the vocal by the carver's latency so both stay in time
    const int delayLength = vocalDelay.getNumSamples();
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* v = vocal.getWritePointer (ch);
        auto* d = vocalDelay.getWritePointer (ch);
        int p = delayPos;
        for (int i = 0; i < n; ++i)
        {
            std::swap (v[i], d[p]);
            if (++p == delayLength)
                p = 0;
        }
    }
    delayPos = (delayPos + n) % delayLength;

    // 5. Mix vocal and carved beat
    vocalGain.setTargetValue (juce::Decibels::decibelsToGain (param ("vocalLevel")));
    beatGain.setTargetValue (beatOn ? juce::Decibels::decibelsToGain (param ("beatLevel")) : 0.0f);
    double outSum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float vg = vocalGain.getNextValue();
        const float bg = beatGain.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            const float s = vocal.getSample (ch, i) * vg + (beatChannels > 0 ? beatBuffer.getSample (ch, i) * bg : 0.0f);
            vocal.setSample (ch, i, s);
            outSum += (double) (s * s);
        }
    }

    // 6. Auto gain: slowly match the output level to the input level
    const float samples = (float) (n * channels);
    const float alpha = std::exp (-(float) n / (0.4f * (float) sr));
    refPower = alpha * refPower + (1.0f - alpha) * (float) (refSum / samples);
    outPower = alpha * outPower + (1.0f - alpha) * (float) (outSum / samples);
    if (param ("autoGain") < 0.5f)
        autoGain.setTargetValue (1.0f);
    else if (refPower > 1.0e-7f && outPower > 1.0e-10f)
        autoGain.setTargetValue (juce::jlimit (0.25f, 4.0f, std::sqrt (refPower / outPower)));
    autoGainDb = juce::Decibels::gainToDecibels (autoGain.getTargetValue());

    outGain.setTargetValue (juce::Decibels::decibelsToGain (param ("outGain")));
    for (int i = 0; i < n; ++i)
    {
        const float g = autoGain.getNextValue() * outGain.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
            vocal.setSample (ch, i, vocal.getSample (ch, i) * g);
    }
    outputLevelDb = peakDb (vocal, channels, n);
}

//==============================================================================

void VocalSitterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalSitterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocalSitterProcessor::createEditor()
{
    return new VocalSitterEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalSitterProcessor();
}
