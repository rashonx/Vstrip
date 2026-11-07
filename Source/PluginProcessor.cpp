#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VocalSitterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 2 }, name, range, def,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    addFloat ("amount", "Amount", { 0.0f, 100.0f, 1.0f }, 50.0f, pct);
    addFloat ("attack", "Attack", skewed (1.0f, 100.0f, 10.0f), 10.0f, ms);
    addFloat ("release", "Release", skewed (10.0f, 1000.0f, 150.0f), 150.0f, ms);
    addFloat ("lowCut", "Low Cut", skewed (20.0f, 2000.0f, 200.0f), 20.0f, hz);
    addFloat ("highCut", "High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, hz);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 2 }, "Stereo", StringArray { "Stereo", "Mid", "Side" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autoGain", 2 }, "Auto Gain", false));
    addFloat ("outGain", "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, db);

    return { p.begin(), p.end() };
}

VocalSitterProcessor::VocalSitterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Beat", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Vocal (Sidechain)", juce::AudioChannelSet::stereo(), true)),
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
    carver.prepare (sampleRate);
    setLatencySamples (0);
    vocalKey.assign ((size_t) samplesPerBlock, 0.0f);
    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("outGain")));
    autoGain.reset (sampleRate, 0.2);
    autoGain.setCurrentAndTargetValue (1.0f);
    refPower = outPower = 0.0f;
}

static float peakDb (const juce::AudioBuffer<float>& b, int channels, int samples)
{
    float peak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, samples));
    return juce::Decibels::gainToDecibels (peak, -100.0f);
}

static double meanSquare (const juce::AudioBuffer<float>& b, int channels, int samples)
{
    double sum = 0.0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < samples; ++i)
            sum += (double) (d[i] * d[i]);
    }
    return sum / (double) juce::jmax (1, channels * samples);
}

void VocalSitterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto beat = getBusBuffer (buffer, true, 0);
    const int channels = beat.getNumChannels();
    if (channels == 0 || n == 0)
        return;
    if ((int) vocalKey.size() < n)
        vocalKey.resize ((size_t) n);

    // 1. Vocal sidechain, mixed to mono
    std::fill (vocalKey.begin(), vocalKey.begin() + n, 0.0f);
    float vocalPeak = 0.0f;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        const int sc = side.getNumChannels();
        for (int ch = 0; ch < sc; ++ch)
        {
            const auto* d = side.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                vocalKey[(size_t) i] += d[i] / (float) sc;
        }
        if (sc > 0)
            vocalPeak = peakDb (side, sc, n);
        else
            vocalPeak = -100.0f;
    }
    else
        vocalPeak = -100.0f;
    vocalLevelDb = vocalPeak;
    beatLevelDb = peakDb (beat, channels, n);
    const double refSum = meanSquare (beat, channels, n);

    // 2. Carve the beat
    carver.setParams (param ("amount") * 0.01f, param ("attack"), param ("release"), param ("lowCut"), param ("highCut"),
                      (BandCarver::Stereo) juce::roundToInt (param ("stereo")));
    carver.process (beat, channels, vocalKey.data(), n);

    // 3. Auto gain: slowly bring the carved beat back to its original loudness
    const float alpha = std::exp (-(float) n / (0.4f * (float) sr));
    refPower = alpha * refPower + (1.0f - alpha) * (float) refSum;
    outPower = alpha * outPower + (1.0f - alpha) * (float) meanSquare (beat, channels, n);
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
            beat.setSample (ch, i, beat.getSample (ch, i) * g);
    }
    outputLevelDb = peakDb (beat, channels, n);
}

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
