#include "PluginEditor.h"
#include "Artwork.h"
#include "ChordExtractor.h"
#include <BinaryData.h>

namespace
{
    const juce::Colour bg (0xff17130f), panel (0xff241d17), panelEdge (0xff3a2f24);
    const juce::Colour text (0xfff1e6d0), dim (0xffa8957a), gold (0xfff2b84b), rose (0xffe0607e), track (0xff3b3128);
    constexpr int W = 1100, H = 720;
}

//==============================================================================

KeysLookAndFeel::KeysLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, track);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, gold);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void KeysLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    juce::Path t, v;
    t.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    v.addCentredArc (c.x, c.y, r, r, 0.0f, a0, angle, true);
    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (track);
    g.strokePath (t, stroke);
    g.setColour (gold);
    g.strokePath (v, stroke);
    const float capR = r - 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a4a3a), c.x - capR * 0.4f, c.y - capR * 0.5f, juce::Colour (0xff1a1410), c.x + capR, c.y + capR, true));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -capR + 3.0f, 3.0f, capR * 0.5f, 1.5f);
    g.setColour (text);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void KeysLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? gold : track);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (on ? juce::Colours::black : dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
}

void KeysLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (b.getToggleState() ? gold : track);
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

//==============================================================================
// Chord map: one big octave

void ChordMapView::refresh()
{
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto c = processor.chordForKey (pc);
        names[(size_t) pc] = c.name;
        juce::StringArray notes;
        for (int n : c.notes)
            notes.add (juce::MidiMessage::getMidiNoteName (n, false, true, 4));
        noteText[(size_t) pc] = notes.joinIntoString (" ");
        held[(size_t) pc] = processor.keyHeld[pc].load();
    }
    last = processor.lastKeyPc.load();
    repaint();
}

void ChordMapView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0f0c09));
    g.fillRoundedRectangle (b, 10.0f);

    // "Now playing" panel on the right
    auto now = b.removeFromRight (250.0f).reduced (10.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (now, 8.0f);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("NOW PLAYING", now.reduced (14.0f, 10.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
    if (last >= 0)
    {
        g.setColour (gold);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawFittedText (names[(size_t) last], now.reduced (14.0f).withTrimmedTop (30.0f).withHeight (80.0f).toNearestInt(), juce::Justification::centredLeft, 2);
        g.setColour (text);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (noteText[(size_t) last], now.reduced (14.0f).withTrimmedTop (112.0f).toNearestInt(), juce::Justification::topLeft, 3);
    }
    else
    {
        g.setColour (text.withAlpha (0.6f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("Play any key.\nWhite keys: the chords.\nBlack keys: related chords that lead into the next one.",
                          now.reduced (14.0f).withTrimmedTop (30.0f).toNearestInt(), juce::Justification::topLeft, 5);
    }

    // The octave
    auto keys = b.reduced (12.0f, 10.0f);
    const float whiteW = keys.getWidth() / 7.0f;
    for (size_t i = 0; i < 7; ++i)
    {
        const int pc = ChordPresets::whitePcs[i];
        auto r = juce::Rectangle<float> (keys.getX() + whiteW * (float) i, keys.getY(), whiteW, keys.getHeight()).reduced (2.0f, 0.0f);
        const bool on = held[(size_t) pc];
        g.setGradientFill (juce::ColourGradient (on ? gold.brighter (0.4f) : juce::Colour (0xfff7f1e4), 0.0f, r.getY(),
                                                 on ? gold : juce::Colour (0xffd9cdb5), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xff2a1f14));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (names[(size_t) pc], r.reduced (6.0f).withTrimmedTop (r.getHeight() * 0.62f).toNearestInt(), juce::Justification::centredTop, 2);
        g.setColour (juce::Colour (0xff8a7a62));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (ChordQ::noteName (pc), r.reduced (6.0f).removeFromBottom (14.0f), juce::Justification::centred);
    }
    const float blackW = whiteW * 0.62f, blackH = keys.getHeight() * 0.56f;
    const std::array<float, 5> afterWhite { 1, 2, 4, 5, 6 };
    for (size_t i = 0; i < 5; ++i)
    {
        const int pc = ChordPresets::blackPcs[i];
        auto r = juce::Rectangle<float> (keys.getX() + whiteW * afterWhite[i] - blackW * 0.5f, keys.getY(), blackW, blackH);
        const bool on = held[(size_t) pc];
        g.setGradientFill (juce::ColourGradient (on ? rose.brighter (0.3f) : juce::Colour (0xff3a3029), 0.0f, r.getY(),
                                                 on ? rose : juce::Colour (0xff120e0b), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? juce::Colours::black : gold.withAlpha (0.95f));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawFittedText (names[(size_t) pc], r.reduced (4.0f).withTrimmedTop (r.getHeight() * 0.45f).toNearestInt(), juce::Justification::centredTop, 3);
    }
}

//==============================================================================

SoulKeysEditor::SoulKeysEditor (SoulKeysProcessor& p)
    : AudioProcessorEditor (p), processor (p), chordMap (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (chordMap);

    keyboard.setAvailableRange (36, 84);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, gold);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, gold.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    auto label = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, dim);
        l.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (l);
    };

    // Chord set picker
    label (setLabel, "CHORD SET");
    addAndMakeVisible (chordSetBox);
    refreshChordSetList();
    chordSetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "chordSet", chordSetBox);
    auto step = [this] (int delta)
    {
        const int n = processor.numChordSets() - (processor.hasUserChords() ? 0 : 1);
        chordSetBox.setSelectedItemIndex ((chordSetBox.getSelectedItemIndex() + delta + n) % n);
    };
    prevSet.onClick = [step] { step (-1); };
    nextSet.onClick = [step] { step (1); };
    addAndMakeVisible (prevSet);
    addAndMakeVisible (nextSet);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pull chords from a MIDI or audio file", juce::File(), ChordExtractor::supportedPatterns());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadChordsFrom (fc.getResult()); });
    };
    addAndMakeVisible (loadButton);

    // Sound picker: choosing a sound also sets the tone and effect knobs
    label (soundLabel, "SOUND");
    for (size_t i = 0; i < soundPresets().size(); ++i)
        soundBox.addItem (soundPresets()[i].name, (int) i + 1);
    soundBox.setSelectedItemIndex (juce::roundToInt (processor.apvts.getRawParameterValue ("sound")->load()), juce::dontSendNotification);
    soundBox.onChange = [this] { processor.applySoundPreset (soundBox.getSelectedItemIndex()); };
    addAndMakeVisible (soundBox);
    auto stepSound = [this] (int d) { const int n = (int) soundPresets().size(); soundBox.setSelectedItemIndex ((soundBox.getSelectedItemIndex() + d + n) % n); };
    prevSound.onClick = [stepSound] { stepSound (-1); };
    nextSound.onClick = [stepSound] { stepSound (1); };
    addAndMakeVisible (prevSound);
    addAndMakeVisible (nextSound);

    // Extensions: as preset / 7th / 9th / 11th
    label (extLabel, "EXTENSIONS");
    for (auto [i, t] : { std::pair { 0, "Preset" }, { 1, "7th" }, { 2, "9th" }, { 3, "11th" } })
    {
        auto* b = extButtons.add (new juce::TextButton (t));
        b->onClick = [this, i = i] { setChoice ("extensions", i); };
        addAndMakeVisible (b);
    }

    // Knobs
    makeKnob ("bell", "Bell");
    makeKnob ("bark", "Bark");
    makeKnob ("decay", "Decay");
    makeKnob ("tone", "Tone");
    makeKnob ("tremRate", "Trem Rate");
    makeKnob ("tremDepth", "Tremolo");
    makeKnob ("chorus", "Chorus");
    makeKnob ("wobble", "Wobble");
    makeKnob ("age", "Age");
    makeKnob ("hiss", "Hiss");
    makeKnob ("reverb", "Reverb");
    makeKnob ("volume", "Volume");
    makeKnob ("strum", "Strum");
    makeKnob ("humanize", "Humanize");
    makeKnob ("octave", "Octave");

    label (strumLabel, "Strum");
    for (auto [i, t] : { std::pair { 0, "Up" }, { 1, "Down" }, { 2, "Alt" } })
    {
        auto* b = strumButtons.add (new juce::TextButton (t));
        b->onClick = [this, i = i] { setChoice ("strumDir", i); };
        addAndMakeVisible (b);
    }
    label (bassLabel, "Bass note");
    label (leadingLabel, "Voice leading");
    label (chordModeLabel, "Chord mode");
    for (auto* t : { &bassToggle, &leadingToggle, &chordModeToggle })
        addAndMakeVisible (*t);
    bassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "bass", bassToggle);
    leadingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "voiceLeading", leadingToggle);
    chordModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "chordMode", chordModeToggle);

    status.setColour (juce::Label::textColourId, dim);
    status.setFont (juce::FontOptions (12.0f));
    status.setText ("Tip: drop a MIDI or audio file anywhere on this window to pull its chords onto the keys.", juce::dontSendNotification);
    addAndMakeVisible (status);

    setSize (W, H);
    chordMap.refresh();
    startTimerHz (20);
}

SoulKeysEditor::~SoulKeysEditor()
{
    setLookAndFeel (nullptr);
}

SoulKeysEditor::Knob* SoulKeysEditor::makeKnob (const char* id, const char* name)
{
    auto* k = knobs.add (new Knob());
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, text);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, k->slider);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, dim);
    k->label.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (k->label);
    return k;
}

void SoulKeysEditor::setChoice (const char* id, int index)
{
    auto* p = processor.apvts.getParameter (id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    p->endChangeGesture();
}

void SoulKeysEditor::refreshChordSetList()
{
    const bool hasUser = processor.hasUserChords();
    if (listedSets == processor.numChordSets() && listedHasUser == hasUser && ! hasUser)
        return;
    const int selected = chordSetBox.getSelectedItemIndex();
    chordSetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < processor.numChordSets(); ++i)
        chordSetBox.addItem (processor.getChordSet (i).name, i + 1);
    chordSetBox.setItemEnabled (processor.numChordSets(), hasUser);
    if (selected >= 0)
        chordSetBox.setSelectedItemIndex (selected, juce::dontSendNotification);
    listedSets = processor.numChordSets();
    listedHasUser = hasUser;
}

bool SoulKeysEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("mid;midi;wav;aif;aiff;flac;mp3;ogg");
}

void SoulKeysEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty())
        loadChordsFrom (juce::File (files[0]));
}

void SoulKeysEditor::loadChordsFrom (const juce::File& file)
{
    status.setText ("Listening to " + file.getFileName() + "...", juce::dontSendNotification);
    juce::Component::SafePointer<SoulKeysEditor> safe (this);
    juce::Thread::launch ([safe, file]
    {
        auto result = ChordExtractor::fromFile (file);
        juce::MessageManager::callAsync ([safe, result, file]
        {
            if (safe == nullptr) return;
            if (result.error.isNotEmpty())
            {
                safe->status.setText (result.error, juce::dontSendNotification);
                return;
            }
            safe->processor.setUserChords (result.set);
            safe->listedSets = -1;
            safe->refreshChordSetList();
            safe->chordSetBox.setSelectedItemIndex (safe->processor.userSlot(), juce::sendNotificationSync);
            safe->status.setText ("Found " + juce::String (result.chordsFound) + " chords in " + file.getFileName()
                                  + (result.chordsFound < 12 ? ". The other keys got related chords." : "."), juce::dontSendNotification);
            safe->chordMap.refresh();
        });
    });
}

void SoulKeysEditor::timerCallback()
{
    const int ext = juce::roundToInt (processor.apvts.getRawParameterValue ("extensions")->load());
    for (int i = 0; i < extButtons.size(); ++i)
        extButtons[i]->setToggleState (i == ext, juce::dontSendNotification);
    const int dir = juce::roundToInt (processor.apvts.getRawParameterValue ("strumDir")->load());
    for (int i = 0; i < strumButtons.size(); ++i)
        strumButtons[i]->setToggleState (i == dir, juce::dontSendNotification);
    const int sound = juce::roundToInt (processor.apvts.getRawParameterValue ("sound")->load());
    if (soundBox.getSelectedItemIndex() != sound)
        soundBox.setSelectedItemIndex (sound, juce::dontSendNotification);
    chordMap.refresh();
}

void SoulKeysEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    auto f = r.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (f, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (f, 8.0f, 1.0f);
    g.setColour (gold);
    g.fillRoundedRectangle (f.withHeight (3.0f).reduced (8.0f, 0.0f), 1.5f);
    g.setColour (text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (title, r.reduced (12, 8).withHeight (18), juce::Justification::centredLeft);
}

void SoulKeysEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2119), 0.0f, 0.0f, bg, 0.0f, 160.0f, false));
    g.fillAll();

    // Title: chrome "Rashon" and gold "Soul Keys"
    const juce::Font font (juce::FontOptions (40.0f, juce::Font::bold));
    const juce::String a = "Rashon ", b = "Soul Keys";
    draw3DText (g, a, font, { 18.0f, 54.0f }, juce::Colour (0xffffffff), juce::Colour (0xffc4cbd6), juce::Colour (0xff6d7685), juce::Colour (0xff3b414c));
    const float aw = juce::GlyphArrangement::getStringWidth (font, a);
    draw3DText (g, b, font, { 18.0f + aw, 54.0f }, juce::Colour (0xfffff3c8), gold, juce::Colour (0xffa8650f), juce::Colour (0xff5a3306));
    const float tw = aw + juce::GlyphArrangement::getStringWidth (font, b);
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("N E O   S O U L   &   R & B   C H O R D S   -   O N E   K E Y", juce::Rectangle<float> (20.0f, 64.0f, 420.0f, 14.0f), juce::Justification::centredLeft);
    g.setColour (gold);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (18.0f + tw - 50.0f, 64.0f, 50.0f, 14.0f), juce::Justification::centredRight);

    const auto logo = juce::ImageCache::getFromMemory (BinaryData::dodge_city_logo_png, BinaryData::dodge_city_logo_pngSize);
    g.drawImage (logo, juce::Rectangle<float> ((float) W - 18.0f - 240.0f, 8.0f, 240.0f, 72.0f), juce::RectanglePlacement::centred);

    for (auto& [r, t] : panels)
        drawPanel (g, r, t);
}

void SoulKeysEditor::resized()
{
    // Picker row
    setLabel.setBounds (18, 92, 80, 16);
    prevSet.setBounds (18, 110, 28, 28);
    chordSetBox.setBounds (50, 110, 300, 28);
    nextSet.setBounds (354, 110, 28, 28);
    loadButton.setBounds (390, 110, 200, 28);
    soundLabel.setBounds (606, 92, 80, 16);
    prevSound.setBounds (606, 110, 28, 28);
    soundBox.setBounds (638, 110, 200, 28);
    nextSound.setBounds (842, 110, 28, 28);
    extLabel.setBounds (884, 92, 120, 16);
    for (int i = 0; i < extButtons.size(); ++i)
        extButtons[i]->setBounds (884 + i * 50, 110, 48, 28);

    chordMap.setBounds (18, 148, W - 36, 196);

    // Knob panels
    panels.clear();
    auto knobAt = [this] (int index, int x, int y)
    {
        auto* k = knobs[index];
        k->label.setBounds (x, y, 70, 16);
        k->slider.setBounds (x, y + 16, 70, 72);
    };
    const int py = 354, ph = 120;
    panels.push_back ({ { 18, py, 300, ph }, "SOUND" });
    for (int i = 0; i < 4; ++i) knobAt (i, 26 + i * 72, py + 24);
    panels.push_back ({ { 326, py, 230, ph }, "MOTION" });
    for (int i = 0; i < 3; ++i) knobAt (4 + i, 334 + i * 72, py + 24);
    panels.push_back ({ { 564, py, 230, ph }, "TAPE" });
    for (int i = 0; i < 3; ++i) knobAt (7 + i, 572 + i * 72, py + 24);
    panels.push_back ({ { 802, py, 280, ph }, "SPACE & OUTPUT" });
    knobAt (10, 840, py + 24);
    knobAt (11, 960, py + 24);

    const int qy = py + ph + 8, qh = 114;
    panels.push_back ({ { 18, qy, W - 36, qh }, "PLAY" });
    knobAt (12, 26, qy + 24);
    knobAt (13, 98, qy + 24);
    knobAt (14, 170, qy + 24);
    strumLabel.setBounds (260, qy + 36, 80, 16);
    for (int i = 0; i < strumButtons.size(); ++i)
        strumButtons[i]->setBounds (260 + i * 52, qy + 56, 50, 26);
    int x = 440;
    for (auto [l, t] : { std::pair { &bassLabel, &bassToggle }, { &leadingLabel, &leadingToggle }, { &chordModeLabel, &chordModeToggle } })
    {
        l->setBounds (x, qy + 36, 110, 16);
        t->setBounds (x, qy + 56, 56, 24);
        x += 130;
    }
    status.setBounds (x, qy + 36, W - 36 - x, 44);

    keyboard.setBounds (18, qy + qh + 8, W - 36, H - (qy + qh + 8) - 10);
    keyboard.setKeyWidth ((float) (W - 36) / 29.0f);
}
