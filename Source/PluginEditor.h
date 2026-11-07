#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// A titled panel with an optional on/off switch and a row of knobs.
class Section : public juce::Component
{
public:
    Section (juce::AudioProcessorValueTreeState& state, const juce::String& title, juce::Colour accent,
             const juce::String& toggleId, const juce::StringArray& paramIds, const juce::StringArray& labels);

    void paint (juce::Graphics&) override;
    void resized() override;
    // Relative width; one-knob panels get extra room so the title fits
    float getWidthUnits() const { return juce::jmax (1.6f, (float) knobs.size()); }

private:
    juce::String title;
    bool hasToggle = false;
    juce::Colour accent;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::Label> names;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
};

class VocalStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalStripEditor (VocalStripProcessor&);
    ~VocalStripEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, const juce::String& name,
                    float valueDb, bool isReduction, juce::Colour colour);

    VocalStripProcessor& processor;
    KnobLookAndFeel lnf;
    juce::OwnedArray<Section> sections;
    std::vector<std::vector<int>> rows;
    float metersLeft = 0.0f;
    float inDb = -100.0f, outDb = -100.0f, compGr = 0.0f, deEssGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalStripEditor)
};
