#include "PluginEditor.h"
#include "Artwork.h"
#include <BinaryData.h>

namespace
{
    const juce::Colour background (0xff15171c);
    const juce::Colour panel      (0xff20242c);
    const juce::Colour textDim    (0xff9aa3b2);
    const juce::Colour carveRed   (0xffff5a5f);
    const juce::Colour beatBlue   (0xff5fb3f0);
    const juce::Colour vocalGold  (0xfff2b84b);
    const juce::Colour mixGreen   (0xff6ec6a8);

    constexpr int editorWidth = 1120, editorHeight = 660, headerHeight = 112, gap = 12;
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

void KnobLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool isListen = b.getButtonText().startsWith ("LISTEN");
    g.setColour (on ? (isListen ? carveRed : vocalGold) : juce::Colour (0xff2c313b));
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

//==============================================================================
// 32-band display

namespace
{
    constexpr float minDb = -72.0f, maxDb = 0.0f, maxCut = BandCarver::maxDepthDb;
}

void CarveView::update()
{
    const auto& d = processor.getDisplay();
    float loudestVocal = -200.0f;
    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        auto follow = [] (float& s, float t) { s = t > s ? t : 0.8f * s + 0.2f * t; };
        follow (beat[b], d.beatDb[b].load());
        follow (vocal[b], d.vocalDb[b].load());
        cut[b] = 0.5f * cut[b] + 0.5f * d.cutDb[b].load();
        loudestVocal = juce::jmax (loudestVocal, d.vocalDb[b].load());
    }
    hasVocal = loudestVocal > -90.0f;
    listening = processor.apvts.getRawParameterValue ("listen")->load() > 0.5f;
    connected = processor.sidechainConnected.load();
    repaint();
}

void CarveView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e1014));
    g.fillRoundedRectangle (bounds, 8.0f);

    auto area = bounds.reduced (12.0f, 10.0f);
    area.removeFromLeft (30.0f);
    auto labels = area.removeFromBottom (16.0f);
    auto depth = area.removeFromBottom (58.0f);
    area.removeFromBottom (8.0f);
    auto bars = area;

    const auto& apvts = processor.apvts;
    const float lowCut = apvts.getRawParameterValue ("lowCut")->load();
    const float highCut = apvts.getRawParameterValue ("highCut")->load();

    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, bars.getBottom(), bars.getY()); };
    const float slot = bars.getWidth() / (float) numBands;
    auto xForBand = [&] (float b) { return bars.getX() + slot * b; };
    // Position of a frequency on the band axis
    auto xForHz = [&] (float hz) { return xForBand ((float) numBands * std::log (hz / 20.0f) / std::log (1000.0f)); };

    // Grid
    g.setFont (juce::FontOptions (10.0f));
    for (float db = -60.0f; db <= 0.0f; db += 12.0f)
    {
        const float y = yForDb (db);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (juce::roundToInt (y), bars.getX(), bars.getRight());
        g.setColour (textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (bars.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForHz (hz);
        g.setColour (textDim);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, labels.getY(), 40.0f, labels.getHeight()), juce::Justification::centred);
    }
    g.setColour (juce::Colour (0xff181b21));
    g.fillRect (depth);
    g.setColour (textDim);
    g.drawText ("0", juce::Rectangle<float> (depth.getX() - 32.0f, depth.getY() - 2.0f, 28.0f, 12.0f), juce::Justification::centredRight);
    g.drawText ("-" + juce::String ((int) maxCut), juce::Rectangle<float> (depth.getX() - 32.0f, depth.getBottom() - 10.0f, 28.0f, 12.0f), juce::Justification::centredRight);

    for (int b = 0; b < numBands; ++b)
    {
        const auto i = (size_t) b;
        const float x = xForBand ((float) b) + 2.0f, w = slot - 4.0f;
        const float top = yForDb (beat[i]);
        const float carvedTop = yForDb (beat[i] + cut[i]);

        // Beat level, with the carved-out part in red on top
        g.setGradientFill (juce::ColourGradient (beatBlue, 0.0f, bars.getY(), beatBlue.withAlpha (0.25f), 0.0f, bars.getBottom(), false));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, carvedTop, x + w, bars.getBottom()));
        if (carvedTop > top + 0.5f)
        {
            g.setColour (carveRed.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, top, x + w, carvedTop));
        }

        // Vocal level as a gold cap
        const float vy = yForDb (vocal[i]);
        if (vocal[i] > minDb)
        {
            g.setColour (vocalGold);
            g.fillRect (x - 1.0f, vy - 1.5f, w + 2.0f, 3.0f);
        }

        // Carve depth per band
        const float depthY = juce::jmap (juce::jlimit (-maxCut, 0.0f, cut[i]), 0.0f, -maxCut, depth.getY(), depth.getBottom());
        g.setGradientFill (juce::ColourGradient (carveRed.withAlpha (0.5f), 0.0f, depth.getY(), carveRed, 0.0f, depth.getBottom(), false));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, depth.getY(), x + w, depthY));
    }

    // Dim the bands outside the carve range
    const float xLow = xForHz (lowCut), xHigh = xForHz (highCut);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    if (xLow > bars.getX())
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bars.getX(), bars.getY(), xLow, depth.getBottom()));
    if (xHigh < bars.getRight())
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xHigh, bars.getY(), bars.getRight(), depth.getBottom()));
    const float dashes[] = { 4.0f, 3.0f };
    auto hzText = [] (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; };
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (carveRed.withAlpha (0.9f));
    if (xLow > bars.getX() + 1.0f)
    {
        g.drawDashedLine ({ xLow, bars.getY(), xLow, depth.getBottom() }, dashes, 2, 1.2f);
        g.drawText ("LOW CUT " + hzText (lowCut), juce::Rectangle<float> (xLow + 4.0f, bars.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredLeft);
    }
    if (xHigh < bars.getRight() - 1.0f)
    {
        g.drawDashedLine ({ xHigh, bars.getY(), xHigh, depth.getBottom() }, dashes, 2, 1.2f);
        g.drawText ("HIGH CUT " + hzText (highCut), juce::Rectangle<float> (xHigh - 114.0f, bars.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredRight);
    }
    g.setColour (textDim);
    g.drawText ("CARVE DEPTH", depth.reduced (6.0f, 4.0f), juce::Justification::bottomLeft);

    // Legend
    auto legend = juce::Rectangle<float> (bars.getX() + 10.0f, bars.getY() + 18.0f, 460.0f, 14.0f);
    auto item = [&] (juce::Colour c, const juce::String& text, bool line)
    {
        auto swatch = legend.removeFromLeft (14.0f).reduced (1.0f, line ? 5.5f : 2.0f);
        g.setColour (c);
        g.fillRect (swatch);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (juce::FontOptions (11.0f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 18.0f;
        g.drawText (text, legend.removeFromLeft (tw).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
    };
    item (beatBlue, "Beat", false);
    item (carveRed, "Carved out", false);
    item (vocalGold, "Vocal (sidechain)", true);

    // Routing label: which signal is carved and which one does the carving
    {
        auto row = juce::Rectangle<float> (bars.getRight() - 470.0f, bars.getY() + 16.0f, 460.0f, 18.0f);
        auto pill = [&] (juce::Rectangle<float> r, const juce::String& text, juce::Colour c)
        {
            g.setColour (c.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (c);
            g.drawRoundedRectangle (r, 9.0f, 1.0f);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (text, r, juce::Justification::centred);
        };
        const juce::String keyText = ! connected ? "SIDECHAIN: NOT CONNECTED"
                                   : hasVocal   ? "SIDECHAIN (VOCAL): SIGNAL OK"
                                                : "SIDECHAIN (VOCAL): NO SIGNAL";
        const auto keyColour = connected && hasVocal ? vocalGold : carveRed;
        pill (row.removeFromRight (230.0f), keyText, keyColour);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("CARVED BY", row.removeFromRight (78.0f), juce::Justification::centred);
        pill (row.removeFromRight (150.0f), "CARVING: THIS TRACK", beatBlue);
    }

    if (listening)
    {
        auto banner = juce::Rectangle<float> (300.0f, 26.0f).withCentre ({ bars.getCentreX(), bars.getY() + 56.0f });
        g.setColour (carveRed);
        g.fillRoundedRectangle (banner, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("LISTENING TO THE CARVED PART ONLY", banner, juce::Justification::centred);
    }

    if (! hasVocal)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No vocal detected - route your vocal into this plugin's sidechain input", bars, juce::Justification::centred);
    }
}

//==============================================================================
// Editor

VocalSitterEditor::VocalSitterEditor (VocalSitterProcessor& p)
    : AudioProcessorEditor (p), processor (p), carveView (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (carveView);

    amount  = makeKnob ("amount", "AMOUNT", carveRed);
    attack  = makeKnob ("attack", "Attack", carveRed);
    release = makeKnob ("release", "Release", carveRed);
    lowCut  = makeKnob ("lowCut", "Low Cut", carveRed);
    highCut = makeKnob ("highCut", "High Cut", carveRed);
    output  = makeKnob ("outGain", "Output", mixGreen);
    amount->label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    amount->label.setColour (juce::Label::textColourId, juce::Colours::white);

    for (auto* l : { &stereoLabel, &autoGainLabel })
    {
        l->setFont (juce::FontOptions (12.0f));
        l->setColour (juce::Label::textColourId, textDim);
        addAndMakeVisible (*l);
    }
    stereoLabel.setText ("Carve the beat's", juce::dontSendNotification);
    autoGainLabel.setText ("Auto gain", juce::dontSendNotification);

    const juce::StringArray modes { "Stereo", "Mid", "Side" };
    for (int i = 0; i < modes.size(); ++i)
    {
        auto* b = stereoButtons.add (new juce::TextButton (modes[i]));
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.8f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b->onClick = [this, i] { setStereoMode (i); };
        addAndMakeVisible (b);
    }

    autoGainButton.setColour (juce::ToggleButton::tickColourId, mixGreen);
    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "autoGain", autoGainButton);

    listenButton.setClickingTogglesState (true);
    listenButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
    listenButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    listenButton.setTooltip ("Hear only what is being carved out of this track");
    addAndMakeVisible (listenButton);
    listenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "listen", listenButton);

    setSize (editorWidth, editorHeight);
    timerCallback();
    startTimerHz (30);
}

VocalSitterEditor::~VocalSitterEditor()
{
    setLookAndFeel (nullptr);
}

VocalSitterEditor::Control* VocalSitterEditor::makeKnob (const juce::String& id, const juce::String& name, juce::Colour accent)
{
    auto* c = controls.add (new Control());
    auto& s = c->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (s);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, s);

    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::FontOptions (12.0f));
    c->label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (c->label);
    return c;
}

void VocalSitterEditor::setStereoMode (int index)
{
    auto* param = processor.apvts.getParameter ("stereo");
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
    param->endChangeGesture();
}

void VocalSitterEditor::timerCallback()
{
    auto follow = [] (float& s, float t) { s = t > s ? t : s + (t - s) * 0.2f; };
    follow (beatDb, processor.beatLevelDb.load());
    follow (vocalDb, processor.vocalLevelDb.load());
    follow (outDb, processor.outputLevelDb.load());
    autoDb += (processor.autoGainDb.load() - autoDb) * 0.2f;

    const int mode = juce::roundToInt (processor.apvts.getRawParameterValue ("stereo")->load());
    for (int i = 0; i < stereoButtons.size(); ++i)
        stereoButtons[i]->setToggleState (i == mode, juce::dontSendNotification);

    carveView.update();
    repaint (meterArea);
}

void VocalSitterEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent)
{
    auto r = area.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title, area.getX() + 12, area.getY() + 8, area.getWidth() - 24, 20, juce::Justification::centredLeft);
}

void VocalSitterEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, float valueDb, juce::Colour colour)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (name + "  " + juce::String (valueDb, 1) + " dB", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto bar = area.removeFromTop (10.0f);
    g.setColour (juce::Colour (0xff2c313b));
    g.fillRoundedRectangle (bar, 3.0f);
    const float filled = juce::jlimit (0.0f, 1.0f, (valueDb + 60.0f) / 60.0f);
    g.setColour (valueDb > -1.0f ? juce::Colours::red : colour);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * filled), 3.0f);
}

void VocalSitterEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    auto headerBg = getLocalBounds().removeFromTop (headerHeight - 4).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a33), 0.0f, 0.0f, background, 0.0f, headerBg.getBottom(), false));
    g.fillRect (headerBg);

    // Title: chrome "Rashon" and gold "Vocal Sitter"
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (gap + 6, 8).toFloat();
    const juce::Font titleFont (juce::FontOptions (40.0f, juce::Font::bold));
    const juce::String first = "Rashon ", second = "Vocal Sitter";
    const float baseline = header.getY() + 50.0f;
    draw3DText (g, first, titleFont, { header.getX(), baseline },
                juce::Colour (0xffffffff), juce::Colour (0xffc4cbd6), juce::Colour (0xff6d7685), juce::Colour (0xff3b414c));
    const float firstWidth = juce::GlyphArrangement::getStringWidth (titleFont, first);
    draw3DText (g, second, titleFont, { header.getX() + firstWidth, baseline },
                juce::Colour (0xfffff3c8), juce::Colour (0xfff2b84b), juce::Colour (0xffa8650f), juce::Colour (0xff5a3306));
    const float titleWidth = firstWidth + juce::GlyphArrangement::getStringWidth (titleFont, second);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    // Version number, so you can tell which build is installed
    g.setColour (vocalGold);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (header.getX() + titleWidth - 60.0f, baseline + 14.0f, 60.0f, 16.0f),
                juce::Justification::centredRight);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("S I D E C H A I N   T R A C K   S P A C E R", juce::Rectangle<float> (header.getX() + 2.0f, baseline + 14.0f, titleWidth + 40.0f, 16.0f),
                juce::Justification::centredLeft);
    header.removeFromLeft (titleWidth + 14.0f);

    // Dodge City Plugins logo, centred in the window
    const auto logo = juce::ImageCache::getFromMemory (BinaryData::dodge_city_logo_png, BinaryData::dodge_city_logo_pngSize);
    const float logoH = 92.0f, logoW = logoH * (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight());
    const auto logoArea = juce::Rectangle<float> (logoW, logoH).withCentre ({ (float) getWidth() * 0.5f, header.getCentreY() });
    g.drawImage (logo, logoArea, juce::RectanglePlacement::centred);
    header = header.withLeft (logoArea.getRight() + 28.0f);

    // Meters in a 2 x 2 grid
    meterArea = header.toNearestInt().expanded (2);
    auto grid = header.withSizeKeepingCentre (header.getWidth(), 64.0f);
    auto topRow = grid.removeFromTop (30.0f);
    grid.removeFromTop (4.0f);
    auto bottomRow = grid;
    const float colW = (header.getWidth() - (float) gap * 2.0f) / 2.0f;
    auto cell = [&] (juce::Rectangle<float>& row) { auto c = row.removeFromLeft (colW); row.removeFromLeft ((float) gap * 2.0f); return c; };
    drawMeter (g, cell (topRow), "BEAT", beatDb, beatBlue);
    drawMeter (g, cell (topRow), "VOCAL", vocalDb, vocalGold);
    drawMeter (g, cell (bottomRow), "OUTPUT", outDb, mixGreen);

    auto ag = cell (bottomRow);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("AUTO GAIN  " + juce::String (autoDb >= 0.0f ? "+" : "") + juce::String (autoDb, 1) + " dB", ag.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto bar = ag.removeFromTop (10.0f);
    g.setColour (juce::Colour (0xff2c313b));
    g.fillRoundedRectangle (bar, 3.0f);
    const float half = bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, autoDb / 12.0f);
    g.setColour (mixGreen);
    g.fillRoundedRectangle (half >= 0.0f ? bar.withX (bar.getCentreX()).withWidth (half) : bar.withX (bar.getCentreX() + half).withWidth (-half), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawVerticalLine (juce::roundToInt (bar.getCentreX()), bar.getY() - 2.0f, bar.getBottom() + 2.0f);

    drawPanel (g, carvePanel, "CARVE", carveRed);
    drawPanel (g, outputPanel, "OUTPUT", mixGreen);
}

void VocalSitterEditor::resized()
{
    auto area = getLocalBounds().reduced (gap);
    area.removeFromTop (headerHeight - gap);
    carveView.setBounds (area.removeFromTop (290));
    area.removeFromTop (gap);

    outputPanel = area.removeFromRight (360);
    area.removeFromRight (gap);
    carvePanel = area;

    auto place = [] (Control* c, juce::Rectangle<int> r)
    {
        c->label.setBounds (r.removeFromTop (16));
        c->slider.setBounds (r);
    };

    // Carve: big Amount knob, then Attack, Release, Low Cut, High Cut
    {
        auto r = carvePanel.withTrimmedTop (32).reduced (10, 4);
        place (amount, r.removeFromLeft (180));
        r.removeFromLeft (10);
        const int w = r.getWidth() / 4;
        for (auto* c : { attack, release, lowCut, highCut })
            place (c, r.removeFromLeft (w).withSizeKeepingCentre (w, juce::jmin (r.getHeight(), 130)));
    }

    // Output: stereo mode buttons and auto gain on the left, Output knob on the right
    {
        auto r = outputPanel.withTrimmedTop (34).reduced (12, 6);
        place (output, r.removeFromRight (130).withSizeKeepingCentre (130, juce::jmin (r.getHeight(), 140)));
        r.removeFromRight (10);
        r.removeFromTop (8);
        stereoLabel.setBounds (r.removeFromTop (18));
        auto row = r.removeFromTop (30);
        const int bw = row.getWidth() / 3;
        for (auto* b : stereoButtons)
            b->setBounds (row.removeFromLeft (bw).reduced (2, 0));
        r.removeFromTop (14);
        autoGainLabel.setBounds (r.removeFromTop (18));
        autoGainButton.setBounds (r.removeFromTop (24).withWidth (56));
        r.removeFromTop (16);
        listenButton.setBounds (r.removeFromTop (34));
    }
}
