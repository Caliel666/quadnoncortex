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
    auto bounds = getLocalBounds().toFloat().reduced (6.0f, 3.0f);
    const bool hovered = isMouseOverOrDragging();

    if (hovered)
    {
        g.setColour (th.surfaceAlt.brighter (0.06f));
        g.fillRoundedRectangle (bounds, 10.0f);
    }

    g.setColour (th.text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    auto textArea = bounds.reduced (14.0f, 0.0f);
    g.drawText (desc.name, textArea.toNearestInt(), juce::Justification::centredLeft, true);

    g.setColour (th.textDim);
    g.setFont (juce::FontOptions (13.0f));
    const bool isNative = desc.pluginFormatName == "Native"
                       || desc.pluginFormatName == "Internal"
                       || desc.fileOrIdentifier.startsWithIgnoreCase ("internal://");
    const juce::String right = isNative ? "Native" : desc.manufacturerName;
    g.drawText (right, textArea.toNearestInt(),
                juce::Justification::centredRight, true);
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
    auto& th = Theme::get();

    titleLabel.setText ("Add plug-in", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, th.text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    th.applyButton (closeButton);
    closeButton.onClick = [this]
    {
        kbOverlay.reset();
        setVisible (false);
        if (onClosed) onClosed();
    };
    addAndMakeVisible (closeButton);

    searchBox.setTextToShowWhenEmpty ("Search plugins...", th.textDim);
    searchBox.setFont (juce::FontOptions (18.0f));
    searchBox.setColour (juce::TextEditor::backgroundColourId, th.surfaceAlt);
    searchBox.setColour (juce::TextEditor::textColourId, th.text);
    searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::highlightColourId, th.accent.withAlpha (0.35f));
    searchBox.setReadOnly (true); // input via our overlay keyboard only
    searchBox.onTextChange = [this]
    {
        filterText = searchBox.getText().trim();
        applyFilter();
    };
    addAndMakeVisible (searchBox);
    searchBox.addMouseListener (this, false);

    filterBox.addItem ("All", 1);
    filterBox.addItem ("Native", 2);
    filterBox.addItem ("Third-party", 3);
    filterBox.setSelectedId (1);
    filterBox.onChange = [this] { applyFilter(); };
    addAndMakeVisible (filterBox);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setVisible (false);
}

void PluginBrowser::openSearchKeyboard()
{
    if (kbOverlay != nullptr)
        return;

    auto* ov = new KeyboardOverlay();
    ov->setInitial (searchBox.getText());
    ov->onDone = [this] (juce::String text) { closeSearchKeyboard (text); };
    ov->setBounds (getLocalBounds());
    addAndMakeVisible (ov);
    ov->toFront (true);
    kbOverlay.reset (ov);
    juce::MessageManager::callAsync ([f = &ov->field] { f->grabKeyboardFocus(); });
}

void PluginBrowser::closeSearchKeyboard (const juce::String& text)
{
    searchBox.setText (text, juce::sendNotification);
    filterText = text.trim();
    applyFilter();
    kbOverlay.reset();
}

void PluginBrowser::show (int replaceIdx)
{
    replaceIndex = replaceIdx;
    titleLabel.setText (replaceIdx >= 0 ? "Replace plug-in" : "Add plug-in",
                        juce::dontSendNotification);
    filterText.clear();
    searchBox.setText ({}, juce::dontSendNotification);
    kbOverlay.reset();
    rebuildList();
    setVisible (true);
    toFront (true);
}

void PluginBrowser::rebuildList()
{
    applyFilter();
}

void PluginBrowser::applyFilter()
{
    rows.clear();
    auto types = pluginChain.getKnownPluginList().getTypes();
    const auto q = filterText.toLowerCase();
    const int mode = filterBox.getSelectedId(); // 1=All 2=Native 3=Third-party

    for (const auto& desc : types)
    {
        const bool isNative = desc.pluginFormatName == "Native"
                           || desc.pluginFormatName == "Internal"
                           || desc.fileOrIdentifier.startsWith ("internal://");
        const bool isVst = desc.pluginFormatName == "VST3";

        if (! isNative && ! isVst)
            continue;
        if (mode == 2 && ! isNative)
            continue;
        if (mode == 3 && ! isVst)
            continue;

        if (q.isNotEmpty())
        {
            if (! desc.name.toLowerCase().contains (q)
                && ! desc.manufacturerName.toLowerCase().contains (q))
                continue;
        }
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

    kbOverlay.reset();
    setVisible (false);
    if (onPluginChosen)
        onPluginChosen (resultIndex);
    if (onClosed)
        onClosed();
}

void PluginBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &searchBox
        || searchBox.getBounds().contains (e.getEventRelativeTo (this).getPosition()))
        openSearchKeyboard();
}

void PluginBrowser::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.background);
    g.setColour (th.text.withAlpha (0.08f));
    g.fillRect (0, 52, getWidth(), 1);
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (20, 16);

    auto top = r.removeFromTop (44);
    closeButton.setBounds (top.removeFromRight (100).reduced (2));
    titleLabel.setBounds (top);

    r.removeFromTop (10);
    auto searchRow = r.removeFromTop (48);
    filterBox.setBounds (searchRow.removeFromRight (140).reduced (4, 4));
    searchBox.setBounds (searchRow.reduced (0, 4));
    r.removeFromTop (8);

    viewport.setBounds (r);

    const int h = rows.size() * kRowHeight;
    content.setSize (viewport.getWidth() - 6, juce::jmax (h, viewport.getHeight()));

    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds (0, y, content.getWidth(), kRowHeight);
        y += kRowHeight;
    }

    if (kbOverlay != nullptr)
        kbOverlay->setBounds (getLocalBounds());
}
