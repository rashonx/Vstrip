#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A colour theme for the plugin window. Every layout draws itself from these colours.
struct Skin
{
    enum class Background { gradient, wood, brushed, glow };

    juce::String name;
    Background background;
    juce::Colour bg, bgAlt;           // window background (two tones for gradients and patterns)
    juce::Colour bgText, bgDim;       // text drawn straight on the background (header)
    juce::Colour panel, panelAlt, panelEdge;
    juce::Colour text, dim;           // text inside panels
    juce::Colour accent, accent2;     // carve controls, output controls
    juce::Colour beat, vocal, cut;    // display colours
    juce::Colour display, displayText;
    juce::Colour capLight, capDark, pointer, track;
    juce::Colour titleA, titleB;      // "Rashon" and "Vocal Sitter"
    bool glow = false;

    static const std::vector<Skin>& all()
    {
        using C = juce::Colour;
        static const std::vector<Skin> skins {
            { "Classic", Background::gradient, C (0xff15171c), C (0xff262a33), C (0xffe9e6df), C (0xff9aa3b2),
              C (0xff20242c), C (0xff20242c), C (0xff2a2f3a), C (0xffe9e6df), C (0xff9aa3b2),
              C (0xffff5a5f), C (0xff6ec6a8), C (0xff5fb3f0), C (0xfff2b84b), C (0xffff5a5f),
              C (0xff0e1014), C (0xff9aa3b2), C (0xff4a505c), C (0xff1c1f25), C (0xffffffff), C (0xff353b47),
              C (0xffc4cbd6), C (0xfff2b84b), false },
            { "Saloon", Background::wood, C (0xff3b2414), C (0xff2f1c0f), C (0xfff6e7c4), C (0xffd9b98a),
              C (0xfff1e2bf), C (0xffe2cc9b), C (0xff2a1a0a), C (0xff3a2410), C (0xff7a5a34),
              C (0xffc0392b), C (0xffc0392b), C (0xff3f6e8c), C (0xffe0a326), C (0xffc0392b),
              C (0xff1f140b), C (0xffb89a6a), C (0xffffe9a6), C (0xff8a5a12), C (0xffffffff), C (0xff8a6a40),
              C (0xfff6e7c4), C (0xfff2b84b), false },
            { "Neon Night", Background::glow, C (0xff07060b), C (0xff2b0d3d), C (0xffffffff), C (0xffa78bc6),
              C (0xff140c1e), C (0xff1b1027), C (0xff5a1f5a), C (0xfff3e9ff), C (0xffa78bc6),
              C (0xffff2bd6), C (0xff2bf3ff), C (0xff7a5cff), C (0xff2bf3ff), C (0xffff2bd6),
              C (0xff06040a), C (0xffa78bc6), C (0xff3a2a4f), C (0xff120b1c), C (0xffffffff), C (0xff3a2a4f),
              C (0xffffffff), C (0xffff2bd6), true },
            { "Vintage Hardware", Background::brushed, C (0xffb4b8bd), C (0xff9fa3a8), C (0xff1d1d1d), C (0xff3c3a36),
              C (0xffe9e2cf), C (0xffd8cfb6), C (0xff6e6a62), C (0xff222222), C (0xff5d564a),
              C (0xffd1342f), C (0xffd1342f), C (0xff5a4a2e), C (0xff1d1d1d), C (0xffd1342f),
              C (0xfff3d9a0), C (0xff5d4a2a), C (0xff666666), C (0xff0d0d0d), C (0xffffffff), C (0xffa8a090),
              C (0xff1d1d1d), C (0xffd1342f), false },
            { "Black & Gold", Background::gradient, C (0xff0b0a08), C (0xff26231d), C (0xfff4e3b2), C (0xffa08a5a),
              C (0xff17140f), C (0xff0f0d0a), C (0xff6b5626), C (0xfff4e3b2), C (0xffa08a5a),
              C (0xffe9b949), C (0xffe9b949), C (0xff8c7442), C (0xfffff1c2), C (0xffe9b949),
              C (0xff080706), C (0xffa08a5a), C (0xfffff3c4), C (0xff7a5208), C (0xff120d04), C (0xff3a3020),
              C (0xfff4e3b2), C (0xffe9b949), false },
            { "Clean Light", Background::gradient, C (0xffeef1f5), C (0xfffafbfc), C (0xff1d2433), C (0xff6b7486),
              C (0xffffffff), C (0xffffffff), C (0xffdfe4ec), C (0xff1d2433), C (0xff6b7486),
              C (0xff2f6bff), C (0xff2f6bff), C (0xff9db4e6), C (0xffff9f1c), C (0xffff4d5e),
              C (0xfff7f9fc), C (0xff6b7486), C (0xffffffff), C (0xffcfd5df), C (0xff2f6bff), C (0xffdfe4ec),
              C (0xff1d2433), C (0xff2f6bff), false },
        };
        return skins;
    }

    bool isLightDisplay() const { return display.getPerceivedBrightness() > 0.5f; }

    void paintBackground (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        const auto r = area.toFloat();
        switch (background)
        {
            case Background::wood:
            {
                g.setColour (bg);
                g.fillRect (area);
                juce::Random rng (11);
                for (float x = r.getX(); x < r.getRight(); x += 26.0f)
                {
                    g.setColour (bgAlt.withAlpha (0.55f));
                    g.fillRect (x, r.getY(), 1.5f, r.getHeight());
                    for (int k = 0; k < 4; ++k)
                    {
                        g.setColour (bg.brighter (0.08f + 0.08f * rng.nextFloat()).withAlpha (0.35f));
                        g.fillRect (x + 3.0f + rng.nextFloat() * 20.0f, r.getY(), 1.0f, r.getHeight());
                    }
                }
                break;
            }
            case Background::brushed:
                g.setGradientFill (juce::ColourGradient (bg.brighter (0.15f), 0.0f, r.getY(), bgAlt, 0.0f, r.getBottom(), false));
                g.fillRect (area);
                for (float y = r.getY(); y < r.getBottom(); y += 2.0f)
                {
                    g.setColour (juce::Colours::white.withAlpha (((int) y / 2) % 3 == 0 ? 0.06f : 0.0f));
                    g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
                }
                break;
            case Background::glow:
                g.fillAll (bg);
                g.setGradientFill (juce::ColourGradient (bgAlt, r.getX() + r.getWidth() * 0.2f, r.getY(),
                                                         bg, r.getX() + r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.7f, true));
                g.fillRect (area);
                break;
            case Background::gradient:
            default:
                g.setGradientFill (juce::ColourGradient (bgAlt, 0.0f, r.getY(), bg, 0.0f, r.getY() + juce::jmin (180.0f, r.getHeight()), false));
                g.fillRect (area);
                break;
        }
    }

    void paintPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour stripe, const juce::String& title = {}) const
    {
        if (glow)
        {
            g.setColour (accent.withAlpha (0.12f));
            g.fillRoundedRectangle (r.expanded (3.0f), 10.0f);
        }
        juce::DropShadow (juce::Colours::black.withAlpha (isLightDisplay() || background == Background::brushed ? 0.15f : 0.35f), 8, { 0, 3 })
            .drawForRectangle (g, r.toNearestInt());
        g.setGradientFill (juce::ColourGradient (panel, 0.0f, r.getY(), panelAlt, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (stripe);
        g.fillRoundedRectangle (r.withHeight (3.0f).reduced (8.0f, 0.0f), 1.5f);
        if (title.isNotEmpty())
        {
            g.setColour (text);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (title, r.reduced (12.0f, 8.0f).withHeight (20.0f), juce::Justification::centredLeft);
        }
    }
};
