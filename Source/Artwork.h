#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared brand artwork: 3D title lettering and the studio mic.

// Draws text as raised 3D lettering: soft shadow, a stepped extrusion, a metallic
// gradient face and a thin bevel highlight along the top edges.
inline void draw3DText (juce::Graphics& g, const juce::String& text, juce::Font font, juce::Point<float> origin,
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
inline void drawCondenserMic (juce::Graphics& g, juce::Rectangle<float> area)
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
