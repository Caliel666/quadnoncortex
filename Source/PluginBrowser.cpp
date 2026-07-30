#include "PluginBrowser.h"

//==============================================================================
PluginBrowser::PluginRow::PluginRow (const juce::PluginDescription& d, PluginBrowser& owner)
    : desc (d), browser (owner)
{
}

void PluginBrowser::PluginRow::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f));
    g.drawText (desc.name, bounds.reduced (16.0f, 0.0f), juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
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
    titleLabel.setText ("Add VST3 Plugin", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff555555));
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
    titleLabel.setText (replaceIdx >= 0 ? "Replace Plugin" : "Add VST3 Plugin",
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
    g.fillAll (juce::Colour (0xee0a0a0a));
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (16);

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
