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
    const float radius = 18.0f;

    auto body = bypassed ? blockColour.withMultipliedSaturation (0.35f)
                                       .withMultipliedBrightness (0.55f)
                         : blockColour;

    if (deleteHover)
        body = th.danger;
    else if (dragOver)
        body = body.brighter (0.12f);

    if (! dragging)
    {
        g.setColour (juce::Colours::black.withAlpha (th.currentName == "Light" ? 0.08f : 0.28f));
        g.fillRoundedRectangle (bounds.translated (0, 2.5f), radius);
    }

    g.setColour (body);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (juce::Colours::white.withAlpha (th.currentName == "Light" ? 0.18f : 0.06f));
    {
        auto sheen = bounds;
        g.fillRoundedRectangle (sheen.removeFromTop (sheen.getHeight() * 0.35f), radius);
    }

    bounds = getLocalBounds().toFloat().reduced (5.0f);

    if (selected)
    {
        g.setColour (th.accent.withAlpha (0.95f));
        g.drawRoundedRectangle (bounds, radius, 2.0f);
    }

    g.setColour (bypassed ? th.textDim : juce::Colours::white);
    if (th.currentName == "Light" && ! bypassed)
        g.setColour (juce::Colour (0xff1a1d24));

    auto textArea = bounds.reduced (10.0f);
    const int bypassH = juce::jlimit (14, 24, getHeight() / 6);
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
    g.drawFittedText (pluginName, textArea.toNearestInt(), juce::Justification::centred, 3);

    bypassBounds = getLocalBounds().removeFromBottom (bypassH + 10)
                       .withSizeKeepingCentre (juce::jlimit (32, 56, getWidth() / 3), bypassH);
    g.setColour (bypassed ? juce::Colours::black.withAlpha (0.25f)
                          : juce::Colours::white.withAlpha (0.9f));
    g.fillRoundedRectangle (bypassBounds.toFloat(), (float) bypassH * 0.5f);
    if (bypassed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions ((float) juce::jmax (9, bypassH - 4), juce::Font::bold));
        g.drawText ("BYP", bypassBounds, juce::Justification::centred);
    }
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

        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            // Snapshot at 1x so the drag ghost matches on-screen block size
            const float oldAlpha = getAlpha();
            setAlpha (1.0f);
            auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            setAlpha (oldAlpha);
            c->startDragging ("PluginBlock:" + juce::String (pluginIndex), this,
                              juce::ScaledImage (img, 1.0), true);
        }
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
        if (bypassBounds.contains (e.getPosition()) && onBypassToggled)
            onBypassToggled (pluginIndex);
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

bool PluginBlockComponent::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("PluginBlock:");
}

void PluginBlockComponent::itemDragEnter (const SourceDetails&) { dragOver = true;  repaint(); }
void PluginBlockComponent::itemDragExit  (const SourceDetails&) { dragOver = false; repaint(); }

void PluginBlockComponent::itemDropped (const SourceDetails& d)
{
    dragOver = false; repaint();
    const auto s = d.description.toString();
    if (! s.startsWith ("PluginBlock:")) return;
    const int from = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (from != pluginIndex && onReorder)
        onReorder (from, pluginIndex);
}
