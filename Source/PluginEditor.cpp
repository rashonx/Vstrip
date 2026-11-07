#include "PluginEditor.h"

namespace
{
    const juce::Colour background (0xff15171c);
    const juce::Colour panel      (0xff20242c);
    const juce::Colour textDim    (0xff9aa3b2);

    constexpr int knobWidth = 84;
    constexpr int rowHeight = 150;
    constexpr int headerHeight = 112;
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
    repaint (getLocalBounds().removeFromTop (headerHeight).withTrimmedLeft ((int) metersLeft));
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

// Draws text as raised 3D lettering: soft shadow, a stepped extrusion, a metallic
// gradient face and a thin bevel highlight along the top edges.
static void draw3DText (juce::Graphics& g, const juce::String& text, juce::Font font, juce::Point<float> origin,
                        juce::Colour top, juce::Colour middle, juce::Colour bottom, juce::Colour side)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, origin.x, origin.y);
    juce::Path face;
    glyphs.createPath (face);
    const auto bounds = face.getBounds();

    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 10, { 5, 7 }).drawForPath (g, face);

    constexpr int depth = 6;
    for (int i = depth; i > 0; --i)
    {
        const float t = (float) i / (float) depth;
        g.setColour (side.interpolatedWith (juce::Colours::black, t * 0.6f));
        g.fillPath (face, juce::AffineTransform::translation (0.7f * (float) i, 0.9f * (float) i));
    }

    juce::ColourGradient grad (top, 0.0f, bounds.getY(), bottom, 0.0f, bounds.getBottom(), false);
    grad.addColour (0.48, middle);
    grad.addColour (0.52, middle.darker (0.25f));
    g.setGradientFill (grad);
    g.fillPath (face);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (face, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.4f, -0.5f));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (face, juce::PathStrokeType (0.6f));
}

// A large-diaphragm condenser studio mic in a shock mount, drawn with vector shapes.
static void drawCondenserMic (juce::Graphics& g, juce::Rectangle<float> area)
{
    using juce::Colour;
    const float w = juce::jmin (area.getWidth(), area.getHeight() * 0.42f);
    const float cx = area.getCentreX();
    const float y0 = area.getY();
    const float h = area.getHeight();

    auto metal = [] (juce::Rectangle<float> r, Colour dark, Colour light)
    {
        juce::ColourGradient grad (dark, r.getX(), 0.0f, dark, r.getRight(), 0.0f, false);
        grad.addColour (0.3, light);
        grad.addColour (0.45, juce::Colours::white.withAlpha (0.95f));
        grad.addColour (0.6, light);
        return grad;
    };

    // Head (capsule grille)
    const auto head = juce::Rectangle<float> (cx - w * 0.5f, y0 + h * 0.02f, w, h * 0.52f);
    juce::Path headPath;
    headPath.addRoundedRectangle (head, w * 0.5f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 3, 4 }).drawForPath (g, headPath);
    g.setGradientFill (metal (head, Colour (0xff4a4f58), Colour (0xffc9ced6)));
    g.fillPath (headPath);

    {   // Mesh pattern clipped to the grille
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (headPath);
        g.setColour (Colour (0xff1c1f24).withAlpha (0.45f));
        for (float d = -head.getHeight(); d < head.getWidth() + head.getHeight(); d += 3.2f)
        {
            g.drawLine (head.getX() + d, head.getY(), head.getX() + d - head.getHeight(), head.getBottom(), 0.6f);
            g.drawLine (head.getX() + d - head.getHeight(), head.getY(), head.getX() + d, head.getBottom(), 0.6f);
        }
        // Darker inner capsule showing through the mesh
        g.setColour (Colour (0xff101215).withAlpha (0.35f));
        g.fillEllipse (head.reduced (w * 0.22f, h * 0.08f));
    }
    g.setColour (Colour (0xff2a2d33));
    g.strokePath (headPath, juce::PathStrokeType (1.2f));

    // Centre ring with a gold badge
    const auto ring = juce::Rectangle<float> (head.getX() - 1.0f, head.getCentreY() - h * 0.035f, w + 2.0f, h * 0.07f);
    g.setGradientFill (metal (ring, Colour (0xff3a3e45), Colour (0xffe3e6ea)));
    g.fillRoundedRectangle (ring, 2.0f);
    g.setColour (Colour (0xff2a2d33));
    g.drawRoundedRectangle (ring, 2.0f, 0.8f);
    const float badge = h * 0.05f;
    g.setGradientFill (juce::ColourGradient (Colour (0xffffe9a8), cx, ring.getY(), Colour (0xffb8801f), cx, ring.getBottom(), false));
    g.fillEllipse (cx - badge * 0.5f, ring.getCentreY() - badge * 0.5f, badge, badge);

    // Body
    const auto body = juce::Rectangle<float> (cx - w * 0.36f, head.getBottom() - h * 0.04f, w * 0.72f, h * 0.24f);
    g.setGradientFill (metal (body, Colour (0xff1d2025), Colour (0xff6b717c)));
    g.fillRoundedRectangle (body, w * 0.12f);

    // Shock mount: elliptical ring with suspension bands
    const auto mount = juce::Rectangle<float> (cx - w * 0.75f, body.getCentreY() - h * 0.06f, w * 1.5f, h * 0.12f);
    g.setColour (Colour (0xff2b2f36));
    g.drawEllipse (mount, 2.4f);
    g.setColour (Colour (0xff9aa1ab));
    g.drawEllipse (mount.translated (0.0f, -0.8f), 0.8f);
    g.setColour (Colour (0xffd8a33a).withAlpha (0.85f));
    for (float side : { -1.0f, 1.0f })
    {
        g.drawLine (cx + side * mount.getWidth() * 0.5f, mount.getCentreY(), cx + side * body.getWidth() * 0.5f, body.getY() + 3.0f, 1.0f);
        g.drawLine (cx + side * mount.getWidth() * 0.5f, mount.getCentreY(), cx + side * body.getWidth() * 0.5f, body.getBottom() - 3.0f, 1.0f);
    }

    // Stand rod
    const auto rod = juce::Rectangle<float> (cx - w * 0.07f, mount.getBottom() - 1.0f, w * 0.14f, area.getBottom() - mount.getBottom());
    g.setGradientFill (metal (rod, Colour (0xff1d2025), Colour (0xff7d838d)));
    g.fillRect (rod);
}

void VocalStripEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto headerBg = getLocalBounds().removeFromTop (headerHeight - 4).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a33), 0.0f, 0.0f, background, 0.0f, headerBg.getBottom(), false));
    g.fillRect (headerBg);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (gap + 6, 8).toFloat();

    // Title: chrome "Rashon X" and gold "Vocal"
    const juce::Font titleFont (juce::FontOptions (40.0f, juce::Font::bold));
    const juce::String first = "Rashon X ", second = "Vocal";
    const float baseline = header.getY() + 50.0f;
    const float x0 = header.getX();
    draw3DText (g, first, titleFont, { x0, baseline },
                juce::Colour (0xffffffff), juce::Colour (0xffc4cbd6), juce::Colour (0xff6d7685), juce::Colour (0xff3b414c));
    const float firstWidth = juce::GlyphArrangement::getStringWidth (titleFont, first);
    draw3DText (g, second, titleFont, { x0 + firstWidth, baseline },
                juce::Colour (0xfffff3c8), juce::Colour (0xfff2b84b), juce::Colour (0xffa8650f), juce::Colour (0xff5a3306));
    const float titleWidth = firstWidth + juce::GlyphArrangement::getStringWidth (titleFont, second);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("V O C A L   C H A N N E L   S T R I P", juce::Rectangle<float> (x0 + 2.0f, baseline + 14.0f, titleWidth, 16.0f),
                juce::Justification::centredLeft);
    header.removeFromLeft (titleWidth + 14.0f);

    // Studio mic to the right of the title
    drawCondenserMic (g, header.removeFromLeft (64.0f).withTrimmedBottom (-6.0f));
    header.removeFromLeft (24.0f);

    // Meters in a 2 x 2 grid (the only part of the header redrawn by the timer)
    metersLeft = header.getX() - 2.0f;
    auto grid = header.withSizeKeepingCentre (header.getWidth(), 64.0f);
    auto topRow = grid.removeFromTop (30.0f);
    grid.removeFromTop (4.0f);
    auto bottomRow = grid;
    const float colW = (header.getWidth() - (float) gap * 2.0f) / 2.0f;
    auto cell = [&] (juce::Rectangle<float>& row) { auto c = row.removeFromLeft (colW); row.removeFromLeft ((float) gap * 2.0f); return c; };
    drawMeter (g, cell (topRow), "INPUT  " + juce::String (inDb, 1) + " dB", inDb, false, juce::Colour (0xff6ec6a8));
    drawMeter (g, cell (topRow), "OUTPUT  " + juce::String (outDb, 1) + " dB", outDb, false, juce::Colour (0xff6ec6a8));
    drawMeter (g, cell (bottomRow), "DE-ESS  " + juce::String (deEssGr, 1) + " dB", deEssGr, true, juce::Colour (0xfff0c05f));
    drawMeter (g, cell (bottomRow), "COMP  " + juce::String (compGr, 1) + " dB", compGr, true, juce::Colour (0xfff08a5f));
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
