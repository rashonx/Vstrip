#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Live picture of the beat (sidechain), the vocal, and what is being carved out of the beat.
class CarveView : public juce::Component
{
public:
    explicit CarveView (VocalSitterProcessor& p) : processor (p) {}
    void update();
    void paint (juce::Graphics&) override;

private:
    VocalSitterProcessor& processor;
    static constexpr int numPoints = CarveDisplayData::numPoints;
    std::array<float, numPoints> beatIn {}, beatOut {}, vocal {}, reduction {};
    bool hasSidechain = false;
};

class VocalSitterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalSitterEditor (VocalSitterProcessor&);
    ~VocalSitterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Switch
    {
        juce::ToggleButton button;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    Control* makeKnob (const juce::String& id, const juce::String& name, juce::Colour accent, bool rotary = true);
    Switch* makeSwitch (const juce::String& id, const juce::String& name, juce::Colour accent);
    void timerCallback() override;
    void drawPanel (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, const juce::String& name, float valueDb, juce::Colour colour);
    void drawEqCurve (juce::Graphics&, juce::Rectangle<float> area);

    VocalSitterProcessor& processor;
    KnobLookAndFeel lnf;
    CarveView carveView;

    juce::OwnedArray<Control> controls;
    juce::OwnedArray<Switch> switches;
    Control *amount = nullptr, *lowCut = nullptr, *highCut = nullptr, *release = nullptr;
    Control *vocalLevel = nullptr, *beatLevel = nullptr, *output = nullptr;
    std::array<Control*, VocalEq::numBands> eqFaders {};
    Switch *beatToOutput = nullptr, *autoGain = nullptr;

    juce::Rectangle<int> carvePanel, eqPanel, mixPanel, eqCurveArea, meterArea;
    float vocalDb = -100.0f, beatDb = -100.0f, outDb = -100.0f, autoDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSitterEditor)
};
