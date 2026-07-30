#include "PluginBlockComponent.h"

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

void PluginBlockComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    auto body = bypassed ? blockColour.darker (0.55f) : blockColour;
    if (deleteHover) body = juce::Colour (0xffc0392b);
    else if (dragOver) body = body.brighter (0.25f);

    g.setColour (body);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (selected ? juce::Colours::white : juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 10.0f, selected ? 2.0f : 1.0f);

    g.setColour (juce::Colours::white);
    auto textArea = bounds.reduced (6.0f);
    const int bypassH = juce::jlimit (12, 22, getHeight() / 6);
    textArea.removeFromBottom ((float) bypassH + 4.0f);
    // Fit full name: scale font using GlyphArrangement (JUCE 9 has no Font::getStringWidth)
    float fontSize = juce::jlimit (16.0f, 42.0f, (float) getHeight() * 0.28f);
    for (;;)
    {
        juce::Font font (juce::FontOptions (fontSize, juce::Font::bold));
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, pluginName, 0.0f, 0.0f);
        const float textW = ga.getBoundingBox (0, -1, true).getWidth();
        if (fontSize <= 14.0f || textW <= textArea.getWidth() * 2.6f)
        {
            g.setFont (font);
            break;
        }
        fontSize -= 1.5f;
    }
    g.drawFittedText (pluginName, textArea.toNearestInt(), juce::Justification::centred, 3);

    bypassBounds = getLocalBounds().removeFromBottom (bypassH + 6)
                       .withSizeKeepingCentre (juce::jlimit (28, 48, getWidth() / 3), bypassH);
    g.setColour (bypassed ? juce::Colours::grey : juce::Colours::white.withAlpha (0.9f));
    g.fillRoundedRectangle (bypassBounds.toFloat(), 4.0f);
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
        if (onDragStarted) onDragStarted (pluginIndex);
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            c->startDragging ("PluginBlock:" + juce::String (pluginIndex), this);
    }
}

void PluginBlockComponent::mouseUp (const juce::MouseEvent& e)
{
    if (didStartDrag)
    {
        if (onDragEnded) onDragEnded();
        didStartDrag = false;
        return;
    }

    if (draggingBypass && bypassBounds.contains (e.getPosition()))
    {
        if (onBypassToggled) onBypassToggled (pluginIndex);
        return;
    }

    if (e.getDistanceFromDragStart() < 10)
    {
        auto now = juce::Time::getCurrentTime();
        if ((now - lastTapTime).inMilliseconds() < 350) ++tapCount;
        else tapCount = 1;
        lastTapTime = now;

        if (tapCount >= 2)
        {
            tapCount = 0;
            if (onDoubleTap) onDoubleTap (pluginIndex);
        }
        else if (onSelected)
            onSelected (pluginIndex);
    }
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
    const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (from != pluginIndex && onReorder) onReorder (from, pluginIndex);
}
