#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KeysSkins.h"

class KeysLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setSkin (const KeysSkin& s);
    const KeysSkin& skin() const { return *current; }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;

private:
    const KeysSkin* current = &KeysSkin::all().front();
};

// One octave drawn large, with the chord each key plays written on it.
class ChordMapView : public juce::Component
{
public:
    ChordMapView (SoulKeysProcessor& p, int zoneIndex) : processor (p), zone (zoneIndex) {}
    void setSkin (const KeysSkin& s) { skin = &s; repaint(); }
    void setSuggestColour (juce::Colour c) { suggestColour = c; repaint(); }
    const std::array<bool, 12>& getSuggested() const { return suggested; }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    SoulKeysProcessor& processor;
    const int zone;
    juce::Colour suggestColour { 0xff3ddc97 };
    const KeysSkin* skin = &KeysSkin::all().front();
    std::array<juce::String, 12> names, noteText;
    std::array<bool, 12> held {}, suggested {};
    juce::String suggestionText;
    int last = -1;
};

// The playable keyboard, tinted to show which keys belong to zone A and zone B.
class ZoneKeyboard : public juce::MidiKeyboardComponent
{
public:
    ZoneKeyboard (juce::MidiKeyboardState& s, SoulKeysProcessor& p) : MidiKeyboardComponent (s, horizontalKeyboard), processor (p) {}
    void setSkin (const KeysSkin& s) { skin = &s; repaint(); }
    void setSuggestions (const std::array<bool, 12>& zoneA, const std::array<bool, 12>& zoneB, juce::Colour colour)
    {
        if (suggested[0] == zoneA && suggested[1] == zoneB && colour == suggestColour) return;
        suggested[0] = zoneA; suggested[1] = zoneB; suggestColour = colour;
        repaint();
    }

protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    juce::Colour zoneColour (int note) const;
    bool isSuggested (int note) const;
    SoulKeysProcessor& processor;
    const KeysSkin* skin = &KeysSkin::all().front();
    std::array<std::array<bool, 12>, 2> suggested {};
    juce::Colour suggestColour { 0xff3ddc97 };
};

// A small colour square you can click
class SwatchButton : public juce::Button
{
public:
    explicit SwatchButton (juce::Colour c) : juce::Button ({}), colour (c) {}
    juce::Colour colour;
    bool selected = false;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (r, 4.0f);
        if (selected || over)
        {
            g.setColour (selected ? juce::Colours::white : juce::Colours::white.withAlpha (0.5f));
            g.drawRoundedRectangle (r.expanded (1.5f), 5.0f, selected ? 2.5f : 1.5f);
        }
    }
};

// A button you can drag out of the plugin: drops the recorded take as a MIDI file into the DAW.
class DragMidiButton : public juce::TextButton
{
public:
    DragMidiButton() : juce::TextButton ("DRAG MIDI") {}
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { dragging = false; juce::TextButton::mouseUp (e); }

private:
    bool dragging = false;
};

// Everything you see, drawn at a fixed design size; SoulKeysEditor scales it to the window size.
class SoulKeysView : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer,
                       private juce::ValueTree::Listener
{
public:
    explicit SoulKeysView (SoulKeysProcessor&);
    ~SoulKeysView() override;

    // Window size (scale) handling
    std::function<void (float)> onScaleRequest;
    float savedScale() const;
    void rememberScale (float scale);

    void setSkin (int index);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void loadChordsFrom (const juce::File& file, int zone);

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct ZoneRow
    {
        juce::Label title;
        juce::Label setCaption, keyCaption, scaleCaption, extCaption, octCaption;
        juce::ComboBox setBox, keyBox, scaleBox;
        juce::TextButton prev { "<" }, next { ">" }, octDown { "-" }, octUp { "+" }, load { "FROM FILE..." };
        juce::TextButton savePreset { "SAVE PRESET" }, importPreset { "IMPORT" };
        juce::Label octValue;
        juce::OwnedArray<juce::TextButton> ext;
        std::unique_ptr<CA> setAtt, keyAtt, scaleAtt;
        std::unique_ptr<ChordMapView> map;
        juce::Rectangle<int> area;
        int listed = -1;
        bool listedUser = false;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SA> attachment;
        const char* base = "";
    };

    void buildZoneRow (int zone);
    void layoutZoneRow (int zone, int y);
    void refreshSetList (int zone, bool force);
    void bindSoundKnobs();
    void setParam (const juce::String& id, float value);
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { lastEdit = juce::Time::getMillisecondCounterHiRes(); editPending = true; }
    void applySkinColours();
    void drawPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour stripe);
    void showStatus (const juce::String& text);
    void askToSavePreset (int zone);
    juce::File writeTake (const juce::File& file) const;
    juce::File takeFileForDrag();
    void finishTake();
    juce::String takeSummary() const;

    SoulKeysProcessor& processor;
    KeysLookAndFeel lnf;
    std::array<ZoneRow, SoulKeysProcessor::numZones> rows;
    ZoneKeyboard keyboard;
    std::unique_ptr<juce::PropertiesFile> settings;

    // Header and split bar
    juce::ComboBox skinBox, splitBox;
    juce::Label skinCaption, splitCaption, splitAtCaption, splitHint;
    juce::ToggleButton splitToggle;
    std::unique_ptr<BA> splitAtt;
    juce::TextButton swapButton { "SWAP A / B" };

    // Sound panel (shows zone A or B)
    juce::TextButton soundTabA { "ZONE A SOUND" }, soundTabB { "ZONE B SOUND" };
    juce::ComboBox soundBox;
    juce::TextButton prevSound { "<" }, nextSound { ">" };
    juce::OwnedArray<Knob> soundKnobs;
    int soundZone = 0;

    // Play panel
    juce::OwnedArray<Knob> playKnobs;
    juce::OwnedArray<juce::TextButton> strumButtons;
    juce::ToggleButton bassToggle, leadingToggle, chordModeToggle;
    juce::Label strumLabel, bassLabel, leadingLabel, chordModeLabel, status;
    std::unique_ptr<BA> bassAtt, leadingAtt, chordModeAtt;

    // Undo / redo and compare
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" }, compare1 { "1" }, compare2 { "2" }, copyCompare { "COPY 1 > 2" };
    juce::Label compareCaption;
    double lastEdit = 0.0;
    bool editPending = false;

    // Voicing & MIDI panel
    juce::OwnedArray<juce::TextButton> voicingButtons;
    juce::ComboBox topNoteBox, midiOutBox;
    juce::ToggleButton suggestToggle;
    juce::OwnedArray<SwatchButton> swatches;
    juce::TextButton customColour { "Custom" };
    juce::Colour suggestColour { 0xff3ddc97 };
    void setSuggestColour (juce::Colour c);
    juce::Label voicingCaption, topNoteCaption, suggestCaption, midiOutCaption, midiHint;
    std::unique_ptr<CA> topNoteAtt, midiOutAtt;
    std::unique_ptr<BA> suggestAtt;

    // Recorder
    juce::TextButton recButton { "REC" }, stopButton { "STOP" }, clearButton { "CLEAR" }, saveMidiButton { "SAVE MIDI..." };
    DragMidiButton dragButton;
    juce::Label recCaption, takeInfo;
    std::vector<SoulKeysProcessor::RecEvent> take;
    double takeBpm = 120.0;
    juce::File lastDragFile;
    double statusUntil = 0.0;

    std::vector<std::tuple<juce::Rectangle<int>, juce::String, bool>> panels; // area, title, uses zone B colour
    std::unique_ptr<juce::FileChooser> chooser;
    int skinIndex = 0;

    juce::ComboBox sizeBox;
    juce::Label sizeCaption;
    float currentScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKeysView)
};

// The plugin window: holds the view and scales it to whatever size the window is dragged to.
class SoulKeysEditor : public juce::AudioProcessorEditor
{
public:
    explicit SoulKeysEditor (SoulKeysProcessor&);
    void resized() override;
    void setScale (float scale);
    SoulKeysView& getView() { return view; }

private:
    SoulKeysView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKeysEditor)
};
