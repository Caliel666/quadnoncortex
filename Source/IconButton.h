#pragma once
#include <JuceHeader.h>
#include "Theme.h"

/** Square tile button that draws a vector icon (no emoji / truncated text). */
class IconButton : public juce::Button
{
public:
    enum class Icon { Save, Pencil, Gear, Plus, ChevronUp, ChevronDown, None };

    IconButton (Icon ic = Icon::None, const juce::String& textFallback = {}, bool flatMode = false)
        : juce::Button ({}), icon (ic), label (textFallback), flat (flatMode)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setIcon (Icon ic) { icon = ic; repaint(); }
    void setLabel (const juce::String& t) { label = t; repaint(); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto& th = Theme::get();
        auto bounds = getLocalBounds().toFloat().reduced (flat ? 0.0f : 1.5f);
        const float rad = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.22f;

        if (! flat)
        {
            auto fill = th.surfaceAlt;
            if (down) fill = fill.darker (0.12f);
            else if (highlighted) fill = fill.brighter (0.08f);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRoundedRectangle (bounds.translated (0, down ? 0.5f : 1.5f), rad);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, rad);
        }
        else if (highlighted || down)
        {
            g.setColour (th.text.withAlpha (down ? 0.20f : 0.10f));
            g.fillRoundedRectangle (bounds, rad);
        }

        g.setColour (th.text.withAlpha (flat && ! highlighted && ! down ? 0.85f : 1.0f));
        auto iconArea = bounds.reduced (bounds.getWidth() * (flat ? 0.22f : 0.28f),
                                       bounds.getHeight() * (flat ? 0.22f : 0.28f));

        if (icon == Icon::None && label.isNotEmpty())
        {
            g.setFont (juce::FontOptions (juce::jlimit (11.0f, 20.0f, getHeight() * 0.36f), juce::Font::bold));
            g.drawText (label, getLocalBounds(), juce::Justification::centred, false);
            return;
        }

        juce::Path p;
        const float x = iconArea.getX(), y = iconArea.getY();
        const float w = iconArea.getWidth(), h = iconArea.getHeight();

        switch (icon)
        {
            case Icon::Save:
            {
                // Floppy outline
                p.addRoundedRectangle (x, y, w, h, w * 0.08f);
                g.strokePath (p, juce::PathStrokeType (2.2f));
                p.clear();
                // Label window top
                p.addRectangle (x + w * 0.22f, y, w * 0.56f, h * 0.28f);
                g.fillPath (p);
                p.clear();
                // Bottom slot
                p.addRoundedRectangle (x + w * 0.22f, y + h * 0.55f, w * 0.56f, h * 0.32f, 1.5f);
                g.strokePath (p, juce::PathStrokeType (1.8f));
                break;
            }
            case Icon::Pencil:
            {
                // Diagonal pencil body
                juce::Path body;
                body.startNewSubPath (x + w * 0.15f, y + h * 0.70f);
                body.lineTo (x + w * 0.55f, y + h * 0.15f);
                body.lineTo (x + w * 0.72f, y + h * 0.28f);
                body.lineTo (x + w * 0.32f, y + h * 0.83f);
                body.closeSubPath();
                g.strokePath (body, juce::PathStrokeType (2.0f));
                // Tip
                juce::Path tip;
                tip.startNewSubPath (x + w * 0.15f, y + h * 0.70f);
                tip.lineTo (x + w * 0.08f, y + h * 0.92f);
                tip.lineTo (x + w * 0.32f, y + h * 0.83f);
                tip.closeSubPath();
                g.fillPath (tip);
                // Eraser band
                g.drawLine (x + w * 0.55f, y + h * 0.15f, x + w * 0.72f, y + h * 0.28f, 2.2f);
                break;
            }
            case Icon::Gear:
            {
                const float cx = x + w * 0.5f, cy = y + h * 0.5f;
                const float outer = juce::jmin (w, h) * 0.48f;
                const float inner = outer * 0.58f;
                const float hole  = outer * 0.28f;
                juce::Path gear;
                const int teeth = 8;
                for (int i = 0; i < teeth; ++i)
                {
                    const float a0 = (float) i / (float) teeth * juce::MathConstants<float>::twoPi
                                     - juce::MathConstants<float>::halfPi;
                    const float a1 = a0 + juce::MathConstants<float>::twoPi / (float) teeth * 0.35f;
                    const float a2 = a0 + juce::MathConstants<float>::twoPi / (float) teeth * 0.50f;
                    const float a3 = a0 + juce::MathConstants<float>::twoPi / (float) teeth * 0.85f;
                    auto pt = [&] (float r, float a) {
                        return juce::Point<float> (cx + r * std::cos (a), cy + r * std::sin (a));
                    };
                    if (i == 0) gear.startNewSubPath (pt (outer, a0));
                    else        gear.lineTo (pt (outer, a0));
                    gear.lineTo (pt (outer, a1));
                    gear.lineTo (pt (inner, a2));
                    gear.lineTo (pt (inner, a3));
                }
                gear.closeSubPath();
                g.strokePath (gear, juce::PathStrokeType (2.0f));
                g.drawEllipse (cx - hole, cy - hole, hole * 2.0f, hole * 2.0f, 2.0f);
                break;
            }
            case Icon::Plus:
            {
                const float t = juce::jmax (2.0f, w * 0.14f);
                g.fillRect (x + w * 0.5f - t * 0.5f, y, t, h);
                g.fillRect (x, y + h * 0.5f - t * 0.5f, w, t);
                break;
            }
            case Icon::ChevronUp:
            {
                juce::Path ch;
                ch.startNewSubPath (x + w * 0.15f, y + h * 0.65f);
                ch.lineTo (x + w * 0.50f, y + h * 0.30f);
                ch.lineTo (x + w * 0.85f, y + h * 0.65f);
                g.strokePath (ch, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
                break;
            }
            case Icon::ChevronDown:
            {
                juce::Path ch;
                ch.startNewSubPath (x + w * 0.15f, y + h * 0.35f);
                ch.lineTo (x + w * 0.50f, y + h * 0.70f);
                ch.lineTo (x + w * 0.85f, y + h * 0.35f);
                g.strokePath (ch, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
                break;
            }
            default:
                break;
        }
    }

private:
    Icon icon;
    juce::String label;
    bool flat = false;
};
