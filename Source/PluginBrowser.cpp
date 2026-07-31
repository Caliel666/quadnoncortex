#include "PluginBrowser.h"
#include "Theme.h"

//==============================================================================
PluginBrowser::PluginRow::PluginRow (const juce::PluginDescription& d, PluginBrowser& owner)
    : desc (d), browser (owner)
{
    setRepaintsOnMouseActivity (true);
}

void PluginBrowser::PluginRow::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    const bool hovered = isMouseOverOrDragging();
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 14.0f);
    g.setColour (hovered ? th.card.brighter (0.06f) : th.card);
    g.fillRoundedRectangle (bounds, 14.0f);
    g.setColour (th.border.withAlpha (0.75f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.0f);

    auto icon = bounds.removeFromLeft (38.0f).reduced (10.0f);
    g.setColour (th.accent.withAlpha (0.90f));
    g.fillEllipse (icon);
    g.setColour (th.surface);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("V", icon.toNearestInt(), juce::Justification::centred);

    g.setColour (th.text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText (desc.name, bounds.reduced (12.0f, 0.0f), juce::Justification::centredLeft);

    g.setColour (th.textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (desc.manufacturerName,
                bounds.reduced (16.0f, 0.0f),
                juce::Justification::centredRight);
}

void PluginBrowser::PluginRow::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        browser.choosePlugin (desc);
}

//==============================================================================
PluginBrowser::PluginBrowser (PluginChain& chain)
    : pluginChain (chain)
{
    titleLabel.setText ("Add plug-in", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Theme::get().text);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    Theme::get().applyButton (closeButton);
    closeButton.onClick = [this]
    {
        setVisible (false);
        if (onClosed) onClosed();
    };
    addAndMakeVisible (closeButton);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setVisible (false);
}

void PluginBrowser::show (int replaceIdx)
{
    replaceIndex = replaceIdx;
    titleLabel.setText (replaceIdx >= 0 ? "Replace plug-in" : "Add plug-in",
                        juce::dontSendNotification);
    rebuildList();
    setVisible (true);
    toFront (true);
}

void PluginBrowser::rebuildList()
{
    rows.clear();

    auto types = pluginChain.getKnownPluginList().getTypes();
    for (const auto& desc : types)
    {
        if (desc.pluginFormatName != "VST3")
            continue;

        auto* row = rows.add (new PluginRow (desc, *this));
        content.addAndMakeVisible (row);
    }

    resized();
}

void PluginBrowser::choosePlugin (const juce::PluginDescription& desc)
{
    juce::String error;
    int resultIndex = -1;

    if (replaceIndex >= 0)
    {
        if (pluginChain.replacePlugin (replaceIndex, desc, error))
            resultIndex = replaceIndex;
    }
    else
    {
        resultIndex = pluginChain.addPlugin (desc, error);
    }

    if (resultIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Load Failed",
                                                error.isNotEmpty() ? error : "Could not load plugin");
        return;
    }

    setVisible (false);
    if (onPluginChosen)
        onPluginChosen (resultIndex);
    if (onClosed)
        onClosed();
}

void PluginBrowser::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.overlay);
    auto card = getLocalBounds().toFloat().reduced (18.0f);
    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.fillRoundedRectangle (card.translated (0.0f, 5.0f), 24.0f);
    g.setColour (th.surface);
    g.fillRoundedRectangle (card, 24.0f);
    g.setColour (th.border.withAlpha (0.9f));
    g.drawRoundedRectangle (card.reduced (0.5f), 24.0f, 1.0f);
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (32, 28);

    auto top = r.removeFromTop (48);
    closeButton.setBounds (top.removeFromRight (100));
    titleLabel.setBounds (top);

    r.removeFromTop (12);
    viewport.setBounds (r);

    const int h = rows.size() * kRowHeight;
    content.setSize (viewport.getWidth() - 8, juce::jmax (h, viewport.getHeight()));

    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds (0, y, content.getWidth(), kRowHeight);
        y += kRowHeight;
    }
}
