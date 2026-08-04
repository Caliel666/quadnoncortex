#include "PluginBlockComponent.h"
#include "Theme.h"

PluginBlockComponent::PluginBlockComponent (int index, const juce::String& name)
    : pluginIndex (index), pluginName (name)
{
    setRepaintsOnMouseActivity (true);
}

void PluginBlockComponent::setPluginName (const juce::String& n) { pluginName = n; repaint(); }
void PluginBlockComponent::setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }
void PluginBlockComponent::setBypassed (bool b) { if (bypassed != b) { bypassed = b; repaint(); } }
void PluginBlockComponent::setBlockColour (juce::Colour c) { blockColour = c; repaint(); }
void PluginBlockComponent::setDeleteHover (bool h) { if (deleteHover != h) { deleteHover = h; repaint(); } }
void PluginBlockComponent::setDragging (bool d)
{
    if (dragging != d)
    {
        dragging = d;
        setAlpha (d ? 0.35f : 1.0f);
        repaint();
    }
}

void PluginBlockComponent::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    constexpr float radius = 15.0f;
    const bool hovered = isMouseOverOrDragging();
    const auto accent = deleteHover ? th.danger : blockColour;
    auto body = bypassed ? th.surfaceAlt.darker (0.06f) : th.surface;
    if (dragOver || hovered)
        body = body.brighter (0.045f);

    if (! dragging)
    {
        g.setColour (juce::Colours::black.withAlpha (th.currentName == "Light" ? 0.10f : 0.30f));
        g.fillRoundedRectangle (bounds.translated (0.0f, hovered ? 1.5f : 3.0f), radius);
    }

    g.setColour (body);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (accent.withAlpha (bypassed ? 0.35f : 0.95f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius - 1.0f, 2.0f);

    if (selected)
    {
        g.setColour (th.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds.expanded (2.0f), radius + 2.0f);
        g.setColour (th.accent);
        g.drawRoundedRectangle (bounds.expanded (1.0f), radius + 1.0f, 1.5f);
    }

    auto textArea = bounds.reduced (12.0f);
    const int bypassH = juce::jlimit (18, 26, getHeight() / 6);
    textArea.removeFromBottom ((float) bypassH + 8.0f);

    float fontSize = juce::jlimit (15.0f, 40.0f, (float) getHeight() * 0.26f);
    for (;;)
    {
        juce::Font font (juce::FontOptions (fontSize, juce::Font::bold));
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, pluginName, 0.0f, 0.0f);
        const float textW = ga.getBoundingBox (0, -1, true).getWidth();
        if (fontSize <= 13.0f || textW <= textArea.getWidth() * 2.5f)
        {
            g.setFont (font);
            break;
        }
        fontSize -= 1.5f;
    }
    g.setColour (bypassed ? th.textDim : th.text);
    g.drawFittedText (pluginName, textArea.toNearestInt(), juce::Justification::centred, 3);

    bypassBounds = getLocalBounds().removeFromBottom (bypassH + 10)
                       .withSizeKeepingCentre (juce::jlimit (44, 64, getWidth() / 3), bypassH);
    g.setColour (bypassed ? th.danger.withAlpha (0.82f)
                          : th.surfaceAlt);
    g.fillRoundedRectangle (bypassBounds.toFloat(), (float) bypassH * 0.5f);
    g.setColour (bypassed ? juce::Colours::white : th.textDim);
    g.setFont (juce::FontOptions ((float) juce::jmax (9, bypassH - 6), juce::Font::bold));
    g.drawText (bypassed ? "BYP" : "ON", bypassBounds, juce::Justification::centred);
}

void PluginBlockComponent::resized() {}

void PluginBlockComponent::mouseDown (const juce::MouseEvent& e)
{
    dragStart = e.getPosition();
    draggingBypass = bypassBounds.contains (e.getPosition());
    didStartDrag = false;
}

void PluginBlockComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBypass) return;
    if (! didStartDrag && e.getDistanceFromDragStart() > 12)
    {
        didStartDrag = true;
        setDragging (true);
        if (onDragStarted) onDragStarted (pluginIndex);
    }
}

void PluginBlockComponent::mouseUp (const juce::MouseEvent& e)
{
    if (didStartDrag)
    {
        setDragging (false);
        if (onDragEnded) onDragEnded();
        didStartDrag = false;
        return;
    }

    if (draggingBypass)
    {
        // Single press on block bypass → MIDI popup (toggle bypass via parameter header)
        if (bypassBounds.contains (e.getPosition()) && onBypassMidiRequested)
            onBypassMidiRequested (pluginIndex);
        draggingBypass = false;
        return;
    }

    if (e.getNumberOfClicks() >= 2)
    {
        if (onDoubleTap) onDoubleTap (pluginIndex);
        return;
    }

    if (onSelected) onSelected (pluginIndex);
}

void PluginBlockComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleTap) onDoubleTap (pluginIndex);
}
