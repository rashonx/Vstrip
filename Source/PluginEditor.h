#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Skins.h"

class SitterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setSkin (const Skin& s);
    const Skin& skin() const { return *current; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;

private:
    const Skin* current = &Skin::all().front();
};

// Live picture of the 32 bands, drawn three ways depending on the layout.
class CarveView : public juce::Component
{
public:
    enum class Style { bars, ring, sideways };

    explicit CarveView (VocalSitterProcessor& p) : processor (p) {}
    void setLook (Style s, const Skin& sk) { style = s; skin = &sk; repaint(); }
    void setCompact (bool c) { compact = c; }
    void update();
    void paint (juce::Graphics&) override;

private:
    void paintBars (juce::Graphics&);
    void paintRing (juce::Graphics&);
    void paintSideways (juce::Graphics&);
    void paintRoutingLabel (juce::Graphics&, juce::Rectangle<float> area);

    VocalSitterProcessor& processor;
    const Skin* skin = &Skin::all().front();
    Style style = Style::bars;
    bool compact = false;
    static constexpr int numBands = CarveDisplayData::numBands;
    std::array<float, numBands> beat {}, vocal {}, cut {};
    bool hasVocal = false, listening = false, connected = false;
};

// Analog needle meter for the Rack layout.
class VuMeter : public juce::Component
{
public:
    void setLook (const Skin& s, const juce::String& title) { skin = &s; label = title; repaint(); }
    void setValue (float normalised) { value += (normalised - value) * 0.35f; repaint(); }
    void paint (juce::Graphics&) override;

private:
    const Skin* skin = &Skin::all().front();
    juce::String label;
    float value = 0.0f;
};

class VocalSitterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    enum Layout { standard = 0, compactLayout, bigKnob, rack, strip, numLayouts };
    static juce::StringArray layoutNames() { return { "Standard", "Compact", "Big Knob", "Rack Unit", "Channel Strip" }; }

    explicit VocalSitterEditor (VocalSitterProcessor&);
    ~VocalSitterEditor() override;

    void setLook (int layoutIndex, int skinIndex);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Control* makeKnob (const juce::String& id, const juce::String& name);
    void timerCallback() override;
    void setStereoMode (int index);
    void showLookMenu();
    void applySkinColours();
    void place (Control* c, juce::Rectangle<int> r, int textBoxWidth = 80);
    void placeStereoButtons (juce::Rectangle<int> row);

    void paintTitle (juce::Graphics&, juce::Point<float> at, float size, bool raised);
    void paintLogo (juce::Graphics&, juce::Rectangle<float> area);
    void paintMeters (juce::Graphics&, juce::Rectangle<float> area, bool twoByTwo);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, const juce::String& name, float valueDb, juce::Colour colour, bool centred = false);
    void paintLed (juce::Graphics&, juce::Point<float> at, juce::Colour colour, bool on, const juce::String& text);

    VocalSitterProcessor& processor;
    SitterLookAndFeel lnf;
    CarveView carveView;
    VuMeter beatVu, carveVu;
    std::unique_ptr<juce::PropertiesFile> settings;

    juce::OwnedArray<Control> controls;
    Control *amount = nullptr, *attack = nullptr, *release = nullptr, *lowCut = nullptr, *highCut = nullptr, *output = nullptr;
    juce::OwnedArray<juce::TextButton> stereoButtons;
    juce::Label stereoLabel, autoGainLabel;
    juce::ToggleButton autoGainButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    juce::TextButton listenButton { "LISTEN TO CARVED PART" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAttachment;
    juce::TextButton lookButton { "LOOK" };

    int layout = standard, skinIndex = 0;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels; // panel areas and titles for the current layout
    juce::Rectangle<int> meterArea, titleArea, logoArea;
    float beatDb = -100.0f, vocalDb = -100.0f, outDb = -100.0f, autoDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSitterEditor)
};
