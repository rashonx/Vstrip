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

void KnobLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float cx = (float) x + (float) w * 0.5f;
    const auto track = juce::Rectangle<float> (cx - 2.5f, (float) y, 5.0f, (float) h);
    g.setColour (juce::Colour (0xff0f1115));
    g.fillRoundedRectangle (track, 2.5f);

    // Fill from the 0 dB centre line to the handle
    const float centreY = (float) y + (float) h * 0.5f;
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (cx - 2.5f, juce::jmin (centreY, pos), 5.0f, std::abs (pos - centreY)), 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (juce::roundToInt (centreY), cx - 10.0f, cx + 10.0f);

    // Metallic fader cap
    const auto cap = juce::Rectangle<float> (cx - 13.0f, pos - 6.0f, 26.0f, 12.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 4, { 0, 2 }).drawForRectangle (g, cap.toNearestInt());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e9ee), 0.0f, cap.getY(), juce::Colour (0xff6f7680), 0.0f, cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (accent);
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
}

//==============================================================================
// Carve display

namespace
{
    constexpr float minDb = -90.0f, maxDb = 0.0f, maxCarveDb = SpectralCarver::maxDepthDb;

    float xForHz (juce::Rectangle<float> r, float hz)
    {
        return r.getX() + r.getWidth() * std::log (hz / CarveDisplayData::minHz) / std::log (CarveDisplayData::maxHz / CarveDisplayData::minHz);
    }
}

void CarveView::update()
{
    const auto& d = processor.getDisplay();
    float loudest = -200.0f;
    for (size_t i = 0; i < (size_t) numPoints; ++i)
    {
        // Rise fast, fall slowly, so the picture is readable
        auto follow = [] (float& s, float t) { s = t > s ? 0.5f * s + 0.5f * t : 0.85f * s + 0.15f * t; };
        follow (beatIn[i], d.beatIn[i].load());
        follow (beatOut[i], d.beatOut[i].load());
        follow (vocal[i], d.vocal[i].load());
        reduction[i] = 0.6f * reduction[i] + 0.4f * d.reductionDb[i].load();
        loudest = juce::jmax (loudest, d.beatIn[i].load());
    }
    hasSidechain = loudest > -100.0f;
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
    auto depthStrip = area.removeFromBottom (58.0f);
    area.removeFromBottom (8.0f);
    auto spectrum = area;

    const auto& apvts = processor.apvts;
    const float lowCut = apvts.getRawParameterValue ("carveLow")->load();
    const float highCut = apvts.getRawParameterValue ("carveHigh")->load();

    // Spectra are tilted +3 dB/octave around 1 kHz so music reads flat-ish
    auto yForDb = [&] (float db, float hz)
    {
        const float tilted = db + 3.0f * std::log2 (hz / 1000.0f);
        return juce::jmap (juce::jlimit (minDb, maxDb, tilted), minDb, maxDb, spectrum.getBottom(), spectrum.getY());
    };
    auto yForCarve = [&] (float db)
    {
        return juce::jmap (juce::jlimit (-maxCarveDb, 0.0f, db), 0.0f, -maxCarveDb, depthStrip.getY(), depthStrip.getBottom());
    };

    // Grid
    g.setFont (juce::FontOptions (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForHz (spectrum, hz);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine (juce::roundToInt (x), spectrum.getY(), depthStrip.getBottom());
        g.setColour (textDim);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, labels.getY(), 40.0f, labels.getHeight()), juce::Justification::centred);
    }
    for (float db = -80.0f; db <= 0.0f; db += 20.0f)
    {
        const float y = juce::jmap (db, minDb, maxDb, spectrum.getBottom(), spectrum.getY());
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (juce::roundToInt (y), spectrum.getX(), spectrum.getRight());
        g.setColour (textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (spectrum.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }
    g.setColour (juce::Colour (0xff181b21));
    g.fillRect (depthStrip);
    g.setColour (textDim);
    g.drawText ("0", juce::Rectangle<float> (depthStrip.getX() - 32.0f, depthStrip.getY() - 2.0f, 28.0f, 12.0f), juce::Justification::centredRight);
    g.drawText ("-" + juce::String ((int) maxCarveDb), juce::Rectangle<float> (depthStrip.getX() - 32.0f, depthStrip.getBottom() - 10.0f, 28.0f, 12.0f), juce::Justification::centredRight);

    // Build curves
    juce::Path inLine, outLine, vocalLine, carvedArea, beatFill, depthFill;
    for (int i = 0; i < numPoints; ++i)
    {
        const float hz = CarveDisplayData::frequencyForPoint (i);
        const float x = xForHz (spectrum, hz);
        const float yIn = yForDb (beatIn[(size_t) i], hz), yOut = yForDb (beatOut[(size_t) i], hz), yVoc = yForDb (vocal[(size_t) i], hz);
        if (i == 0)
        {
            inLine.startNewSubPath (x, yIn);
            outLine.startNewSubPath (x, yOut);
            vocalLine.startNewSubPath (x, yVoc);
            depthFill.startNewSubPath (x, depthStrip.getY());
        }
        else
        {
            inLine.lineTo (x, yIn);
            outLine.lineTo (x, yOut);
            vocalLine.lineTo (x, yVoc);
        }
        depthFill.lineTo (x, yForCarve (reduction[(size_t) i]));
    }
    depthFill.lineTo (spectrum.getRight(), depthStrip.getY());
    depthFill.closeSubPath();

    beatFill = outLine;
    beatFill.lineTo (spectrum.getRight(), spectrum.getBottom());
    beatFill.lineTo (spectrum.getX(), spectrum.getBottom());
    beatFill.closeSubPath();

    // Area between the original beat and the carved beat = what was removed
    carvedArea = inLine;
    for (int i = numPoints - 1; i >= 0; --i)
    {
        const float hz = CarveDisplayData::frequencyForPoint (i);
        carvedArea.lineTo (xForHz (spectrum, hz), yForDb (beatOut[(size_t) i], hz));
    }
    carvedArea.closeSubPath();

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (spectrum.toNearestInt());
        g.setGradientFill (juce::ColourGradient (beatBlue.withAlpha (0.45f), 0.0f, spectrum.getY(), beatBlue.withAlpha (0.08f), 0.0f, spectrum.getBottom(), false));
        g.fillPath (beatFill);
        g.setColour (carveRed.withAlpha (0.75f));
        g.fillPath (carvedArea);
        g.setColour (beatBlue);
        g.strokePath (outLine, juce::PathStrokeType (1.4f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (inLine, juce::PathStrokeType (0.8f));
        g.setColour (vocalGold);
        g.strokePath (vocalLine, juce::PathStrokeType (2.0f));
    }

    g.setGradientFill (juce::ColourGradient (carveRed.withAlpha (0.35f), 0.0f, depthStrip.getY(), carveRed.withAlpha (0.9f), 0.0f, depthStrip.getBottom(), false));
    g.fillPath (depthFill);
    g.setColour (carveRed);
    g.strokePath (depthFill, juce::PathStrokeType (1.2f));

    // Dim everything outside the carve range
    const float xLow = xForHz (spectrum, lowCut), xHigh = xForHz (spectrum, highCut);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (spectrum.getX(), spectrum.getY(), xLow, depthStrip.getBottom()));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xHigh, spectrum.getY(), spectrum.getRight(), depthStrip.getBottom()));
    const float dashes[] = { 4.0f, 3.0f };
    g.setColour (carveRed.withAlpha (0.9f));
    for (float x : { xLow, xHigh })
        g.drawDashedLine ({ x, spectrum.getY(), x, depthStrip.getBottom() }, dashes, 2, 1.2f);
    auto hzText = [] (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; };
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("LOW CUT " + hzText (lowCut), juce::Rectangle<float> (xLow + 4.0f, spectrum.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("HIGH CUT " + hzText (highCut), juce::Rectangle<float> (xHigh - 114.0f, spectrum.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredRight);
    g.setColour (textDim);
    g.drawText ("CARVE DEPTH", depthStrip.reduced (6.0f, 4.0f), juce::Justification::bottomLeft);

    // Legend
    auto legend = juce::Rectangle<float> (spectrum.getX() + 10.0f, spectrum.getY() + 20.0f, 420.0f, 14.0f);
    auto item = [&] (juce::Colour c, const juce::String& text, bool line)
    {
        auto swatch = legend.removeFromLeft (14.0f).reduced (1.0f, line ? 6.0f : 2.0f);
        g.setColour (c);
        g.fillRect (swatch);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (juce::FontOptions (11.0f));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 18.0f;
        g.drawText (text, legend.removeFromLeft (w).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
    };
    item (beatBlue, "Beat (sidechain)", false);
    item (carveRed, "Carved out", false);
    item (vocalGold, "Vocal", true);

    if (! hasSidechain)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No beat detected - route your beat into this plugin's sidechain input",
                    spectrum, juce::Justification::centred);
    }
}

//==============================================================================
// Editor

VocalSitterEditor::VocalSitterEditor (VocalSitterProcessor& p)
    : AudioProcessorEditor (p), processor (p), carveView (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (carveView);

    amount  = makeKnob ("carveAmount", "CARVE AMOUNT", carveRed);
    release = makeKnob ("carveRelease", "Release", carveRed);
    lowCut  = makeKnob ("carveLow", "Low Cut", carveRed);
    highCut = makeKnob ("carveHigh", "High Cut", carveRed);
    amount->label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    amount->label.setColour (juce::Label::textColourId, juce::Colours::white);

    for (int b = 0; b < VocalEq::numBands; ++b)
    {
        const float f = VocalEq::frequencies[(size_t) b];
        eqFaders[(size_t) b] = makeKnob ("eq" + juce::String (b), f >= 1000.0f ? juce::String (f / 1000.0f, 1) + "k" : juce::String ((int) f),
                                         vocalGold, false);
    }

    vocalLevel = makeKnob ("vocalLevel", "Vocal", vocalGold);
    beatLevel  = makeKnob ("beatLevel", "Beat", beatBlue);
    output     = makeKnob ("outGain", "Output", mixGreen);
    beatToOutput = makeSwitch ("beatToOutput", "Beat to output", beatBlue);
    autoGain     = makeSwitch ("autoGain", "Auto gain", mixGreen);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

VocalSitterEditor::~VocalSitterEditor()
{
    setLookAndFeel (nullptr);
}

VocalSitterEditor::Control* VocalSitterEditor::makeKnob (const juce::String& id, const juce::String& name, juce::Colour accent, bool rotary)
{
    auto* c = controls.add (new Control());
    auto& s = c->slider;
    s.setSliderStyle (rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, rotary ? 80 : 52, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::trackColourId, accent);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (s);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, s);
    if (! rotary)
        s.onValueChange = [this] { repaint (eqCurveArea); };

    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::FontOptions (12.0f));
    c->label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (c->label);
    return c;
}

VocalSitterEditor::Switch* VocalSitterEditor::makeSwitch (const juce::String& id, const juce::String& name, juce::Colour accent)
{
    auto* sw = switches.add (new Switch());
    sw->button.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (sw->button);
    sw->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, sw->button);
    sw->label.setText (name, juce::dontSendNotification);
    sw->label.setFont (juce::FontOptions (12.0f));
    sw->label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (sw->label);
    return sw;
}

void VocalSitterEditor::timerCallback()
{
    auto follow = [] (float& s, float t) { s = t > s ? t : s + (t - s) * 0.2f; };
    follow (vocalDb, processor.vocalLevelDb.load());
    follow (beatDb, processor.beatLevelDb.load());
    follow (outDb, processor.outputLevelDb.load());
    autoDb += (processor.autoGainDb.load() - autoDb) * 0.2f;
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
    const float amountFilled = juce::jlimit (0.0f, 1.0f, (valueDb + 60.0f) / 60.0f);
    g.setColour (valueDb > -1.0f ? juce::Colours::red : colour);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * amountFilled), 3.0f);
}

void VocalSitterEditor::drawEqCurve (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xff0e1014));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());

    const double rate = processor.getCurrentSampleRate() > 0.0 ? processor.getCurrentSampleRate() : 44100.0;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, VocalEq::numBands> coeffs;
    for (int b = 0; b < VocalEq::numBands; ++b)
    {
        const auto c = VocalEq::coefficients (b, (float) eqFaders[(size_t) b]->slider.getValue(), rate);
        coeffs[(size_t) b] = new juce::dsp::IIR::Coefficients<float> (c[0], c[1], c[2], c[3], c[4], c[5]);
    }

    juce::Path curve;
    const int steps = juce::roundToInt (area.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const double hz = 20.0 * std::pow (1000.0, (double) i / steps);
        double mag = 1.0;
        for (auto& c : coeffs)
            mag *= c->getMagnitudeForFrequency (hz, rate);
        const float db = juce::jlimit (-14.0f, 14.0f, (float) juce::Decibels::gainToDecibels (mag));
        const float x = area.getX() + (float) i;
        const float y = juce::jmap (db, -14.0f, 14.0f, area.getBottom() - 2.0f, area.getY() + 2.0f);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (area.getRight(), area.getCentreY());
    fill.lineTo (area.getX(), area.getCentreY());
    fill.closeSubPath();
    g.setColour (vocalGold.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (vocalGold);
    g.strokePath (curve, juce::PathStrokeType (1.8f));
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
    g.drawText ("S I D E C H A I N   V O C A L   S P A C E", juce::Rectangle<float> (header.getX() + 2.0f, baseline + 14.0f, titleWidth + 40.0f, 16.0f),
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
    drawMeter (g, cell (topRow), "VOCAL", vocalDb, vocalGold);
    drawMeter (g, cell (topRow), "BEAT", beatDb, beatBlue);
    drawMeter (g, cell (bottomRow), "OUTPUT", outDb, mixGreen);

    // Auto gain shows how much it is boosting or cutting, from the centre
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
    drawPanel (g, eqPanel, "VOCAL EQ", vocalGold);
    drawPanel (g, mixPanel, "MIX", mixGreen);
    drawEqCurve (g, eqCurveArea.toFloat());
}

void VocalSitterEditor::resized()
{
    auto area = getLocalBounds().reduced (gap);
    area.removeFromTop (headerHeight - gap);
    carveView.setBounds (area.removeFromTop (250));
    area.removeFromTop (gap);

    carvePanel = area.removeFromLeft (340);
    area.removeFromLeft (gap);
    mixPanel = area.removeFromRight (230);
    area.removeFromRight (gap);
    eqPanel = area;

    auto place = [] (Control* c, juce::Rectangle<int> r)
    {
        c->label.setBounds (r.removeFromTop (16));
        c->slider.setBounds (r);
    };

    // Carve: big Amount knob on the left, three smaller knobs on the right
    {
        auto r = carvePanel.withTrimmedTop (34).reduced (8, 6);
        place (amount, r.removeFromLeft (150).withSizeKeepingCentre (150, 170));

        // Release on top, Low/High Cut side by side below
        auto top = r.removeFromTop (r.getHeight() / 2);
        place (release, top.withSizeKeepingCentre (90, top.getHeight()));
        place (lowCut, r.removeFromLeft (r.getWidth() / 2));
        place (highCut, r);
    }

    // EQ: response curve on top, 7 faders below
    {
        auto r = eqPanel.withTrimmedTop (34).reduced (10, 6);
        eqCurveArea = r.removeFromTop (56);
        r.removeFromTop (6);
        const int w = r.getWidth() / VocalEq::numBands;
        for (auto* f : eqFaders)
        {
            auto col = r.removeFromLeft (w);
            f->label.setBounds (col.removeFromBottom (16));
            f->slider.setBounds (col);
        }
    }

    // Mix: Vocal and Beat levels, then Output and the switches
    {
        auto r = mixPanel.withTrimmedTop (34).reduced (8, 6);
        auto top = r.removeFromTop (r.getHeight() / 2);
        place (vocalLevel, top.removeFromLeft (top.getWidth() / 2));
        place (beatLevel, top);
        place (output, r.removeFromLeft (r.getWidth() / 2));
        r.removeFromTop (14);
        for (auto* sw : { beatToOutput, autoGain })
        {
            auto row = r.removeFromTop (44);
            sw->label.setBounds (row.removeFromTop (18));
            sw->button.setBounds (row.removeFromTop (22).withWidth (52));
        }
    }
}
