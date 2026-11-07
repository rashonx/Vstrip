#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

// 32 bars: the beat's level per band, the part being carved out (red), and the vocal (gold).
class CarveView : public juce::Component
{
public:
    explicit CarveView (VocalSitterProcessor& p) : processor (p) {}
    void update();
    void paint (juce::Graphics&) override;

private:
    VocalSitterProcessor& processor;
    static constexpr int numBands = CarveDisplayData::numBands;
    std::array<float, numBands> beat {}, vocal {}, cut {};
    bool hasVocal = false;
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

    Control* makeKnob (const juce::String& id, const juce::String& name, juce::Colour accent);
    void timerCallback() override;
    void setStereoMode (int index);
    void drawPanel (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, const juce::String& name, float valueDb, juce::Colour colour);

    VocalSitterProcessor& processor;
    KnobLookAndFeel lnf;
    CarveView carveView;

    juce::OwnedArray<Control> controls;
    Control *amount = nullptr, *attack = nullptr, *release = nullptr, *lowCut = nullptr, *highCut = nullptr, *output = nullptr;
    juce::OwnedArray<juce::TextButton> stereoButtons;
    juce::Label stereoLabel, autoGainLabel;
    juce::ToggleButton autoGainButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;

    juce::Rectangle<int> carvePanel, outputPanel, meterArea;
    float beatDb = -100.0f, vocalDb = -100.0f, outDb = -100.0f, autoDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSitterEditor)
};
