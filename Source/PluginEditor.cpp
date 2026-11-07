#include "PluginEditor.h"
#include "Artwork.h"
#include <BinaryData.h>

namespace
{
    constexpr int gap = 12;

    juce::Point<int> layoutSize (int layout)
    {
        switch (layout)
        {
            case VocalSitterEditor::compactLayout: return { 720, 400 };
            case VocalSitterEditor::bigKnob:       return { 760, 820 };
            case VocalSitterEditor::rack:          return { 1240, 320 };
            case VocalSitterEditor::strip:         return { 400, 1000 };
            default:                               return { 1120, 660 };
        }
    }

    juce::Colour contrastText (juce::Colour background)
    {
        return background.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff111111) : juce::Colours::white;
    }

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

//==============================================================================
// Look and feel

void SitterLookAndFeel::setSkin (const Skin& s)
{
    current = &s;
    setColour (juce::PopupMenu::backgroundColourId, s.panel);
    setColour (juce::PopupMenu::textColourId, s.text);
    setColour (juce::PopupMenu::headerTextColourId, s.dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, s.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, contrastText (s.accent));
}

void SitterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& sl)
{
    const auto& s = *current;
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = sl.findColour (juce::Slider::rotarySliderFillColourId);
    const float thickness = juce::jlimit (3.0f, 8.0f, radius * 0.08f);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (s.track);
    g.strokePath (track, stroke);
    if (s.glow)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (value, juce::PathStrokeType (thickness * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (accent);
    g.strokePath (value, stroke);

    const float capR = radius - thickness - 4.0f;
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), (int) (capR * 0.25f) + 2, { 0, 3 })
        .drawForPath (g, [&] { juce::Path p; p.addEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f); return p; }());
    g.setGradientFill (juce::ColourGradient (s.capLight, centre.x - capR * 0.4f, centre.y - capR * 0.5f,
                                             s.capDark, centre.x + capR * 0.5f, centre.y + capR * 0.8f, true));
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

    juce::Path pointer;
    const float pw = juce::jlimit (2.5f, 6.0f, capR * 0.09f);
    pointer.addRoundedRectangle (-pw * 0.5f, -capR + capR * 0.15f, pw, capR * 0.5f, pw * 0.5f);
    g.setColour (s.pointer);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void SitterLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto& s = *current;
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto fill = on ? s.accent2 : s.track;
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (on ? contrastText (fill) : s.dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
}

void SitterLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    const auto& s = *current;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool isListen = b.getButtonText().startsWith ("LISTEN");
    const bool on = b.getToggleState();
    g.setColour (on ? (isListen ? s.cut : s.accent2) : s.track);
    g.fillRoundedRectangle (r, 5.0f);
    if (s.glow && on)
    {
        g.setColour (s.accent.withAlpha (0.5f));
        g.drawRoundedRectangle (r.expanded (1.0f), 6.0f, 2.0f);
    }
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

//==============================================================================
// Carve display

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
        auto follow = [] (float& v, float t) { v = t > v ? t : 0.8f * v + 0.2f * t; };
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
    switch (style)
    {
        case Style::ring:     paintRing (g); break;
        case Style::sideways: paintSideways (g); break;
        case Style::bars:
        default:              paintBars (g); break;
    }
}

void CarveView::paintRoutingLabel (juce::Graphics& g, juce::Rectangle<float> row)
{
    const auto& s = *skin;
    auto pill = [&] (juce::Rectangle<float> r, const juce::String& text, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.15f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (c);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setFont (juce::FontOptions (r.getHeight() * 0.6f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    };
    const juce::String keyText = ! connected ? "SIDECHAIN: NOT CONNECTED" : hasVocal ? "SIDECHAIN (VOCAL): SIGNAL OK" : "SIDECHAIN (VOCAL): NO SIGNAL";
    const auto okColour = s.isLightDisplay() ? s.accent : s.vocal;
    const float scale = row.getHeight() / 18.0f;
    pill (row.removeFromRight (230.0f * scale), keyText, connected && hasVocal ? okColour : s.cut);
    g.setColour (s.displayText);
    g.setFont (juce::FontOptions (11.0f * scale, juce::Font::bold));
    g.drawText ("CARVED BY", row.removeFromRight (78.0f * scale), juce::Justification::centred);
    pill (row.removeFromRight (150.0f * scale), "CARVING: THIS TRACK", s.isLightDisplay() ? s.text : s.beat);
}

void CarveView::paintBars (juce::Graphics& g)
{
    const auto& s = *skin;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (s.display);
    g.fillRoundedRectangle (bounds, 8.0f);
    if (s.glow)
    {
        g.setColour (s.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);
    }

    auto area = bounds.reduced (compact ? 8.0f : 12.0f, compact ? 6.0f : 10.0f);
    area.removeFromLeft (compact ? 4.0f : 30.0f);
    auto labels = area.removeFromBottom (compact ? 0.0f : 16.0f);
    auto depth = area.removeFromBottom (compact ? 0.0f : 58.0f);
    if (! compact)
        area.removeFromBottom (8.0f);
    auto bars = area;

    const auto& apvts = processor.apvts;
    const float lowCut = apvts.getRawParameterValue ("lowCut")->load();
    const float highCut = apvts.getRawParameterValue ("highCut")->load();
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, bars.getBottom(), bars.getY()); };
    const float slot = bars.getWidth() / (float) numBands;
    auto xForBand = [&] (float b) { return bars.getX() + slot * b; };
    auto xForHz = [&] (float hz) { return xForBand ((float) numBands * std::log (hz / 20.0f) / std::log (1000.0f)); };
    const auto grid = s.displayText.withAlpha (0.12f);

    if (! compact)
    {
        g.setFont (juce::FontOptions (10.0f));
        for (float db = -60.0f; db <= 0.0f; db += 12.0f)
        {
            const float y = yForDb (db);
            g.setColour (grid);
            g.drawHorizontalLine (juce::roundToInt (y), bars.getX(), bars.getRight());
            g.setColour (s.displayText);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (bars.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
        }
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xForHz (hz);
            g.setColour (s.displayText);
            g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 20.0f, labels.getY(), 40.0f, labels.getHeight()), juce::Justification::centred);
        }
        g.setColour (s.displayText.withAlpha (0.08f));
        g.fillRect (depth);
    }

    for (int b = 0; b < numBands; ++b)
    {
        const auto i = (size_t) b;
        const float x = xForBand ((float) b) + 2.0f, w = slot - 4.0f;
        const float top = yForDb (beat[i]);
        const float carvedTop = yForDb (beat[i] + cut[i]);
        g.setGradientFill (juce::ColourGradient (s.beat, 0.0f, bars.getY(), s.beat.withAlpha (0.3f), 0.0f, bars.getBottom(), false));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, carvedTop, x + w, bars.getBottom()));
        if (carvedTop > top + 0.5f)
        {
            g.setColour (s.cut.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, top, x + w, carvedTop));
        }
        if (vocal[i] > minDb)
        {
            g.setColour (s.vocal);
            g.fillRect (x - 1.0f, yForDb (vocal[i]) - 1.5f, w + 2.0f, 3.0f);
        }
        if (! compact)
        {
            const float depthY = juce::jmap (juce::jlimit (-maxCut, 0.0f, cut[i]), 0.0f, -maxCut, depth.getY(), depth.getBottom());
            g.setGradientFill (juce::ColourGradient (s.cut.withAlpha (0.5f), 0.0f, depth.getY(), s.cut, 0.0f, depth.getBottom(), false));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, depth.getY(), x + w, depthY));
        }
    }

    // Dim the bands outside the carve range
    const float xLow = xForHz (lowCut), xHigh = xForHz (highCut);
    const auto shade = s.isLightDisplay() ? s.display.withAlpha (0.7f) : juce::Colours::black.withAlpha (0.55f);
    g.setColour (shade);
    const float bottom = compact ? bars.getBottom() : depth.getBottom();
    if (xLow > bars.getX())
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bars.getX(), bars.getY(), xLow, bottom));
    if (xHigh < bars.getRight())
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xHigh, bars.getY(), bars.getRight(), bottom));
    const float dashes[] = { 4.0f, 3.0f };
    g.setColour (s.cut.withAlpha (0.9f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    if (xLow > bars.getX() + 1.0f)
    {
        g.drawDashedLine ({ xLow, bars.getY(), xLow, bottom }, dashes, 2, 1.2f);
        if (! compact)
            g.drawText ("LOW CUT " + hzText (lowCut), juce::Rectangle<float> (xLow + 4.0f, bars.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredLeft);
    }
    if (xHigh < bars.getRight() - 1.0f)
    {
        g.drawDashedLine ({ xHigh, bars.getY(), xHigh, bottom }, dashes, 2, 1.2f);
        if (! compact)
            g.drawText ("HIGH CUT " + hzText (highCut), juce::Rectangle<float> (xHigh - 114.0f, bars.getY() + 2.0f, 110.0f, 12.0f), juce::Justification::centredRight);
    }

    if (compact)
    {
        g.setColour (connected && hasVocal ? s.vocal : s.cut);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (connected && hasVocal ? "SIDECHAIN OK" : (connected ? "NO VOCAL SIGNAL" : "SIDECHAIN NOT CONNECTED"),
                    bars.reduced (4.0f).withHeight (12.0f), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (s.displayText);
        g.drawText ("CARVE DEPTH", depth.reduced (6.0f, 4.0f), juce::Justification::bottomLeft);

        auto legend = juce::Rectangle<float> (bars.getX() + 10.0f, bars.getY() + 18.0f, 460.0f, 14.0f);
        auto item = [&] (juce::Colour c, const juce::String& text, bool line)
        {
            auto swatch = legend.removeFromLeft (14.0f).reduced (1.0f, line ? 5.5f : 2.0f);
            g.setColour (c);
            g.fillRect (swatch);
            g.setColour (s.isLightDisplay() ? s.text : juce::Colours::white.withAlpha (0.8f));
            g.setFont (juce::FontOptions (11.0f));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 18.0f;
            g.drawText (text, legend.removeFromLeft (tw).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
        };
        item (s.beat, "Beat", false);
        item (s.cut, "Carved out", false);
        item (s.vocal, "Vocal (sidechain)", true);
        paintRoutingLabel (g, juce::Rectangle<float> (bars.getRight() - 470.0f, bars.getY() + 16.0f, 460.0f, 18.0f));
    }

    if (listening)
    {
        auto banner = juce::Rectangle<float> (compact ? 220.0f : 300.0f, 24.0f).withCentre ({ bars.getCentreX(), bars.getY() + (compact ? 30.0f : 56.0f) });
        g.setColour (s.cut);
        g.fillRoundedRectangle (banner, 6.0f);
        g.setColour (contrastText (s.cut));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("LISTENING TO THE CARVED PART", banner, juce::Justification::centred);
    }
    else if (! hasVocal && ! compact)
    {
        g.setColour (s.isLightDisplay() ? s.text : juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No vocal detected - route your vocal into this plugin's sidechain input", bars, juce::Justification::centred);
    }
}

void CarveView::paintRing (juce::Graphics& g)
{
    const auto& s = *skin;
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;
    const float len = outer * 0.4f, r0 = outer - len;
    const float a0 = juce::MathConstants<float>::pi * 0.75f, span = juce::MathConstants<float>::pi * 1.5f;
    const float barWidth = (r0 + len * 0.5f) * span / (float) numBands * 0.72f;
    auto n = [&] (float db) { return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)) * len; };

    for (int b = 0; b < numBands; ++b)
    {
        const auto i = (size_t) b;
        const float a = a0 + span * ((float) b + 0.5f) / (float) numBands;
        const float cx = std::cos (a), cy = std::sin (a);
        auto seg = [&] (float from, float to, juce::Colour c, float w)
        {
            if (to <= from) return;
            g.setColour (c);
            g.drawLine (centre.x + cx * (r0 + from), centre.y + cy * (r0 + from), centre.x + cx * (r0 + to), centre.y + cy * (r0 + to), w);
        };
        seg (0.0f, len, s.displayText.withAlpha (0.1f), barWidth);
        seg (0.0f, n (beat[i] + cut[i]), s.beat, barWidth);
        seg (n (beat[i] + cut[i]), n (beat[i]), s.cut, barWidth);
        if (vocal[i] > minDb)
            seg (n (vocal[i]) - 1.5f, n (vocal[i]) + 1.5f, s.vocal, barWidth + 4.0f);
    }

    g.setColour (s.bgDim);
    g.setFont (juce::FontOptions (11.0f));
    const float ly = centre.y + std::sin (a0) * outer + 14.0f;
    g.drawText ("20 Hz", juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ centre.x + std::cos (a0) * (r0 + len * 0.5f), ly }), juce::Justification::centred);
    g.drawText ("20 kHz", juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ centre.x - std::cos (a0) * (r0 + len * 0.5f), ly }), juce::Justification::centred);

    const juce::String status = listening ? "LISTENING TO THE CARVED PART"
                              : ! connected ? "SIDECHAIN NOT CONNECTED"
                              : hasVocal ? "CARVING THIS TRACK  |  SIDECHAIN OK" : "SIDECHAIN: NO VOCAL SIGNAL";
    g.setColour (listening || ! (connected && hasVocal) ? s.cut : s.vocal);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (status, juce::Rectangle<float> (bounds.getWidth(), 16.0f).withCentre ({ centre.x, ly + 4.0f }), juce::Justification::centred);
}

void CarveView::paintSideways (juce::Graphics& g)
{
    const auto& s = *skin;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (s.display);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto area = bounds.reduced (10.0f);
    auto status = area.removeFromTop (18.0f);
    area.removeFromTop (6.0f);
    const float labelW = 40.0f;
    auto labelCol = area.removeFromLeft (labelW);
    const float h = area.getHeight() / (float) numBands, len = area.getWidth() - 6.0f;
    auto n = [&] (float db) { return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)) * len; };

    for (int b = 0; b < numBands; ++b)
    {
        const auto i = (size_t) b;
        const float y = area.getY() + (float) (numBands - 1 - b) * h;
        g.setColour (s.beat.withAlpha (0.8f));
        g.fillRect (area.getX(), y + 1.0f, n (beat[i] + cut[i]), h - 2.0f);
        g.setColour (s.cut);
        g.fillRect (area.getX() + n (beat[i] + cut[i]), y + 1.0f, juce::jmax (0.0f, n (beat[i]) - n (beat[i] + cut[i])), h - 2.0f);
        if (vocal[i] > minDb)
        {
            g.setColour (s.vocal);
            g.fillRect (area.getX() + n (vocal[i]) - 1.5f, y - 1.0f, 3.0f, h + 2.0f);
        }
    }
    g.setColour (s.displayText);
    g.setFont (juce::FontOptions (10.0f));
    for (auto [band, text] : { std::pair { 0, "20" }, { 6, "80" }, { 12, "300" }, { 18, "1k" }, { 24, "4k" }, { 30, "15k" } })
        g.drawText (text, juce::Rectangle<float> (labelCol.getX(), area.getY() + (float) (numBands - 1 - band) * h, labelW - 6.0f, h * 2.0f),
                    juce::Justification::centredRight);

    const bool ok = connected && hasVocal;
    g.setColour (listening ? s.cut : ok ? (s.isLightDisplay() ? s.accent : s.vocal) : s.cut);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (listening ? "LISTENING TO THE CARVED PART" : ok ? "CARVING THIS TRACK  |  SIDECHAIN OK"
                                                            : (connected ? "SIDECHAIN: NO VOCAL SIGNAL" : "SIDECHAIN NOT CONNECTED"),
                status, juce::Justification::centred);
}

//==============================================================================
// VU meter

void VuMeter::paint (juce::Graphics& g)
{
    const auto& s = *skin;
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1d1f23));
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (8.0f);
    const auto faceColour = s.isLightDisplay() ? s.display : (s.glow ? s.display : juce::Colour (0xfff3d9a0));
    g.setColour (faceColour);
    g.fillRoundedRectangle (face, 4.0f);
    const bool brightFace = faceColour.getPerceivedBrightness() > 0.5f;
    const auto ink = brightFace ? juce::Colour (0xff3b2a10) : s.displayText;

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
    const float radius = face.getHeight() * 1.05f;
    const float a0 = -0.82f, a1 = 0.82f; // radians either side of straight up
    auto at = [&] (float a, float rr) { return pivot + juce::Point<float> (std::sin (a) * rr, -std::cos (a) * rr); };

    for (int t = 0; t <= 10; ++t)
    {
        const float a = a0 + (a1 - a0) * (float) t / 10.0f;
        g.setColour (t > 7 ? s.cut : ink);
        g.drawLine ({ at (a, radius - 12.0f), at (a, radius) }, 2.0f);
    }
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius - 2.0f, radius - 2.0f, 0.0f, a0, a1, true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f));

    g.setColour (ink);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (label, face.withTrimmedTop (face.getHeight() * 0.55f), juce::Justification::centredTop);

    const float a = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, value);
    g.setColour (brightFace ? juce::Colour (0xff111111) : s.accent);
    g.drawLine ({ at (a, 10.0f), at (a, radius + 4.0f) }, 2.0f);
}

//==============================================================================
// Editor

VocalSitterEditor::VocalSitterEditor (VocalSitterProcessor& p)
    : AudioProcessorEditor (p), processor (p), carveView (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (carveView);
    addChildComponent (beatVu);
    addChildComponent (carveVu);

    amount  = makeKnob ("amount", "AMOUNT");
    attack  = makeKnob ("attack", "Attack");
    release = makeKnob ("release", "Release");
    lowCut  = makeKnob ("lowCut", "Low Cut");
    highCut = makeKnob ("highCut", "High Cut");
    output  = makeKnob ("outGain", "Output");
    amount->label.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    for (auto* l : { &stereoLabel, &autoGainLabel })
    {
        l->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (*l);
    }
    stereoLabel.setText ("Carve the beat's", juce::dontSendNotification);
    autoGainLabel.setText ("Auto gain", juce::dontSendNotification);

    const juce::StringArray modes { "Stereo", "Mid", "Side" };
    for (int i = 0; i < modes.size(); ++i)
    {
        auto* b = stereoButtons.add (new juce::TextButton (modes[i]));
        b->onClick = [this, i] { setStereoMode (i); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "autoGain", autoGainButton);

    listenButton.setClickingTogglesState (true);
    listenButton.setTooltip ("Hear only what is being carved out of this track");
    addAndMakeVisible (listenButton);
    listenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "listen", listenButton);

    lookButton.setTooltip ("Choose a layout and skin");
    lookButton.onClick = [this] { showLookMenu(); };
    addAndMakeVisible (lookButton);

    // Look: saved with the project, otherwise the last look you picked anywhere
    juce::PropertiesFile::Options options;
    options.applicationName = "Rashon Vocal Sitter";
    options.filenameSuffix = "settings";
    options.folderName = "Dodge City Plugins";
    options.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (options);

    const auto& state = processor.apvts.state;
    const int savedLayout = state.hasProperty ("layout") ? (int) state.getProperty ("layout") : settings->getIntValue ("layout", standard);
    const int savedSkin = state.hasProperty ("skin") ? (int) state.getProperty ("skin") : settings->getIntValue ("skin", 0);
    setLook (savedLayout, savedSkin);

    timerCallback();
    startTimerHz (30);
}

VocalSitterEditor::~VocalSitterEditor()
{
    setLookAndFeel (nullptr);
}

VocalSitterEditor::Control* VocalSitterEditor::makeKnob (const juce::String& id, const juce::String& name)
{
    auto* c = controls.add (new Control());
    auto& sl = c->slider;
    sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    sl.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (sl);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, sl);
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (c->label);
    return c;
}

void VocalSitterEditor::setLook (int layoutIndex, int newSkin)
{
    layout = juce::jlimit (0, numLayouts - 1, layoutIndex);
    skinIndex = juce::jlimit (0, (int) Skin::all().size() - 1, newSkin);

    processor.apvts.state.setProperty ("layout", layout, nullptr);
    processor.apvts.state.setProperty ("skin", skinIndex, nullptr);
    if (settings != nullptr)
    {
        settings->setValue ("layout", layout);
        settings->setValue ("skin", skinIndex);
        settings->saveIfNeeded();
    }

    const auto& s = Skin::all()[(size_t) skinIndex];
    lnf.setSkin (s);
    carveView.setLook (layout == bigKnob ? CarveView::Style::ring : layout == strip ? CarveView::Style::sideways : CarveView::Style::bars, s);
    carveView.setCompact (layout == compactLayout);
    beatVu.setLook (s, "BEAT  VU");
    carveVu.setLook (s, "CARVE  dB");
    applySkinColours();

    const auto size = layoutSize (layout);
    if (getWidth() == size.x && getHeight() == size.y)
        resized();
    else
        setSize (size.x, size.y);
    sendLookAndFeelChange();
    repaint();
}

void VocalSitterEditor::applySkinColours()
{
    const auto& s = lnf.skin();
    const bool onBackground = layout == rack; // rack knobs sit straight on the faceplate
    const auto labelColour = onBackground ? s.bgDim : s.dim;
    const auto valueColour = onBackground ? s.bgText : s.text;

    for (auto* c : controls)
    {
        c->slider.setColour (juce::Slider::rotarySliderFillColourId, c == output ? s.accent2 : s.accent);
        c->slider.setColour (juce::Slider::textBoxTextColourId, valueColour);
        c->label.setColour (juce::Label::textColourId, labelColour);
    }
    const bool amountOnBackground = layout == bigKnob || layout == rack;
    amount->label.setColour (juce::Label::textColourId, amountOnBackground ? s.bgText : valueColour);
    amount->slider.setColour (juce::Slider::textBoxTextColourId, amountOnBackground ? s.bgText : valueColour);
    for (auto* l : { &stereoLabel, &autoGainLabel })
        l->setColour (juce::Label::textColourId, labelColour);
    for (auto* b : stereoButtons)
    {
        b->setColour (juce::TextButton::textColourOffId, s.text);
        b->setColour (juce::TextButton::textColourOnId, contrastText (s.accent2));
    }
    listenButton.setColour (juce::TextButton::textColourOffId, s.text);
    listenButton.setColour (juce::TextButton::textColourOnId, contrastText (s.cut));
    lookButton.setColour (juce::TextButton::textColourOffId, s.text);
}

void VocalSitterEditor::showLookMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Layout");
    const auto names = layoutNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (1 + i, names[i], true, i == layout);
    menu.addSectionHeader ("Skin");
    for (size_t i = 0; i < Skin::all().size(); ++i)
        menu.addItem (100 + (int) i, Skin::all()[i].name, true, (int) i == skinIndex);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lookButton).withParentComponent (this),
                        [safe = juce::Component::SafePointer<VocalSitterEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result >= 100)
                                safe->setLook (safe->layout, result - 100);
                            else
                                safe->setLook (result - 1, safe->skinIndex);
                        });
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
    auto follow = [] (float& v, float t) { v = t > v ? t : v + (t - v) * 0.2f; };
    follow (beatDb, processor.beatLevelDb.load());
    follow (vocalDb, processor.vocalLevelDb.load());
    follow (outDb, processor.outputLevelDb.load());
    autoDb += (processor.autoGainDb.load() - autoDb) * 0.2f;

    const int mode = juce::roundToInt (processor.apvts.getRawParameterValue ("stereo")->load());
    for (int i = 0; i < stereoButtons.size(); ++i)
        stereoButtons[i]->setToggleState (i == mode, juce::dontSendNotification);

    carveView.update();
    if (layout == rack)
    {
        float deepest = 0.0f;
        for (auto& c : processor.getDisplay().cutDb)
            deepest = juce::jmin (deepest, c.load());
        beatVu.setValue ((beatDb + 40.0f) / 40.0f);
        carveVu.setValue (-deepest / BandCarver::maxDepthDb);
        repaint (meterArea);
    }
    else if (! meterArea.isEmpty())
        repaint (meterArea);
}

//==============================================================================
// Drawing helpers

void VocalSitterEditor::paintTitle (juce::Graphics& g, juce::Point<float> at, float size, bool raised)
{
    const auto& s = lnf.skin();
    const juce::Font font (juce::FontOptions (size, juce::Font::bold));
    const juce::String first = "Rashon ", second = "Vocal Sitter";
    const float firstWidth = juce::GlyphArrangement::getStringWidth (font, first);
    if (raised)
    {
        draw3DText (g, first, font, at, s.titleA.brighter (0.4f), s.titleA, s.titleA.darker (0.5f), s.titleA.darker (0.8f));
        draw3DText (g, second, font, { at.x + firstWidth, at.y }, s.titleB.brighter (0.5f), s.titleB, s.titleB.darker (0.5f), s.titleB.darker (0.9f));
    }
    else
    {
        g.setFont (font);
        g.setColour (s.titleA);
        g.drawSingleLineText (first, juce::roundToInt (at.x), juce::roundToInt (at.y));
        g.setColour (s.titleB);
        g.drawSingleLineText (second, juce::roundToInt (at.x + firstWidth), juce::roundToInt (at.y));
    }
}

void VocalSitterEditor::paintLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto logo = juce::ImageCache::getFromMemory (BinaryData::dodge_city_logo_png, BinaryData::dodge_city_logo_pngSize);
    g.drawImage (logo, area, juce::RectanglePlacement::centred);
}

void VocalSitterEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, float valueDb, juce::Colour colour, bool centred)
{
    const auto& s = lnf.skin();
    g.setColour (s.bgDim);
    g.setFont (juce::FontOptions (11.0f));
    const auto valueText = centred ? (valueDb >= 0.0f ? "+" : "") + juce::String (valueDb, 1) : juce::String (valueDb, 1);
    g.drawText (name + "  " + valueText + " dB", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto bar = area.removeFromTop (10.0f);
    g.setColour (s.bgDim.withAlpha (0.25f));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (colour);
    if (centred)
    {
        const float half = bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, valueDb / 12.0f);
        g.fillRoundedRectangle (half >= 0.0f ? bar.withX (bar.getCentreX()).withWidth (half) : bar.withX (bar.getCentreX() + half).withWidth (-half), 3.0f);
        g.setColour (s.bgText.withAlpha (0.5f));
        g.drawVerticalLine (juce::roundToInt (bar.getCentreX()), bar.getY() - 2.0f, bar.getBottom() + 2.0f);
    }
    else
    {
        if (valueDb > -1.0f)
            g.setColour (juce::Colours::red);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, (valueDb + 60.0f) / 60.0f)), 3.0f);
    }
}

void VocalSitterEditor::paintMeters (juce::Graphics& g, juce::Rectangle<float> area, bool twoByTwo)
{
    const auto& s = lnf.skin();
    meterArea = area.toNearestInt().expanded (2);
    const int cols = twoByTwo ? 2 : 1;
    const float colW = (area.getWidth() - (float) gap * 2.0f * (float) (cols - 1)) / (float) cols;
    const float rowH = 30.0f;
    auto cell = [&] (int index)
    {
        const int col = index % cols, row = index / cols;
        return juce::Rectangle<float> (area.getX() + (float) col * (colW + (float) gap * 2.0f), area.getY() + (float) row * (rowH + 4.0f), colW, rowH);
    };
    drawMeter (g, cell (0), "BEAT", beatDb, s.beat);
    drawMeter (g, cell (1), "VOCAL", vocalDb, s.vocal);
    drawMeter (g, cell (2), "OUTPUT", outDb, s.accent2);
    drawMeter (g, cell (3), "AUTO GAIN", autoDb, s.accent2, true);
}

void VocalSitterEditor::paintLed (juce::Graphics& g, juce::Point<float> at, juce::Colour colour, bool on, const juce::String& text)
{
    const auto& s = lnf.skin();
    if (on)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (at.x - 10.0f, at.y - 10.0f, 20.0f, 20.0f);
    }
    g.setColour (on ? colour : colour.darker (2.5f));
    g.fillEllipse (at.x - 6.0f, at.y - 6.0f, 12.0f, 12.0f);
    g.setColour (s.bgText);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText (text, juce::Rectangle<float> (80.0f, 12.0f).withCentre ({ at.x, at.y + 18.0f }), juce::Justification::centred);
}

void VocalSitterEditor::paint (juce::Graphics& g)
{
    const auto& s = lnf.skin();
    s.paintBackground (g, getLocalBounds());
    const float W = (float) getWidth();
    meterArea = {};

    for (auto& [area, title] : panels)
        s.paintPanel (g, area.toFloat(), title == "OUTPUT" || title == "MIX" ? s.accent2 : s.accent, title);

    auto versionText = [&] (juce::Rectangle<float> r, juce::Justification j)
    {
        g.setColour (s.titleB);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("v" JucePlugin_VersionString, r, j);
    };
    auto subtitle = [&] (juce::Rectangle<float> r, juce::Justification j)
    {
        g.setColour (s.bgDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("S I D E C H A I N   T R A C K   S P A C E R", r, j);
    };

    switch (layout)
    {
        case compactLayout:
            paintTitle (g, { 18.0f, 40.0f }, 26.0f, false);
            subtitle ({ 20.0f, 46.0f, 220.0f, 14.0f }, juce::Justification::centredLeft);
            versionText ({ 230.0f, 46.0f, 60.0f, 14.0f }, juce::Justification::centredLeft);
            paintLogo (g, { W - 18.0f - 64.0f - 150.0f, 12.0f, 150.0f, 48.0f });
            break;

        case bigKnob:
            paintTitle (g, { 24.0f, 50.0f }, 30.0f, true);
            subtitle ({ 26.0f, 58.0f, 240.0f, 14.0f }, juce::Justification::centredLeft);
            versionText ({ 260.0f, 58.0f, 60.0f, 14.0f }, juce::Justification::centredLeft);
            paintLogo (g, { W - 24.0f - 64.0f - 180.0f, 12.0f, 180.0f, 56.0f });
            break;

        case rack:
        {
            // Rack ears with mounting slots
            for (float ex : { 0.0f, W - 46.0f })
            {
                g.setColour (s.panelEdge);
                g.fillRect (ex, 0.0f, 46.0f, (float) getHeight());
                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.drawVerticalLine (juce::roundToInt (ex < 1.0f ? 45.0f : ex), 0.0f, (float) getHeight());
                for (float sy : { 40.0f, (float) getHeight() - 40.0f })
                {
                    g.setColour (juce::Colour (0xff2a2c30));
                    g.fillRoundedRectangle (ex + 13.0f, sy - 8.0f, 20.0f, 16.0f, 8.0f);
                }
            }
            paintLogo (g, { 60.0f, 16.0f, 210.0f, 64.0f });
            g.setColour (s.bgText);
            g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
            g.drawText ("RASHON", juce::Rectangle<float> (68.0f, 88.0f, 220.0f, 26.0f), juce::Justification::centredLeft);
            g.setColour (s.titleB);
            g.drawText ("VOCAL SITTER", juce::Rectangle<float> (68.0f, 112.0f, 220.0f, 26.0f), juce::Justification::centredLeft);
            g.setColour (s.bgDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ("SIDECHAIN TRACK SPACER  |  v" JucePlugin_VersionString, juce::Rectangle<float> (70.0f, 140.0f, 220.0f, 14.0f), juce::Justification::centredLeft);
            const bool ok = processor.sidechainConnected.load() && vocalDb > -90.0f;
            paintLed (g, { 82.0f, 186.0f }, juce::Colour (0xffffcf3a), ok, "SIDECHAIN");
            paintLed (g, { 152.0f, 186.0f }, s.cut, listenButton.getToggleState(), "LISTEN");
            paintLed (g, { 222.0f, 186.0f }, juce::Colour (0xff4cff8f), autoGainButton.getToggleState(), "AUTO GAIN");
            meterArea = { 46, 0, 260, 220 };
            g.setColour (s.bgDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("STEREO MODE", juce::Rectangle<float> (790.0f, 244.0f, 150.0f, 14.0f), juce::Justification::centredLeft);
            break;
        }

        case strip:
        {
            paintLogo (g, { 30.0f, 14.0f, 290.0f, 88.0f });
            const juce::Font f (juce::FontOptions (28.0f, juce::Font::bold));
            const float w = juce::GlyphArrangement::getStringWidth (f, "Rashon Vocal Sitter");
            paintTitle (g, { (W - w) * 0.5f, 138.0f }, 28.0f, false);
            subtitle ({ 0.0f, 146.0f, W, 14.0f }, juce::Justification::centred);
            versionText ({ 0.0f, 161.0f, W, 14.0f }, juce::Justification::centred);
            break;
        }

        case standard:
        default:
        {
            auto header = juce::Rectangle<float> (18.0f, 8.0f, W - 36.0f, 96.0f);
            paintTitle (g, { header.getX(), header.getY() + 50.0f }, 40.0f, true);
            const juce::Font tf (juce::FontOptions (40.0f, juce::Font::bold));
            const float titleWidth = juce::GlyphArrangement::getStringWidth (tf, "Rashon Vocal Sitter");
            subtitle ({ header.getX() + 2.0f, header.getY() + 64.0f, titleWidth, 16.0f }, juce::Justification::centredLeft);
            versionText ({ header.getX() + titleWidth - 60.0f, header.getY() + 64.0f, 60.0f, 16.0f }, juce::Justification::centredRight);
            const float logoH = 92.0f, logoW = logoH * 720.0f / 217.0f;
            const auto logoRect = juce::Rectangle<float> (logoW, logoH).withCentre ({ W * 0.5f, header.getCentreY() });
            paintLogo (g, logoRect);
            auto meters = header.withLeft (logoRect.getRight() + 28.0f).withTrimmedTop (16.0f);
            paintMeters (g, meters.withSizeKeepingCentre (meters.getWidth(), 64.0f), true);
            break;
        }
    }
}

//==============================================================================
// Layouts

void VocalSitterEditor::place (Control* c, juce::Rectangle<int> r, int textBoxWidth)
{
    c->label.setVisible (true);
    c->slider.setVisible (true);
    c->label.setBounds (r.removeFromTop (16));
    c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, 18);
    c->slider.setBounds (r);
}

void VocalSitterEditor::placeStereoButtons (juce::Rectangle<int> row)
{
    const int bw = row.getWidth() / 3;
    for (auto* b : stereoButtons)
        b->setBounds (row.removeFromLeft (bw).reduced (2, 0));
}

void VocalSitterEditor::resized()
{
    const int W = getWidth(), H = getHeight();
    panels.clear();
    stereoLabel.setVisible (layout == standard);
    autoGainLabel.setVisible (true);
    beatVu.setVisible (layout == rack);
    carveVu.setVisible (layout == rack);
    carveView.setVisible (layout != rack);
    amount->label.setText (layout == bigKnob ? "AMOUNT" : "AMOUNT", juce::dontSendNotification);
    listenButton.setButtonText (layout == compactLayout ? "LISTEN" : layout == rack ? "LISTEN TO CARVED" : "LISTEN TO CARVED PART");
    applySkinColours();

    switch (layout)
    {
        case compactLayout:
        {
            lookButton.setBounds (W - 76, 22, 58, 24);
            const juce::Rectangle<int> panel (12, 72, W - 24, H - 84);
            panels.push_back ({ panel, {} });
            carveView.setBounds (210, 84, panel.getRight() - 12 - 210, 150);
            place (amount, { 20, 92, 180, 200 }, 70);
            int x = 210;
            for (auto* c : { attack, release, lowCut, highCut, output })
            {
                place (c, { x, 246, 62, 132 }, 62);
                x += 62;
            }
            const int rx = 528, rw = panel.getRight() - 10 - rx;
            placeStereoButtons ({ rx, 250, rw, 24 });
            autoGainLabel.setBounds (rx, 284, rw, 18);
            autoGainButton.setBounds (rx, 302, 52, 22);
            listenButton.setBounds (rx, 340, rw, 30);
            break;
        }

        case bigKnob:
        {
            lookButton.setBounds (W - 84, 24, 60, 24);
            carveView.setBounds ((W - 580) / 2, 80, 580, 580);
            const auto c = carveView.getBounds().getCentre();
            place (amount, juce::Rectangle<int> (240, 290).withCentre ({ c.x, c.y + 14 }), 100);
            amount->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 24);
            const juce::Rectangle<int> panel (16, 668, W - 32, 136);
            panels.push_back ({ panel, {} });
            int x = 28;
            for (auto* k : { attack, release, lowCut, highCut, output })
            {
                place (k, { x, 680, 90, 116 }, 80);
                x += 90;
            }
            const int rx = x + 10, rw = panel.getRight() - 12 - rx;
            placeStereoButtons ({ rx, 684, rw, 26 });
            autoGainLabel.setBounds (rx, 716, 80, 18);
            autoGainButton.setBounds (rx + 80, 714, 52, 22);
            listenButton.setBounds (rx, 750, rw, 32);
            break;
        }

        case rack:
        {
            lookButton.setBounds (66, 262, 60, 24);
            beatVu.setBounds (300, 28, 230, 170);
            carveVu.setBounds (545, 28, 230, 170);
            place (amount, { 785, 18, 140, 200 }, 80);
            place (attack,  { 930, 14, 90, 104 }, 70);
            place (release, { 1020, 14, 90, 104 }, 70);
            place (lowCut,  { 930, 120, 90, 104 }, 70);
            place (highCut, { 1020, 120, 90, 104 }, 70);
            place (output,  { 1108, 60, 84, 120 }, 70);
            placeStereoButtons ({ 788, 262, 150, 26 });
            autoGainLabel.setBounds (300, 266, 70, 18);
            autoGainButton.setBounds (370, 264, 52, 22);
            listenButton.setBounds (980, 262, 160, 28);
            break;
        }

        case strip:
        {
            lookButton.setBounds (W - 66, 10, 56, 22);
            carveView.setBounds (16, 182, W - 32, 390);
            const juce::Rectangle<int> panel (16, 584, W - 32, H - 600);
            panels.push_back ({ panel, {} });
            place (amount, juce::Rectangle<int> (150, 190).withCentre ({ W / 2, 694 }), 80);
            int x = 25;
            for (auto* k : { attack, release, lowCut, highCut, output })
            {
                place (k, { x, 790, 70, 100 }, 66);
                x += 70;
            }
            placeStereoButtons ({ 32, 902, 170, 28 });
            autoGainLabel.setBounds (222, 896, 70, 18);
            autoGainButton.setBounds (292, 902, 52, 22);
            listenButton.setBounds (32, 940, W - 64, 32);
            break;
        }

        case standard:
        default:
        {
            lookButton.setBounds (W - 70, 4, 58, 20);
            auto area = getLocalBounds().reduced (gap);
            area.removeFromTop (112 - gap);
            carveView.setBounds (area.removeFromTop (290));
            area.removeFromTop (gap);
            const auto outputPanel = area.removeFromRight (360);
            area.removeFromRight (gap);
            const auto carvePanel = area;
            panels.push_back ({ carvePanel, "CARVE" });
            panels.push_back ({ outputPanel, "OUTPUT" });
            {
                auto r = carvePanel.withTrimmedTop (32).reduced (10, 4);
                place (amount, r.removeFromLeft (180), 80);
                r.removeFromLeft (10);
                const int w = r.getWidth() / 4;
                for (auto* c : { attack, release, lowCut, highCut })
                    place (c, r.removeFromLeft (w).withSizeKeepingCentre (w, juce::jmin (r.getHeight(), 130)), 80);
            }
            {
                auto r = outputPanel.withTrimmedTop (34).reduced (12, 6);
                place (output, r.removeFromRight (130).withSizeKeepingCentre (130, juce::jmin (r.getHeight(), 140)), 80);
                r.removeFromRight (10);
                r.removeFromTop (8);
                stereoLabel.setBounds (r.removeFromTop (18));
                placeStereoButtons (r.removeFromTop (30));
                r.removeFromTop (14);
                autoGainLabel.setBounds (r.removeFromTop (18));
                autoGainButton.setBounds (r.removeFromTop (24).withWidth (56));
                r.removeFromTop (16);
                listenButton.setBounds (r.removeFromTop (34));
            }
            break;
        }
    }
    repaint();
}
