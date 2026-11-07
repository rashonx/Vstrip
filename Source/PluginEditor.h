#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class KeysLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KeysLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

// One octave drawn large, with the chord each key plays written on it.
class ChordMapView : public juce::Component
{
public:
    explicit ChordMapView (SoulKeysProcessor& p) : processor (p) {}
    void refresh();
    void paint (juce::Graphics&) override;

private:
    SoulKeysProcessor& processor;
    std::array<juce::String, 12> names;
    std::array<juce::String, 12> noteText;
    std::array<bool, 12> held {};
    int last = -1;
};

class SoulKeysEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    explicit SoulKeysEditor (SoulKeysProcessor&);
    ~SoulKeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void loadChordsFrom (const juce::File& file);

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    Knob* makeKnob (const char* id, const char* name);
    void timerCallback() override;
    void refreshChordSetList();
    void setChoice (const char* id, int index);
    void drawPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title);

    SoulKeysProcessor& processor;
    KeysLookAndFeel lnf;
    ChordMapView chordMap;
    juce::MidiKeyboardComponent keyboard;

    juce::ComboBox chordSetBox, soundBox;
    juce::TextButton prevSet { "<" }, nextSet { ">" }, prevSound { "<" }, nextSound { ">" };
    juce::TextButton loadButton { "PULL CHORDS FROM FILE..." };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordSetAttachment;
    juce::OwnedArray<juce::TextButton> extButtons, strumButtons;
    juce::ToggleButton bassToggle, leadingToggle, chordModeToggle;
    juce::Label bassLabel, leadingLabel, chordModeLabel, extLabel, strumLabel, setLabel, soundLabel, status;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bassAtt, leadingAtt, chordModeAtt;

    juce::OwnedArray<Knob> knobs;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
    std::unique_ptr<juce::FileChooser> chooser;
    int listedSets = -1;
    bool listedHasUser = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKeysEditor)
};
