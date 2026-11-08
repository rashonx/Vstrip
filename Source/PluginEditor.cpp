#include "PluginEditor.h"
#include "Artwork.h"
#include "ChordExtractor.h"
#include "PresetFile.h"
#include <BinaryData.h>

namespace
{
    constexpr int W = 1160, H = 920;

    juce::Colour contrast (juce::Colour c) { return c.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff111111) : juce::Colours::white; }
}

//==============================================================================
// Look and feel

void KeysLookAndFeel::setSkin (const KeysSkin& s)
{
    current = &s;
    setColour (juce::ComboBox::backgroundColourId, s.track);
    setColour (juce::ComboBox::textColourId, s.text);
    setColour (juce::ComboBox::outlineColourId, s.panelEdge);
    setColour (juce::ComboBox::arrowColourId, s.accent);
    setColour (juce::PopupMenu::backgroundColourId, s.panel);
    setColour (juce::PopupMenu::textColourId, s.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, s.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, contrast (s.accent));
    setColour (juce::TextButton::textColourOffId, s.text);
    setColour (juce::TextButton::textColourOnId, contrast (s.accent));
    setColour (juce::Label::textColourId, s.dim);
}

void KeysLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
{
    const auto& s = *current;
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    juce::Path t, v;
    t.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    v.addCentredArc (c.x, c.y, r, r, 0.0f, a0, angle, true);
    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (s.track);
    g.strokePath (t, stroke);
    g.setColour (sl.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (v, stroke);
    const float capR = r - 7.0f;
    g.setGradientFill (juce::ColourGradient (s.capLight, c.x - capR * 0.4f, c.y - capR * 0.5f, s.capDark, c.x + capR, c.y + capR, true));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -capR + 3.0f, 3.0f, capR * 0.5f, 1.5f);
    g.setColour (s.pointer);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void KeysLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto& s = *current;
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? s.accent : s.track);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (on ? contrast (s.accent) : s.dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
}

void KeysLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    const auto& s = *current;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool zoneB = b.getProperties()["zoneB"];
    g.setColour (b.getToggleState() ? (zoneB ? s.accent2 : s.accent) : s.track);
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

//==============================================================================
// Chord map

void ChordMapView::refresh()
{
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto c = processor.chordForKey (pc, zone);
        names[(size_t) pc] = c.name;
        juce::StringArray notes;
        for (int n : c.notes)
            notes.add (juce::MidiMessage::getMidiNoteName (n, false, true, 4));
        noteText[(size_t) pc] = notes.joinIntoString (" ");
        held[(size_t) pc] = processor.keyHeld[zone][pc].load();
    }
    last = processor.lastKeyPc[zone].load();
    repaint();
}

void ChordMapView::paint (juce::Graphics& g)
{
    const auto& s = *skin;
    const auto zoneColour = zone == 0 ? s.accent : s.accent2;
    auto b = getLocalBounds().toFloat();
    g.setColour (s.display);
    g.fillRoundedRectangle (b, 10.0f);

    auto now = b.removeFromRight (230.0f).reduced (8.0f);
    g.setColour (s.panel);
    g.fillRoundedRectangle (now, 8.0f);
    g.setColour (s.dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("NOW PLAYING", now.reduced (12.0f, 8.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    if (last >= 0)
    {
        g.setColour (zoneColour);
        g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
        g.drawFittedText (names[(size_t) last], now.reduced (12.0f).withTrimmedTop (22.0f).withHeight (50.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour (s.text);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (noteText[(size_t) last], now.reduced (12.0f).withTrimmedTop (76.0f).toNearestInt(), juce::Justification::topLeft, 3);
    }
    else
    {
        g.setColour (s.text.withAlpha (0.6f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText ("White keys: the chords.\nBlack keys: related chords that lead into the next one.",
                          now.reduced (12.0f).withTrimmedTop (24.0f).toNearestInt(), juce::Justification::topLeft, 4);
    }

    auto keys = b.reduced (10.0f, 8.0f);
    const float whiteW = keys.getWidth() / 7.0f;
    for (size_t i = 0; i < 7; ++i)
    {
        const int pc = ChordPresets::whitePcs[i];
        auto r = juce::Rectangle<float> (keys.getX() + whiteW * (float) i, keys.getY(), whiteW, keys.getHeight()).reduced (2.0f, 0.0f);
        const bool on = held[(size_t) pc];
        g.setGradientFill (juce::ColourGradient (on ? zoneColour.brighter (0.4f) : s.whiteTop, 0.0f, r.getY(), on ? zoneColour : s.whiteBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? contrast (zoneColour) : s.whiteText);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawFittedText (names[(size_t) pc], r.reduced (5.0f).withTrimmedTop (r.getHeight() * 0.58f).toNearestInt(), juce::Justification::centredTop, 2);
        g.setColour (s.whiteText.withAlpha (0.5f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (ChordQ::noteName (pc), r.reduced (5.0f).removeFromBottom (12.0f), juce::Justification::centred);
    }
    const float blackW = whiteW * 0.64f, blackH = keys.getHeight() * 0.54f;
    const std::array<float, 5> afterWhite { 1, 2, 4, 5, 6 };
    for (size_t i = 0; i < 5; ++i)
    {
        const int pc = ChordPresets::blackPcs[i];
        auto r = juce::Rectangle<float> (keys.getX() + whiteW * afterWhite[i] - blackW * 0.5f, keys.getY(), blackW, blackH);
        const bool on = held[(size_t) pc];
        g.setGradientFill (juce::ColourGradient (on ? zoneColour.brighter (0.3f) : s.blackTop, 0.0f, r.getY(), on ? zoneColour : s.blackBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? contrast (zoneColour) : s.blackText);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawFittedText (names[(size_t) pc], r.reduced (3.0f).withTrimmedTop (r.getHeight() * 0.4f).toNearestInt(), juce::Justification::centredTop, 3);
    }
}

//==============================================================================
// Keyboard with zone colours

juce::Colour ZoneKeyboard::zoneColour (int note) const
{
    if (! processor.splitOn()) return skin->accent;
    return note >= processor.splitPoint() ? skin->accent2 : skin->accent;
}

void ZoneKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    const auto zc = zoneColour (note);
    g.setColour (juce::Colours::white);
    g.fillRect (area);
    g.setColour (zc.withAlpha (processor.splitOn() ? 0.16f : 0.06f));
    g.fillRect (area.withTrimmedTop (area.getHeight() * 0.7f));
    if (isDown || isOver)
    {
        g.setColour (isDown ? zc : zc.withAlpha (0.3f));
        g.fillRect (area);
    }
    g.setColour (lineColour);
    g.fillRect (area.withWidth (1.0f));
    if (note % 12 == 0)
    {
        g.setColour (textColour);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (getWhiteNoteText (note), area.reduced (2.0f).removeFromBottom (14.0f), juce::Justification::centred);
    }
    if (processor.splitOn() && note == processor.splitPoint())
    {
        g.setColour (skin->accent2);
        g.fillRect (area.withWidth (3.0f));
    }
}

void ZoneKeyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    const auto zc = zoneColour (note);
    g.setColour (isDown ? zc : juce::Colour (0xff1c1c1e).interpolatedWith (zc, isOver ? 0.4f : (processor.splitOn() ? 0.18f : 0.06f)));
    g.fillRect (area);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (area.reduced (area.getWidth() * 0.15f, 0.0f).withHeight (area.getHeight() * 0.85f).withTrimmedTop (2.0f).withWidth (2.0f));
}

//==============================================================================
// Drag a recorded take out to the DAW

void DragMidiButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile)
        return;
    const auto file = makeFile();
    if (! file.existsAsFile())
        return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [this] { dragging = false; });
}

//==============================================================================
// Editor

SoulKeysEditor::SoulKeysEditor (SoulKeysProcessor& p)
    : AudioProcessorEditor (p), processor (p), keyboard (p.keyboardState, p)
{
    setLookAndFeel (&lnf);

    // Skin: saved with the project, otherwise the last one picked anywhere
    juce::PropertiesFile::Options options;
    options.applicationName = "Rashon Soul Keys";
    options.filenameSuffix = "settings";
    options.folderName = "Dodge City Plugins";
    options.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (options);

    auto caption = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (l);
    };

    caption (skinCaption, "SKIN");
    for (size_t i = 0; i < KeysSkin::all().size(); ++i)
        skinBox.addItem (KeysSkin::all()[i].name, (int) i + 1);
    skinBox.onChange = [this] { setSkin (skinBox.getSelectedItemIndex()); };
    addAndMakeVisible (skinBox);

    // Split bar
    caption (splitCaption, "SPLIT MODE");
    addAndMakeVisible (splitToggle);
    splitAtt = std::make_unique<BA> (processor.apvts, "split", splitToggle);
    caption (splitAtCaption, "Split at");
    for (int n = 36; n <= 84; ++n)
        splitBox.addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 4), n - 35);
    splitBox.onChange = [this] { setParam ("splitPoint", (float) (splitBox.getSelectedItemIndex() + 36)); };
    addAndMakeVisible (splitBox);
    swapButton.onClick = [this] { processor.swapZones(); refreshSetList (0, true); refreshSetList (1, true); };
    addAndMakeVisible (swapButton);
    splitHint.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (splitHint);

    for (int z = 0; z < SoulKeysProcessor::numZones; ++z)
        buildZoneRow (z);

    // Sound panel
    soundTabB.getProperties().set ("zoneB", true);
    soundTabA.onClick = [this] { soundZone = 0; bindSoundKnobs(); };
    soundTabB.onClick = [this] { soundZone = 1; bindSoundKnobs(); };
    addAndMakeVisible (soundTabA);
    addAndMakeVisible (soundTabB);
    for (size_t i = 0; i < soundPresets().size(); ++i)
        soundBox.addItem (soundPresets()[i].name, (int) i + 1);
    soundBox.onChange = [this] { processor.applySoundPreset (soundZone, soundBox.getSelectedItemIndex()); };
    addAndMakeVisible (soundBox);
    auto stepSound = [this] (int d) { const int n = (int) soundPresets().size(); soundBox.setSelectedItemIndex ((soundBox.getSelectedItemIndex() + d + n) % n); };
    prevSound.onClick = [stepSound] { stepSound (-1); };
    nextSound.onClick = [stepSound] { stepSound (1); };
    addAndMakeVisible (prevSound);
    addAndMakeVisible (nextSound);

    auto makeKnob = [this] (juce::OwnedArray<Knob>& list, const char* base, const char* name)
    {
        auto* k = list.add (new Knob());
        k->base = base;
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible (k->slider);
        k->label.setText (name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (11.5f));
        addAndMakeVisible (k->label);
    };
    for (auto [base, name] : { std::pair { "bell", "Bell" }, { "bark", "Bark" }, { "decay", "Decay" }, { "tone", "Tone" },
                               { "tremRate", "Trem Rate" }, { "tremDepth", "Tremolo" }, { "chorus", "Chorus" }, { "wobble", "Wobble" },
                               { "age", "Age" }, { "hiss", "Hiss" }, { "reverb", "Reverb" }, { "volume", "Volume" } })
        makeKnob (soundKnobs, base, name);
    bindSoundKnobs();

    // Play panel
    makeKnob (playKnobs, "strum", "Strum");
    makeKnob (playKnobs, "humanize", "Humanize");
    for (auto* k : playKnobs)
        k->attachment = std::make_unique<SA> (processor.apvts, k->base, k->slider);
    caption (strumLabel, "Strum direction");
    for (auto [i, t] : { std::pair { 0, "Up" }, { 1, "Down" }, { 2, "Alt" } })
    {
        auto* b = strumButtons.add (new juce::TextButton (t));
        b->onClick = [this, i = i] { setParam ("strumDir", (float) i); };
        addAndMakeVisible (b);
    }
    caption (bassLabel, "Bass note");
    caption (leadingLabel, "Voice leading");
    caption (chordModeLabel, "Chord mode");
    for (auto* t : { &bassToggle, &leadingToggle, &chordModeToggle })
        addAndMakeVisible (*t);
    bassAtt = std::make_unique<BA> (processor.apvts, "bass", bassToggle);
    leadingAtt = std::make_unique<BA> (processor.apvts, "voiceLeading", leadingToggle);
    chordModeAtt = std::make_unique<BA> (processor.apvts, "chordMode", chordModeToggle);
    // Recorder
    recCaption.setText ("RECORD WHAT YOU PLAY", juce::dontSendNotification);
    recCaption.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (recCaption);
    takeInfo.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (takeInfo);
    recButton.getProperties().set ("zoneB", true); // shows in the zone B (second) colour while recording
    recButton.onClick = [this]
    {
        if (processor.isRecording()) { processor.stopRecording(); finishTake(); }
        else
        {
            take.clear();
            takeBpm = processor.hostBpm();
            processor.startRecording();
            showStatus ("Recording... play some chords, then press STOP.");
        }
    };
    stopButton.onClick = [this] { if (processor.isRecording()) { processor.stopRecording(); finishTake(); } };
    clearButton.onClick = [this] { processor.stopRecording(); take.clear(); showStatus ("Take cleared."); };
    dragButton.makeFile = [this] { return takeFileForDrag(); };
    dragButton.setTooltip ("Drag this onto a MIDI track or the piano roll in your DAW");
    dragButton.onClick = [this] { showStatus (take.empty() ? "Record something first." : "Hold the mouse on DRAG MIDI and drag it onto a MIDI track."); };
    saveMidiButton.onClick = [this]
    {
        if (take.empty()) { showStatus ("Record something first."); return; }
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Dodge City Plugins").getChildFile ("Soul Keys").getChildFile ("Takes");
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save the take as a MIDI file", dir.getChildFile ("Soul Keys take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M") + ".mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  writeTake (f.withFileExtension ("mid"));
                                  showStatus ("Saved " + f.withFileExtension ("mid").getFileName());
                              });
    };
    for (auto* b : std::initializer_list<juce::Component*> { &recButton, &stopButton, &clearButton, &dragButton, &saveMidiButton })
        addAndMakeVisible (b);

    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    const auto& state = processor.apvts.state;
    setSkin (state.hasProperty ("skin") ? (int) state.getProperty ("skin") : settings->getIntValue ("skin", 0));
    setSize (W, H);
    timerCallback();
    startTimerHz (20);
}

SoulKeysEditor::~SoulKeysEditor()
{
    setLookAndFeel (nullptr);
}

void SoulKeysEditor::showStatus (const juce::String& text)
{
    splitHint.setText (text, juce::dontSendNotification);
    statusUntil = juce::Time::getMillisecondCounterHiRes() + 7000.0;
}

void SoulKeysEditor::setParam (const juce::String& id, float value)
{
    auto* p = processor.apvts.getParameter (id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (value));
    p->endChangeGesture();
}

void SoulKeysEditor::buildZoneRow (int z)
{
    auto& r = rows[(size_t) z];
    r.title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (r.title);
    for (auto [l, t] : { std::pair { &r.setCaption, "CHORD SET" }, { &r.keyCaption, "KEY" }, { &r.scaleCaption, "SCALE" },
                         { &r.extCaption, "EXTENSIONS" }, { &r.octCaption, "OCTAVE" } })
    {
        l->setText (t, juce::dontSendNotification);
        l->setFont (juce::FontOptions (10.5f));
        addAndMakeVisible (*l);
    }

    refreshSetList (z, true);
    // Connected by hand: the list length (presets + My Presets) differs from the parameter's range
    r.setBox.onChange = [this, z]
    {
        const int index = rows[(size_t) z].setBox.getSelectedItemIndex();
        if (index >= 0) setParam (SoulKeysProcessor::pid (z, "chordSet"), (float) index);
    };
    addAndMakeVisible (r.setBox);
    auto step = [this, z] (int d)
    {
        auto& row = rows[(size_t) z];
        const int n = processor.numChordSets() - (processor.hasUserChords (z) ? 0 : 1);
        row.setBox.setSelectedItemIndex ((row.setBox.getSelectedItemIndex() + d + n) % n);
    };
    r.prev.onClick = [step] { step (-1); };
    r.next.onClick = [step] { step (1); };
    addAndMakeVisible (r.prev);
    addAndMakeVisible (r.next);

    r.keyBox.addItem ("Original", 1);
    for (int pc = 0; pc < 12; ++pc) r.keyBox.addItem (ChordQ::noteName (pc), pc + 2);
    r.keyAtt = std::make_unique<CA> (processor.apvts, SoulKeysProcessor::pid (z, "key"), r.keyBox);
    r.keyBox.setTooltip ("Moves the chord set to this key (Key & Scale chords are built in this key)");
    addAndMakeVisible (r.keyBox);

    r.scaleBox.addItemList (ScaleChords::scaleNames(), 1);
    r.scaleAtt = std::make_unique<CA> (processor.apvts, SoulKeysProcessor::pid (z, "scale"), r.scaleBox);
    r.scaleBox.setTooltip ("Scale used by the \"Key & Scale chords\" chord set");
    addAndMakeVisible (r.scaleBox);

    for (auto [i, t] : { std::pair { 0, "Preset" }, { 1, "7th" }, { 2, "9th" }, { 3, "11th" } })
    {
        auto* b = r.ext.add (new juce::TextButton (t));
        b->getProperties().set ("zoneB", z == 1);
        b->onClick = [this, z, i = i] { setParam (SoulKeysProcessor::pid (z, "extensions"), (float) i); };
        addAndMakeVisible (b);
    }

    auto octaveStep = [this, z] (int d)
    {
        const int now = juce::roundToInt (processor.apvts.getRawParameterValue (SoulKeysProcessor::pid (z, "octave"))->load());
        setParam (SoulKeysProcessor::pid (z, "octave"), (float) juce::jlimit (-2, 2, now + d));
    };
    r.octDown.onClick = [octaveStep] { octaveStep (-1); };
    r.octUp.onClick = [octaveStep] { octaveStep (1); };
    r.octValue.setJustificationType (juce::Justification::centred);
    r.octValue.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (r.octDown);
    addAndMakeVisible (r.octUp);
    addAndMakeVisible (r.octValue);

    r.load.onClick = [this, z]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pull chords from a MIDI or audio file", juce::File(), ChordExtractor::supportedPatterns());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, z] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadChordsFrom (fc.getResult(), z); });
    };
    addAndMakeVisible (r.load);

    r.savePreset.setTooltip ("Save this zone's chords (including chords pulled from a file) to My Presets");
    r.savePreset.onClick = [this, z] { askToSavePreset (z); };
    r.importPreset.setTooltip ("Import a .soulkeys preset file into My Presets");
    r.importPreset.onClick = [this, z]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a Soul Keys preset", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"), "*" + PresetFile::extension());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, z] (const juce::FileChooser& fc)
                              {
                                  int ok = 0;
                                  juce::String lastError;
                                  for (auto& f : fc.getResults())
                                  {
                                      const auto error = processor.importPreset (f, z);
                                      if (error.isEmpty()) ++ok; else lastError = f.getFileName() + ": " + error;
                                  }
                                  refreshSetList (0, true);
                                  refreshSetList (1, true);
                                  if (ok > 0) showStatus ("Imported " + juce::String (ok) + " preset(s) into My Presets." + (lastError.isNotEmpty() ? "  " + lastError : ""));
                                  else if (lastError.isNotEmpty()) showStatus (lastError);
                              });
    };
    addAndMakeVisible (r.savePreset);
    addAndMakeVisible (r.importPreset);

    r.map = std::make_unique<ChordMapView> (processor, z);
    addAndMakeVisible (*r.map);
}

void SoulKeysEditor::askToSavePreset (int z)
{
    auto current = processor.chordSetName (z, juce::roundToInt (processor.apvts.getRawParameterValue (SoulKeysProcessor::pid (z, "chordSet"))->load()));
    current = current.fromFirstOccurrenceOf ("From file: ", false, false).isNotEmpty() ? current.fromFirstOccurrenceOf ("From file: ", false, false)
            : current.startsWith ("My: ") ? current.substring (4)
            : current.retainCharacters ("0123456789").length() >= 2 && current[2] == ' ' ? current.substring (4).upToFirstOccurrenceOf ("  (", false, false)
            : current;
    auto* w = new juce::AlertWindow ("Save as a preset", "Name for these chords (saved in My Presets):", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", current.trim() + " (mine)");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<SoulKeysEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, z] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) { safe->showStatus ("Give the preset a name."); return; }
        const auto error = safe->processor.saveZoneAsPreset (z, name);
        safe->refreshSetList (0, true);
        safe->refreshSetList (1, true);
        safe->showStatus (error.isEmpty() ? "Saved \"" + name + "\" to My Presets (" + PresetFile::folder().getFullPathName() + ")." : error);
    }), true);
}

//==============================================================================
// Recorder

void SoulKeysEditor::finishTake()
{
    processor.drainRecording (take);
    showStatus (take.empty() ? "Nothing was recorded." : "Take ready: " + takeSummary() + ". Drag DRAG MIDI onto a MIDI track, or SAVE MIDI.");
}

juce::String SoulKeysEditor::takeSummary() const
{
    int notes = 0;
    double first = 1.0e9, last = 0.0;
    for (auto& e : take)
        if (e.on) { ++notes; first = std::min (first, e.time); last = std::max (last, e.time); }
    if (notes == 0) return "empty";
    const int secs = (int) std::ceil (last - first);
    return juce::String (notes) + " notes, " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2)
           + " at " + juce::String (juce::roundToInt (takeBpm)) + " BPM";
}

juce::File SoulKeysEditor::writeTake (const juce::File& file) const
{
    if (take.empty()) return {};
    auto events = take;
    std::stable_sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.time < b.time; });
    double start = 0.0;
    for (auto& e : events) if (e.on) { start = e.time; break; } // the take starts at the first note
    const int ppq = 960;
    const double ticksPerSecond = takeBpm / 60.0 * ppq;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / takeBpm)), 0.0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Rashon Soul Keys take"), 0.0);
    std::array<int, 128> open {};
    double end = 0.0;
    for (auto& e : events)
    {
        const double t = std::max (0.0, (e.time - start) * ticksPerSecond);
        end = std::max (end, t);
        if (e.on) { seq.addEvent (juce::MidiMessage::noteOn (1, e.note, juce::jlimit (0.05f, 1.0f, e.velocity)), t); ++open[(size_t) e.note]; }
        else if (open[(size_t) e.note] > 0) { seq.addEvent (juce::MidiMessage::noteOff (1, e.note), t); --open[(size_t) e.note]; }
    }
    for (int n = 0; n < 128; ++n) // close anything still held when recording stopped
        while (open[(size_t) n]-- > 0) seq.addEvent (juce::MidiMessage::noteOff (1, n), end + ppq / 4);
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! mf.writeTo (out, 1)) return {};
    return file;
}

juce::File SoulKeysEditor::takeFileForDrag()
{
    if (processor.isRecording()) { processor.stopRecording(); finishTake(); }
    if (take.empty()) { showStatus ("Record something first."); return {}; }
    lastDragFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("Soul Keys take " + juce::Time::getCurrentTime().formatted ("%H-%M-%S") + ".mid");
    return writeTake (lastDragFile);
}

void SoulKeysEditor::refreshSetList (int z, bool force)
{
    auto& r = rows[(size_t) z];
    const bool hasUser = processor.hasUserChords (z);
    if (! force && r.listed == processor.numChordSets() && r.listedUser == hasUser)
        return;
    // Separator text for My Presets is part of the names ("My: ...")
    const int selected = r.setBox.getSelectedItemIndex();
    r.setBox.clear (juce::dontSendNotification);
    for (int i = 0; i < processor.numChordSets(); ++i)
        r.setBox.addItem (processor.chordSetName (z, i), i + 1);
    r.setBox.setItemEnabled (SoulKeysProcessor::fileSlot + 1, hasUser);
    const int current = juce::roundToInt (processor.apvts.getRawParameterValue (SoulKeysProcessor::pid (z, "chordSet"))->load());
    r.setBox.setSelectedItemIndex (selected >= 0 ? current : current, juce::dontSendNotification);
    r.listed = processor.numChordSets();
    r.listedUser = hasUser;
}

void SoulKeysEditor::bindSoundKnobs()
{
    for (auto* k : soundKnobs)
    {
        k->attachment.reset();
        k->attachment = std::make_unique<SA> (processor.apvts, SoulKeysProcessor::pid (soundZone, k->base), k->slider);
    }
    soundTabA.setToggleState (soundZone == 0, juce::dontSendNotification);
    soundTabB.setToggleState (soundZone == 1, juce::dontSendNotification);
    const int sound = juce::roundToInt (processor.apvts.getRawParameterValue (SoulKeysProcessor::pid (soundZone, "sound"))->load());
    soundBox.setSelectedItemIndex (sound, juce::dontSendNotification);
    applySkinColours();
    repaint();
}

void SoulKeysEditor::setSkin (int index)
{
    skinIndex = juce::jlimit (0, (int) KeysSkin::all().size() - 1, index);
    processor.apvts.state.setProperty ("skin", skinIndex, nullptr);
    if (settings != nullptr)
    {
        settings->setValue ("skin", skinIndex);
        settings->saveIfNeeded();
    }
    lnf.setSkin (KeysSkin::all()[(size_t) skinIndex]);
    skinBox.setSelectedItemIndex (skinIndex, juce::dontSendNotification);
    applySkinColours();
    sendLookAndFeelChange();
    repaint();
}

void SoulKeysEditor::applySkinColours()
{
    const auto& s = lnf.skin();
    for (auto& r : rows)
    {
        if (r.map) r.map->setSkin (s);
        r.title.setColour (juce::Label::textColourId, &r == &rows[1] ? s.accent2 : s.accent);
        for (auto* l : { &r.setCaption, &r.keyCaption, &r.scaleCaption, &r.extCaption, &r.octCaption })
            l->setColour (juce::Label::textColourId, s.dim);
        r.octValue.setColour (juce::Label::textColourId, s.text);
    }
    keyboard.setSkin (s);
    for (auto* l : { &skinCaption, &splitCaption, &splitAtCaption, &strumLabel, &bassLabel, &leadingLabel, &chordModeLabel, &recCaption, &takeInfo })
        l->setColour (juce::Label::textColourId, s.dim);
    splitHint.setColour (juce::Label::textColourId, s.dim);
    status.setColour (juce::Label::textColourId, s.dim);
    for (auto* list : { &soundKnobs, &playKnobs })
        for (auto* k : *list)
        {
            k->slider.setColour (juce::Slider::rotarySliderFillColourId, list == &soundKnobs && soundZone == 1 ? s.accent2 : s.accent);
            k->slider.setColour (juce::Slider::textBoxTextColourId, s.text);
            k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            k->label.setColour (juce::Label::textColourId, s.dim);
        }
    soundTabB.setColour (juce::TextButton::textColourOnId, contrast (s.accent2));
    recButton.setColour (juce::TextButton::textColourOnId, contrast (s.accent2));
    for (auto* b : rows[1].ext)
        b->setColour (juce::TextButton::textColourOnId, contrast (s.accent2));
}

bool SoulKeysEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (! juce::File (f).hasFileExtension ("mid;midi;wav;aif;aiff;flac;mp3;ogg;soulkeys"))
            return false;
    return ! files.isEmpty();
}

void SoulKeysEditor::filesDropped (const juce::StringArray& files, int, int y)
{
    if (files.isEmpty()) return;
    // Dropped on zone B's area (and split is on) -> zone B, otherwise zone A
    const int zone = processor.splitOn() && y >= rows[1].area.getY() && y < rows[1].area.getBottom() ? 1 : 0;
    if (juce::File (files[0]).hasFileExtension ("soulkeys"))
    {
        int ok = 0;
        for (auto& f : files)
            if (processor.importPreset (juce::File (f), zone).isEmpty()) ++ok;
        refreshSetList (0, true);
        refreshSetList (1, true);
        showStatus ("Imported " + juce::String (ok) + " preset(s) into My Presets.");
        return;
    }
    loadChordsFrom (juce::File (files[0]), zone);
}

void SoulKeysEditor::loadChordsFrom (const juce::File& file, int zone)
{
    showStatus ("Listening to " + file.getFileName() + "...");
    juce::Component::SafePointer<SoulKeysEditor> safe (this);
    juce::Thread::launch ([safe, file, zone]
    {
        auto result = ChordExtractor::fromFile (file);
        juce::MessageManager::callAsync ([safe, result, file, zone]
        {
            if (safe == nullptr) return;
            if (result.error.isNotEmpty())
            {
                safe->showStatus (result.error);
                return;
            }
            safe->processor.setUserChords (zone, result.set);
            safe->refreshSetList (zone, true);
            safe->showStatus ("Found " + juce::String (result.chordsFound) + " chords in " + file.getFileName()
                                  + " for zone " + (zone == 0 ? "A" : "B")
                                  + (result.chordsFound < 12 ? ". The other keys got related chords." : "."));
        });
    });
}

void SoulKeysEditor::timerCallback()
{
    if (processor.isRecording())
    {
        processor.drainRecording (take);
        takeInfo.setText ("Recording...  " + takeSummary(), juce::dontSendNotification);
    }
    else
        takeInfo.setText (take.empty() ? "Press REC, play, then STOP. Drag the take into your DAW." : "Take: " + takeSummary(), juce::dontSendNotification);
    recButton.setToggleState (processor.isRecording(), juce::dontSendNotification);
    recButton.setButtonText (processor.isRecording() ? "REC  *" : "REC");

    const bool split = processor.splitOn();
    for (int z = 0; z < SoulKeysProcessor::numZones; ++z)
    {
        auto& r = rows[(size_t) z];
        refreshSetList (z, false);
        const auto pid = [z] (const char* b) { return SoulKeysProcessor::pid (z, b); };
        const int ext = juce::roundToInt (processor.apvts.getRawParameterValue (pid ("extensions"))->load());
        for (int i = 0; i < r.ext.size(); ++i)
            r.ext[i]->setToggleState (i == ext, juce::dontSendNotification);
        const int oct = juce::roundToInt (processor.apvts.getRawParameterValue (pid ("octave"))->load());
        r.octValue.setText (oct > 0 ? "+" + juce::String (oct) : juce::String (oct), juce::dontSendNotification);
        const int set = juce::roundToInt (processor.apvts.getRawParameterValue (pid ("chordSet"))->load());
        if (r.setBox.getSelectedItemIndex() != set && set < r.setBox.getNumItems())
            r.setBox.setSelectedItemIndex (set, juce::dontSendNotification);
        r.scaleBox.setEnabled (set == SoulKeysProcessor::scaleSlot);
        r.map->refresh();
    }
    rows[0].title.setText (split ? "ZONE A  |  LOW KEYS" : "ALL KEYS", juce::dontSendNotification);
    rows[1].title.setText ("ZONE B  |  HIGH KEYS", juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &rows[1].setBox, &rows[1].keyBox, &rows[1].prev, &rows[1].next, &rows[1].octDown,
                                                             &rows[1].octUp, &rows[1].load, rows[1].map.get(), &swapButton, &splitBox })
        c->setEnabled (split);
    for (auto* b : rows[1].ext) b->setEnabled (split);

    const int sp = processor.splitPoint();
    if (splitBox.getSelectedItemIndex() != sp - 36)
        splitBox.setSelectedItemIndex (sp - 36, juce::dontSendNotification);
    splitHint.setText (split ? "Keys below " + juce::MidiMessage::getMidiNoteName (sp, true, true, 4) + " play zone A, keys from "
                                   + juce::MidiMessage::getMidiNoteName (sp, true, true, 4) + " up play zone B. Both play in the same register."
                             : "Turn on Split Mode to use two chord sets (and two sounds) across the keyboard.",
                       juce::dontSendNotification);

    const int dir = juce::roundToInt (processor.apvts.getRawParameterValue ("strumDir")->load());
    for (int i = 0; i < strumButtons.size(); ++i)
        strumButtons[i]->setToggleState (i == dir, juce::dontSendNotification);
    const int sound = juce::roundToInt (processor.apvts.getRawParameterValue (SoulKeysProcessor::pid (soundZone, "sound"))->load());
    if (soundBox.getSelectedItemIndex() != sound)
        soundBox.setSelectedItemIndex (sound, juce::dontSendNotification);
    keyboard.repaint();
    repaint (rows[1].area);
}

void SoulKeysEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour stripe)
{
    const auto& s = lnf.skin();
    auto f = r.toFloat();
    g.setColour (s.panel);
    g.fillRoundedRectangle (f, 8.0f);
    g.setColour (s.panelEdge);
    g.drawRoundedRectangle (f, 8.0f, 1.0f);
    g.setColour (stripe);
    g.fillRoundedRectangle (f.withHeight (3.0f).reduced (8.0f, 0.0f), 1.5f);
    if (title.isNotEmpty())
    {
        g.setColour (s.text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (title, r.reduced (12, 8).withHeight (18), juce::Justification::centredLeft);
    }
}

void SoulKeysEditor::paint (juce::Graphics& g)
{
    const auto& s = lnf.skin();
    g.setGradientFill (juce::ColourGradient (s.bgTop, 0.0f, 0.0f, s.bg, 0.0f, 200.0f, false));
    g.fillAll();

    const juce::Font font (juce::FontOptions (38.0f, juce::Font::bold));
    const juce::String a = "Rashon ", b = "Soul Keys";
    draw3DText (g, a, font, { 18.0f, 50.0f }, s.titleA.brighter (0.4f), s.titleA, s.titleA.darker (0.5f), s.titleA.darker (0.8f));
    const float aw = juce::GlyphArrangement::getStringWidth (font, a);
    draw3DText (g, b, font, { 18.0f + aw, 50.0f }, s.titleB.brighter (0.5f), s.titleB, s.titleB.darker (0.5f), s.titleB.darker (0.9f));
    g.setColour (s.dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("N E O   S O U L   &   R & B   C H O R D S   -   O N E   K E Y", juce::Rectangle<float> (20.0f, 60.0f, 420.0f, 14.0f), juce::Justification::centredLeft);
    g.setColour (s.titleB);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (300.0f, 60.0f, 60.0f, 14.0f), juce::Justification::centredLeft);

    const auto logo = juce::ImageCache::getFromMemory (BinaryData::dodge_city_logo_png, BinaryData::dodge_city_logo_pngSize);
    g.drawImage (logo, juce::Rectangle<float> (560.0f, 6.0f, 230.0f, 70.0f), juce::RectanglePlacement::centred);

    for (auto& [r, t, zoneB] : panels)
        drawPanel (g, r, t, zoneB ? s.accent2 : s.accent);
}

void SoulKeysEditor::paintOverChildren (juce::Graphics& g)
{
    if (processor.splitOn())
        return;
    // Zone B is inactive: dim it and say how to turn it on
    const auto& s = lnf.skin();
    auto r = rows[1].area.toFloat();
    g.setColour (s.bg.withAlpha (0.72f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (s.text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("Zone B is off.  Turn on SPLIT MODE above to play a second chord set on the high keys.", r, juce::Justification::centred);
}

void SoulKeysEditor::layoutZoneRow (int z, int y)
{
    auto& r = rows[(size_t) z];
    r.area = { 12, y - 4, W - 24, 184 };
    panels.push_back ({ r.area, {}, z == 1 });
    r.title.setBounds (22, y + 2, 160, 18);
    r.savePreset.setBounds (22, y + 22, 92, 24);
    r.importPreset.setBounds (116, y + 22, 62, 24);
    const int cy = y + 4, ry = y + 18;
    r.setCaption.setBounds (186, cy, 120, 12);
    r.prev.setBounds (186, ry, 26, 26);
    r.setBox.setBounds (214, ry, 250, 26);
    r.next.setBounds (466, ry, 26, 26);
    r.keyCaption.setBounds (504, cy, 60, 12);
    r.keyBox.setBounds (504, ry, 90, 26);
    r.scaleCaption.setBounds (600, cy, 60, 12);
    r.scaleBox.setBounds (600, ry, 132, 26);
    r.extCaption.setBounds (742, cy, 100, 12);
    for (int i = 0; i < r.ext.size(); ++i)
        r.ext[i]->setBounds (742 + i * 52, ry, 50, 26);
    r.octCaption.setBounds (960, cy, 60, 12);
    r.octDown.setBounds (960, ry, 24, 26);
    r.octValue.setBounds (984, ry, 30, 26);
    r.octUp.setBounds (1014, ry, 24, 26);
    r.load.setBounds (1048, ry, 100, 26);
    r.map->setBounds (18, y + 50, W - 36, 126);
}

void SoulKeysEditor::resized()
{
    panels.clear();
    skinCaption.setBounds (W - 260, 14, 60, 14);
    skinBox.setBounds (W - 260, 30, 242, 26);

    // Split bar
    splitCaption.setBounds (18, 88, 90, 14);
    splitToggle.setBounds (18, 104, 56, 24);
    splitAtCaption.setBounds (100, 88, 80, 14);
    splitBox.setBounds (100, 104, 80, 24);
    swapButton.setBounds (192, 104, 110, 24);
    splitHint.setBounds (314, 104, W - 330, 24);

    layoutZoneRow (0, 140);
    layoutZoneRow (1, 330);

    // Sound panel
    const int sy = 522, sh = 118;
    panels.push_back ({ { 12, sy, W - 24, sh }, {}, soundZone == 1 });
    soundTabA.setBounds (22, sy + 12, 120, 26);
    soundTabB.setBounds (146, sy + 12, 120, 26);
    prevSound.setBounds (22, sy + 46, 26, 26);
    soundBox.setBounds (50, sy + 46, 190, 26);
    nextSound.setBounds (242, sy + 46, 26, 26);
    for (int i = 0; i < soundKnobs.size(); ++i)
    {
        auto* k = soundKnobs[i];
        const int x = 290 + i * 71;
        k->label.setBounds (x, sy + 12, 70, 16);
        k->slider.setBounds (x, sy + 28, 70, 74);
    }

    // Play panel
    const int py = sy + sh + 8, ph = 104;
    panels.push_back ({ { 12, py, W - 24, ph }, "PLAY", false });
    for (int i = 0; i < playKnobs.size(); ++i)
    {
        playKnobs[i]->label.setBounds (22 + i * 72, py + 24, 70, 16);
        playKnobs[i]->slider.setBounds (22 + i * 72, py + 40, 70, 60);
    }
    strumLabel.setBounds (180, py + 32, 120, 14);
    for (int i = 0; i < strumButtons.size(); ++i)
        strumButtons[i]->setBounds (180 + i * 52, py + 52, 50, 26);
    int x = 360;
    for (auto [l, t] : { std::pair { &bassLabel, &bassToggle }, { &leadingLabel, &leadingToggle }, { &chordModeLabel, &chordModeToggle } })
    {
        l->setBounds (x, py + 32, 100, 14);
        t->setBounds (x, py + 52, 56, 24);
        x += 104;
    }
    // Recorder
    const int rx = 690;
    recCaption.setBounds (rx, py + 24, 200, 14);
    recButton.setBounds (rx, py + 42, 64, 28);
    stopButton.setBounds (rx + 68, py + 42, 60, 28);
    clearButton.setBounds (rx + 132, py + 42, 60, 28);
    dragButton.setBounds (rx + 196, py + 42, 120, 28);
    saveMidiButton.setBounds (rx + 320, py + 42, 120, 28);
    takeInfo.setBounds (rx, py + 74, W - 24 - rx, 18);

    keyboard.setBounds (12, py + ph + 8, W - 24, H - (py + ph + 8) - 8);
    keyboard.setKeyWidth ((float) (W - 24) / 36.0f);
}
