#include "PluginEditor.h"

namespace
{
    const juce::Colour background (0xff15171c);
    const juce::Colour panel      (0xff20242c);
    const juce::Colour textDim    (0xff9aa3b2);

    constexpr int knobWidth = 84;
    constexpr int rowHeight = 150;
    constexpr int headerHeight = 64;
    constexpr int gap = 10;
}

//==============================================================================

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colour (0xff353b47));
    g.strokePath (track, stroke);
    g.setColour (accent.withMultipliedAlpha (alpha));
    g.strokePath (value, stroke);

    const float knobR = radius - 7.0f;
    g.setColour (juce::Colour (0xff2c313b));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.45f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void KnobLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool onState = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (onState ? accent : juce::Colour (0xff3a404c));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (onState ? juce::Colours::black : textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (onState ? "ON" : "OFF", r, juce::Justification::centred);
}

//==============================================================================

Section::Section (juce::AudioProcessorValueTreeState& state, const juce::String& t, juce::Colour c,
                  const juce::String& toggleId, const juce::StringArray& ids, const juce::StringArray& labels)
    : title (t), accent (c)
{
    for (int i = 0; i < ids.size(); ++i)
    {
        auto* knob = knobs.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobWidth - 6, 18);
        knob->setColour (juce::Slider::rotarySliderFillColourId, accent);
        knob->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
        addAndMakeVisible (knob);
        attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (state, ids[i], *knob));

        auto* name = names.add (new juce::Label ({}, labels[i]));
        name->setJustificationType (juce::Justification::centred);
        name->setFont (juce::FontOptions (12.0f));
        name->setColour (juce::Label::textColourId, textDim);
        addAndMakeVisible (name);
    }

    if (toggleId.isNotEmpty())
    {
        hasToggle = true;
        power.setColour (juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible (power);
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, toggleId, power);
        power.onStateChange = [this]
        {
            for (auto* k : knobs)
                k->setEnabled (power.getToggleState());
        };
        power.onStateChange();
    }
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title, 12, 8, getWidth() - (hasToggle ? 70 : 24), 20, juce::Justification::centredLeft);
}

void Section::resized()
{
    power.setBounds (getWidth() - 52, 8, 42, 20);
    auto area = getLocalBounds().withTrimmedTop (32).reduced (4, 4);
    const int w = area.getWidth() / juce::jmax (1, knobs.size());
    for (int i = 0; i < knobs.size(); ++i)
    {
        auto cell = area.removeFromLeft (w);
        names[i]->setBounds (cell.removeFromTop (16));
        knobs[i]->setBounds (cell);
    }
}

//==============================================================================

VocalStripEditor::VocalStripEditor (VocalStripProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    auto& s = p.apvts;

    auto add = [&] (const char* title, juce::uint32 colour, const char* toggle,
                    juce::StringArray ids, juce::StringArray labels)
    {
        addAndMakeVisible (sections.add (new Section (s, title, juce::Colour (colour), toggle, ids, labels)));
        return sections.size() - 1;
    };

    const int input  = add ("INPUT", 0xff8fa3bf, "", { "inGain", "hpFreq" }, { "Gain", "Low Cut" });
    const int gate   = add ("GATE", 0xff6ec6a8, "gateOn", { "gateThresh" }, { "Threshold" });
    const int eq     = add ("EQ", 0xff5fb3f0, "", { "lowGain", "midGain", "midFreq", "airGain" },
                            { "Body", "Presence", "Pres. Freq", "Air" });
    const int deEss  = add ("DE-ESSER", 0xfff0c05f, "deEssOn", { "deEssThresh", "deEssFreq" }, { "Threshold", "Frequency" });
    const int comp   = add ("COMPRESSOR", 0xfff08a5f, "compOn",
                            { "compThresh", "compRatio", "compAttack", "compRelease", "compMakeup" },
                            { "Threshold", "Ratio", "Attack", "Release", "Makeup" });
    const int sat    = add ("SATURATION", 0xffe0607e, "satOn", { "satDrive", "satMix" }, { "Drive", "Mix" });
    const int delay  = add ("DELAY", 0xffb07ef0, "delayOn", { "delayTime", "delayFeedback", "delayMix" },
                            { "Time", "Feedback", "Mix" });
    const int verb   = add ("REVERB", 0xff7e8cf0, "reverbOn", { "reverbSize", "reverbDamp", "reverbMix" },
                            { "Size", "Damping", "Mix" });
    const int output = add ("OUTPUT", 0xff8fa3bf, "", { "outGain" }, { "Gain" });

    rows = { { input, gate, eq, deEss }, { comp, sat }, { delay, verb, output } };

    float maxUnits = 0.0f;
    for (auto& row : rows)
    {
        float units = 0.0f;
        for (int idx : row)
            units += sections[idx]->getWidthUnits();
        maxUnits = juce::jmax (maxUnits, units);
    }
    setSize (juce::roundToInt (maxUnits * knobWidth) + 5 * gap + 8, headerHeight + (int) rows.size() * (rowHeight + gap) + gap);
    startTimerHz (30);
}

VocalStripEditor::~VocalStripEditor()
{
    setLookAndFeel (nullptr);
}

void VocalStripEditor::timerCallback()
{
    // Meters fall back smoothly instead of flickering
    auto smooth = [] (float current, float target, bool upward)
    {
        return (upward ? target > current : target < current) ? target : current + (target - current) * 0.2f;
    };
    inDb    = smooth (inDb, processor.inputLevelDb.load(), true);
    outDb   = smooth (outDb, processor.outputLevelDb.load(), true);
    compGr  = smooth (compGr, processor.compReductionDb.load(), false);
    deEssGr = smooth (deEssGr, processor.getDeEssReductionDb(), false);
    repaint (getLocalBounds().removeFromTop (headerHeight));
}

void VocalStripEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name,
                                  float valueDb, bool isReduction, juce::Colour colour)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (name, area.removeFromTop (14.0f), juce::Justification::centredLeft);

    auto bar = area.removeFromTop (10.0f);
    g.setColour (juce::Colour (0xff2c313b));
    g.fillRoundedRectangle (bar, 3.0f);

    // Level meters span -60..0 dB; reduction meters span 0..-24 dB, filling from the right
    const float amount = isReduction ? juce::jlimit (0.0f, 1.0f, -valueDb / 24.0f)
                                     : juce::jlimit (0.0f, 1.0f, (valueDb + 60.0f) / 60.0f);
    auto fill = isReduction ? bar.withLeft (bar.getRight() - bar.getWidth() * amount)
                            : bar.withWidth (bar.getWidth() * amount);
    g.setColour (! isReduction && valueDb > -1.0f ? juce::Colours::red : colour);
    g.fillRoundedRectangle (fill, 3.0f);
}

void VocalStripEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (gap + 4, 10).toFloat();
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VocalStrip", header.removeFromLeft (170.0f), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("vocal channel strip", header.removeFromLeft (130.0f), juce::Justification::centredLeft);

    const float meterW = (header.getWidth() - 3 * gap) / 4.0f;
    auto meter = [&] { auto m = header.removeFromLeft (meterW); header.removeFromLeft ((float) gap); return m; };
    drawMeter (g, meter(), "IN  " + juce::String (inDb, 1) + " dB", inDb, false, juce::Colour (0xff6ec6a8));
    drawMeter (g, meter(), "DE-ESS  " + juce::String (deEssGr, 1) + " dB", deEssGr, true, juce::Colour (0xfff0c05f));
    drawMeter (g, meter(), "COMP  " + juce::String (compGr, 1) + " dB", compGr, true, juce::Colour (0xfff08a5f));
    drawMeter (g, meter(), "OUT  " + juce::String (outDb, 1) + " dB", outDb, false, juce::Colour (0xff6ec6a8));
}

void VocalStripEditor::resized()
{
    auto area = getLocalBounds().reduced (gap);
    area.removeFromTop (headerHeight - gap);

    for (auto& row : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        area.removeFromTop (gap);

        float totalUnits = 0.0f;
        for (int idx : row)
            totalUnits += sections[idx]->getWidthUnits();

        const int available = line.getWidth() - gap * ((int) row.size() - 1);
        for (size_t i = 0; i < row.size(); ++i)
        {
            auto* section = sections[row[i]];
            const bool last = i + 1 == row.size();
            const int w = last ? line.getWidth() : juce::roundToInt ((float) available * section->getWidthUnits() / totalUnits);
            section->setBounds (line.removeFromLeft (w));
            line.removeFromLeft (gap);
        }
    }
}
