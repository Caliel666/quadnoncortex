#include "MainComponent.h"

static const juce::uint32 kPalette[] = {
    0xffe67e22, 0xffe74c3c, 0xff9b59b6, 0xff3498db,
    0xff1abc9c, 0xff2ecc71, 0xfff1c40f, 0xffe91e63,
    0xff00bcd4, 0xffff5722, 0xff607d8b, 0xff795548
};

MainComponent::MainComponent()
{
    getLookAndFeel().setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0d0d0d));
    getLookAndFeel().setColour (juce::TextButton::buttonColourId, juce::Colour (0xff37474f));
    getLookAndFeel().setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    titleLabel.setText ("quadnoncortex", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    prevPresetBtn.onClick = [this] { presetPrev(); };
    nextPresetBtn.onClick = [this] { presetNext(); };
    newPresetBtn.onClick = [this] { newPreset(); };
    savePresetBtn.onClick = [this] { savePreset(); };
    renamePresetBtn.onClick = [this] { renamePreset(); };
    addAndMakeVisible (prevPresetBtn);
    addAndMakeVisible (nextPresetBtn);
    addAndMakeVisible (newPresetBtn);
    addAndMakeVisible (savePresetBtn);
    addAndMakeVisible (renamePresetBtn);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (i, presetFiles.size()))
        {
            currentPresetIndex = i;
            loadPreset (presetFiles[i]);
        }
    };
    addAndMakeVisible (presetBox);

    addButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
    addButton.onClick = [this] { if (pluginBrowser) pluginBrowser->show (-1); };
    addAndMakeVisible (addButton);
    settingsBtn.onClick = [this] { openSettings(); };
    addAndMakeVisible (settingsBtn);

    trashZone.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
    trashZone.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    trashZone.setVisible (false);
    addChildComponent (trashZone);

    auto setupFader = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 2.0, 0.01);
        s.setValue (1.0);
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xff4fc3f7));
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff455a64));
    };
    setupFader (inputFader);
    setupFader (outputFader);
    inputFader.onValueChange  = [this] { audioEngine.setInputGain  ((float) inputFader.getValue()); };
    outputFader.onValueChange = [this] { audioEngine.setOutputGain ((float) outputFader.getValue()); };
    inputFader.setValue  (AppSettings::get().inputGain,  juce::dontSendNotification);
    outputFader.setValue (AppSettings::get().outputGain, juce::dontSendNotification);
    addAndMakeVisible (inputFader);
    addAndMakeVisible (outputFader);
    inLabel.setJustificationType (juce::Justification::centred);
    outLabel.setJustificationType (juce::Justification::centred);
    inLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    outLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    inLabel.setFont (juce::FontOptions (11.0f));
    outLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (inLabel);
    addAndMakeVisible (outLabel);

    tabPedal.setClickingTogglesState (true);
    tabTuner.setClickingTogglesState (true);
    tabPedal.setRadioGroupId (1);
    tabTuner.setRadioGroupId (1);
    tabPedal.setToggleState (true, juce::dontSendNotification);
    tabPedal.onClick = [this] { setTab (0); };
    tabTuner.onClick = [this] { setTab (1); };
    addAndMakeVisible (tabPedal);
    addAndMakeVisible (tabTuner);

    blocksViewport.setViewedComponent (&blocksContent, false);
    blocksContent.addMouseListener (this, true); // clicks on empty board deselect
    blocksViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (blocksViewport);

    parameterPanel = std::make_unique<ParameterPanel> (audioEngine.getMidiLearnManager());
    addAndMakeVisible (parameterPanel.get());

    tuner = std::make_unique<TunerComponent>();
    addChildComponent (tuner.get());

    pluginBrowser = std::make_unique<PluginBrowser> (audioEngine.getPluginChain());
    pluginBrowser->onPluginChosen = [this] (int)
    {
        rebuildBlocks();
        const int n = audioEngine.getPluginChain().getNumPlugins();
        if (n > 0) selectPlugin (n - 1);
    };
    addChildComponent (pluginBrowser.get());

    scanOverlay = std::make_unique<PluginScanOverlay> (audioEngine.getPluginChain());
    addChildComponent (scanOverlay.get());

    audioEngine.getMidiLearnManager().onParamChangedByMidi =
        [this] (int pluginIdx, int paramIdx, float value)
        {
            if (paramIdx == -2)
            {
                rebuildBlocks();
                if (parameterPanel && selectedIndex == pluginIdx)
                    parameterPanel->updateBypass (audioEngine.getPluginChain().isBypassed (pluginIdx));
                return;
            }
            if (parameterPanel && selectedIndex == pluginIdx)
                parameterPanel->updateParamValue (paramIdx, value);
        };

    audioEngine.getMidiLearnManager().onGlobalAction =
        [this] (const juce::String& action, float value) { handleGlobalMidi (action, value); };

    audioEngine.onAudioForTuner = [this] (const float* data, int n)
    {
        if (tuner != nullptr) tuner->pushSamples (data, n);
    };

    audioEngine.initialise();
    audioEngine.getPluginChain().loadKnownPluginsFromDisk();

    refreshPresetList();
    if (AppSettings::get().lastPreset.isNotEmpty())
    {
        auto f = AppSettings::get().getPresetsDir().getChildFile (AppSettings::get().lastPreset);
        if (f.existsAsFile())
        {
            for (int i = 0; i < presetFiles.size(); ++i)
                if (presetFiles[i] == f)
                {
                    currentPresetIndex = i;
                    presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
                    break;
                }
            loadPreset (f);
        }
    }

    startTimerHz (30);
    setSize (1024, 600);
    juce::MessageManager::callAsync ([this] { startBootScan(); });
}

MainComponent::~MainComponent()
{
    stopTimer();
    editorWindow = nullptr;
    scanOverlay = nullptr;
    audioEngine.shutdown();
}

void MainComponent::openSettings()
{
    auto* panel = new SettingsComponent (audioEngine, audioEngine.getMidiLearnManager());
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (panel);
    opts.dialogTitle = "Audio / MIDI";
    opts.dialogBackgroundColour = juce::Colour (0xff1a1a1a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = false;
    opts.resizable = true;
    opts.launchAsync();
}

void MainComponent::startBootScan()
{
    if (scanOverlay)
        scanOverlay->startScan ([this] { if (pluginBrowser) pluginBrowser->rebuildList(); });
}

void MainComponent::setTab (int tab)
{
    currentTab = tab;
    const bool board = (tab == 0);
    blocksViewport.setVisible (board);
    if (parameterPanel) parameterPanel->setVisible (board && selectedIndex >= 0);
    if (tuner) tuner->setVisible (! board);
    audioEngine.setMuted (! board);
    tabPedal.setToggleState (board, juce::dontSendNotification);
    tabTuner.setToggleState (! board, juce::dontSendNotification);
    resized();
}

void MainComponent::handleGlobalMidi (const juce::String& action, float)
{
    if (action == "tuner") setTab (currentTab == 0 ? 1 : 0);
    else if (action == "presetNext") presetNext();
    else if (action == "presetPrev") presetPrev();
}

void MainComponent::cycleBlockColour (int index)
{
    auto c = audioEngine.getPluginChain().getBlockColour (index);
    int found = 0;
    for (int i = 0; i < 12; ++i)
        if (c.getARGB() == kPalette[i]) { found = (i + 1) % 12; break; }
    audioEngine.getPluginChain().setBlockColour (index, juce::Colour (kPalette[found]));
    rebuildBlocks();
}

void MainComponent::showPluginEditor (int index)
{
    auto* inst = audioEngine.getPluginChain().getPluginInstance (index);
    if (inst == nullptr || ! inst->hasEditor()) return;
    editorWindow = nullptr;
    auto* ed = inst->createEditorIfNeeded();
    if (ed == nullptr) return;

    class EditorWin : public juce::DocumentWindow
    {
    public:
        EditorWin (const juce::String& name, juce::AudioProcessorEditor* content)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
            centreWithSize (juce::jmax (400, content->getWidth()),
                            juce::jmax (300, content->getHeight() + 30));
            setVisible (true);
            setAlwaysOnTop (true);
            toFront (true);
        }
        void closeButtonPressed() override { setVisible (false); }
    };
    editorWindow = std::make_unique<EditorWin> (inst->getName(), ed);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));
    auto r = getLocalBounds();
    r.removeFromTop (kTopBar);
    auto left  = r.removeFromLeft (kSideW);
    auto right = r.removeFromRight (kSideW);
    left.removeFromBottom (kTabH + 18);
    right.removeFromBottom (kTabH + 18);
    paintMeter (g, left.removeFromRight (10).reduced (1, 4), smoothInPeak);
    paintMeter (g, right.removeFromLeft (10).reduced (1, 4), smoothOutPeak);
}

void MainComponent::paintMeter (juce::Graphics& g, juce::Rectangle<int> area, float peak)
{
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (area.toFloat(), 3.0f);
    const float db = juce::Decibels::gainToDecibels (peak + 1.0e-6f, -60.0f);
    const float norm = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
    auto fill = area.removeFromBottom ((int) (area.getHeight() * juce::jlimit (0.0f, 1.0f, norm)));
    juce::Colour c = peak >= 0.99f ? juce::Colours::red
                     : peak >= 0.7f  ? juce::Colours::orange
                                     : juce::Colour (0xff2ecc71);
    g.setColour (c);
    g.fillRoundedRectangle (fill.toFloat(), 2.0f);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    // ---- Top bar ----
    auto top = r.removeFromTop (kTopBar);
    titleLabel.setBounds (top.removeFromLeft (120).reduced (6, 10));
    settingsBtn.setBounds (top.removeFromRight (48).reduced (3));
    addButton.setBounds (top.removeFromRight (40).reduced (3));

    const int presetBoxW = juce::jlimit (140, 220, top.getWidth() / 3);
    const int clusterW = 36 + 4 + presetBoxW + 4 + 36 + 4 + 48 + 4 + 56 + 4 + 44;
    auto centre = top.withSizeKeepingCentre (juce::jmin (clusterW, top.getWidth()), top.getHeight());
    prevPresetBtn.setBounds (centre.removeFromLeft (36).reduced (2));
    presetBox.setBounds (centre.removeFromLeft (presetBoxW).reduced (2));
    nextPresetBtn.setBounds (centre.removeFromLeft (36).reduced (2));
    newPresetBtn.setBounds (centre.removeFromLeft (48).reduced (2));
    savePresetBtn.setBounds (centre.removeFromLeft (56).reduced (2));
    renamePresetBtn.setBounds (centre.removeFromLeft (44).reduced (2));

    // Trash sits below the top bar so it doesn't cover presets
    if (showTrash)
        trashZone.setBounds (getWidth() / 2 - 32, kTopBar + 8, 64, 48);

    auto tabs = r.removeFromBottom (kTabH);
    tabPedal.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (6, 5));
    tabTuner.setBounds (tabs.reduced (6, 5));

    if (currentTab == 0)
    {
        // Side faders only on board view
        auto left  = r.removeFromLeft (kSideW);
        auto right = r.removeFromRight (kSideW);
        inLabel.setVisible (true);
        outLabel.setVisible (true);
        inputFader.setVisible (true);
        outputFader.setVisible (true);
        inLabel.setBounds (left.removeFromBottom (20));
        inputFader.setBounds (left.reduced (12, 4));
        outLabel.setBounds (right.removeFromBottom (20));
        outputFader.setBounds (right.reduced (12, 4));

        const bool showParams = selectedIndex >= 0;
        // When a plugin is selected, params take lower portion; board keeps the rest
        const int paramH = showParams ? juce::jmax (180, (r.getHeight() * 40) / 100) : 0;
        if (parameterPanel)
        {
            parameterPanel->setBounds (r.removeFromBottom (paramH));
            parameterPanel->setVisible (showParams);
        }

        blocksViewport.setBounds (r.reduced (4));
        // No scrollbars needed when we fit everything
        blocksViewport.setScrollBarsShown (false, false);

        const int n = blocks.size();
        const int maxPerRow = 5;
        const int availW = juce::jmax (80, blocksViewport.getWidth());
        const int availH = juce::jmax (80, blocksViewport.getHeight());
        const int gap = 12;

        auto& chain = audioEngine.getPluginChain();

        // Relative weights from name length (min weight so short names stay touchable)
        juce::Array<float> weights;
        for (int i = 0; i < n; ++i)
        {
            const int len = juce::jmax (4, chain.getPluginName (i).length());
            weights.add ((float) len);
        }

        // Pack into rows of at most 5
        struct Row { int start = 0, count = 0; float weightSum = 0.0f; };
        juce::Array<Row> rows;
        {
            int i = 0;
            while (i < n)
            {
                Row row;
                row.start = i;
                while (i < n && row.count < maxPerRow)
                {
                    row.weightSum += weights[i];
                    row.count++;
                    i++;
                }
                rows.add (row);
            }
        }
        const int numRows = juce::jmax (1, rows.size());

        // Target: use most of the board. Single row → large tiles; more rows → share height.
        const float fillW = 0.88f; // fraction of width to use
        const float fillH = 0.82f;

        int blockH = (int) ((availH * fillH - gap * (numRows + 1)) / (float) numRows);
        blockH = juce::jlimit (100, 180, blockH);

        blocksContent.setSize (availW, availH);
        const int gridH = numRows * blockH + (numRows + 1) * gap;
        const int originY = juce::jmax (gap, (availH - gridH) / 2);

        for (int r = 0; r < rows.size(); ++r)
        {
            const auto& row = rows.getReference (r);
            const int rowBudget = (int) (availW * fillW) - gap * (row.count - 1);
            // Minimum width so short names stay large enough to tap
            const int minW = juce::jlimit (100, 160, blockH - 10);

            juce::Array<int> widths;
            int used = 0;
            for (int c = 0; c < row.count; ++c)
            {
                const float frac = weights[row.start + c] / juce::jmax (0.001f, row.weightSum);
                int w = juce::jmax (minW, (int) (rowBudget * frac));
                widths.add (w);
                used += w;
            }
            // If we overflowed minWs, scale down; if under budget, distribute remainder
            if (used > rowBudget)
            {
                const float s = (float) rowBudget / (float) used;
                used = 0;
                for (int c = 0; c < widths.size(); ++c)
                {
                    widths.set (c, juce::jmax (80, (int) (widths[c] * s)));
                    used += widths[c];
                }
            }
            else if (used < rowBudget && row.count > 0)
            {
                const int extra = (rowBudget - used) / row.count;
                for (int c = 0; c < widths.size(); ++c)
                    widths.set (c, widths[c] + extra);
                used = 0;
                for (auto w : widths) used += w;
            }

            int x = juce::jmax (gap, (availW - (used + gap * (row.count - 1))) / 2);
            const int y = originY + r * (blockH + gap);
            for (int c = 0; c < row.count; ++c)
            {
                blocks[row.start + c]->setBounds (x, y, widths[c], blockH);
                x += widths[c] + gap;
            }
        }
    }
    else
    {
        // Tuner: full width + height under top bar / above tabs
        inLabel.setVisible (false);
        outLabel.setVisible (false);
        inputFader.setVisible (false);
        outputFader.setVisible (false);
        if (tuner)
            tuner->setBounds (r);
        if (parameterPanel)
            parameterPanel->setVisible (false);
    }

    if (pluginBrowser) pluginBrowser->setBounds (getLocalBounds());
    if (scanOverlay)   scanOverlay->setBounds (getLocalBounds());
    if (nameOverlay)   nameOverlay->setBounds (getLocalBounds());
}

void MainComponent::rebuildBlocks()
{
    blocks.clear();
    auto& chain = audioEngine.getPluginChain();
    for (int i = 0; i < chain.getNumPlugins(); ++i)
    {
        auto* block = blocks.add (new PluginBlockComponent (i, chain.getPluginName (i)));
        block->setBypassed (chain.isBypassed (i));
        block->setBlockColour (chain.getBlockColour (i));
        block->onSelected = [this] (int idx) { selectPlugin (idx); };
        block->onDragStarted = [this] (int idx)
        {
            dragSourceIndex = idx;
            dragOverTrash = false;
            showTrash = true;
            trashZone.setVisible (true);
            trashZone.toFront (false);
            resized();
        };
        block->onDragEnded = [this]
        {
            // Final hit-test: if released over trash, delete
            if (dragOverTrash && juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
            {
                const int idx = dragSourceIndex;
                dragSourceIndex = -1;
                dragOverTrash = false;
                showTrash = false;
                trashZone.setVisible (false);
                removePlugin (idx);
                return;
            }
            if (juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
                blocks[dragSourceIndex]->setDeleteHover (false);
            dragSourceIndex = -1;
            dragOverTrash = false;
            showTrash = false;
            trashZone.setVisible (false);
            trashZone.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
            resized();
        };
        block->onReorder = [this] (int from, int to) { reorderPlugins (from, to); };
        block->onBypassToggled = [this] (int idx)
        {
            audioEngine.getPluginChain().toggleBypass (idx);
            rebuildBlocks();
            if (parameterPanel && selectedIndex == idx)
                parameterPanel->updateBypass (audioEngine.getPluginChain().isBypassed (idx));
        };
        block->onDoubleTap = [this] (int idx) { showPluginEditor (idx); };
        block->onColourRequested = [this] (int idx) { cycleBlockColour (idx); };
        block->setSelected (i == selectedIndex);
        blocksContent.addAndMakeVisible (block);
    }
    if (selectedIndex >= chain.getNumPlugins())
        selectedIndex = chain.getNumPlugins() - 1;
    resized();
}

void MainComponent::selectPlugin (int index)
{
    selectedIndex = index;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setSelected (i == selectedIndex);

    if (parameterPanel)
    {
        if (selectedIndex >= 0)
        {
            auto& chain = audioEngine.getPluginChain();
            parameterPanel->setPlugin (chain.getPluginInstance (selectedIndex), selectedIndex,
                                       chain.isBypassed (selectedIndex),
                                       [this]
                                       {
                                           audioEngine.getPluginChain().toggleBypass (selectedIndex);
                                           rebuildBlocks();
                                           parameterPanel->updateBypass (
                                               audioEngine.getPluginChain().isBypassed (selectedIndex));
                                       },
                                       [this] { showColourPicker (selectedIndex); });
        }
        else parameterPanel->clear();
    }
    resized();
}

void MainComponent::removePlugin (int index)
{
    static int lastRemoved = -999;
    static juce::uint32 lastTime = 0;
    const auto now = juce::Time::getMillisecondCounter();
    if (index == lastRemoved && (now - lastTime) < 400)
        return; // debounce double-delete from drag end + drop
    lastRemoved = index;
    lastTime = now;
    if (index < 0) return;

    // Close editor if it belongs to this plugin
    if (editorWindow != nullptr)
        editorWindow = nullptr;

    if (selectedIndex == index)
    {
        selectedIndex = -1;
        if (parameterPanel)
        {
            parameterPanel->clear();
            parameterPanel->setVisible (false);
        }
    }
    else if (selectedIndex > index)
        --selectedIndex;

    // Drop MIDI maps for this slot and shift the rest
    audioEngine.getMidiLearnManager().clearPlugin (index);

    // Safe async remove (won't crash audio thread)
    audioEngine.getPluginChain().requestRemovePlugin (index);

    // Rebuild after a short delay so the async remove has applied
    juce::Timer::callAfterDelay (50, [this]()
    {
        rebuildBlocks();
        if (selectedIndex >= 0)
            selectPlugin (selectedIndex);
        else
            resized();
    });
}

void MainComponent::reorderPlugins (int from, int to)
{
    audioEngine.getMidiLearnManager().remapAfterMove (from, to);
    audioEngine.getPluginChain().movePlugin (from, to);
    if (selectedIndex == from) selectedIndex = to;
    else if (from < selectedIndex && to >= selectedIndex) --selectedIndex;
    else if (from > selectedIndex && to <= selectedIndex) ++selectedIndex;
    rebuildBlocks();
}

void MainComponent::refreshPresetList()
{
    presetFiles.clear();
    presetBox.clear (juce::dontSendNotification);
    auto dir = AppSettings::get().getPresetsDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    // Alphabetical (so 01, 02, 03... sort naturally by name)
    files.sort();
    for (auto& f : files)
    {
        presetFiles.add (f);
        presetBox.addItem (f.getFileNameWithoutExtension(), presetFiles.size());
    }
}

void MainComponent::dismissNameOverlay (bool commit, const juce::String& text)
{
    auto cb = std::move (pendingNameCallback);
    pendingNameCallback = nullptr;
    nameOverlay.reset();

    if (commit && cb)
        cb (text);
}

void MainComponent::showPresetNameDialog (const juce::String& title,
                                          const juce::String& initial,
                                          std::function<void(const juce::String&)> onOk)
{
    nameOverlay.reset();
    pendingNameCallback = std::move (onOk);

    struct Shared
    {
        juce::String text;
    };
    auto shared = std::make_shared<Shared>();
    shared->text = initial;

    class Overlay : public juce::Component
    {
    public:
        MainComponent& owner;
        std::shared_ptr<Shared> shared;
        juce::Label titleLab;
        juce::TextEditor editor;
        juce::TextButton okBtn { "OK" }, cancelBtn { "Cancel" };
        OnScreenKeyboard keyboard;

        Overlay (MainComponent& o, std::shared_ptr<Shared> s, const juce::String& titleTxt)
            : owner (o), shared (std::move (s))
        {
            setInterceptsMouseClicks (true, true);

            titleLab.setText (titleTxt, juce::dontSendNotification);
            titleLab.setFont (juce::FontOptions (22.0f, juce::Font::bold));
            titleLab.setColour (juce::Label::textColourId, juce::Colours::white);
            titleLab.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (titleLab);

            const juce::Font f (juce::FontOptions (22.0f));
            editor.setFont (f);
            editor.setText (shared->text, false);
            editor.applyFontToAllText (f);
            editor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
            editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2c2c2c));
            editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff4fc3f7));
            editor.setColour (juce::CaretComponent::caretColourId, juce::Colours::white);
            editor.setMultiLine (false);
            editor.setSelectAllWhenFocused (true);
            editor.setWantsKeyboardFocus (true);
            editor.onTextChange = [this]
            {
                shared->text = editor.getText();
            };
            editor.onReturnKey = [this] { finish (true); };
            addAndMakeVisible (editor);

            okBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
            okBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            cancelBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff546e7a));
            cancelBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            okBtn.onClick = [this] { finish (true); };
            cancelBtn.onClick = [this] { finish (false); };
            addAndMakeVisible (okBtn);
            addAndMakeVisible (cancelBtn);

            addAndMakeVisible (keyboard);
            keyboard.onKey = [this] (juce::String s)
            {
                if (s == "\b")
                {
                    if (shared->text.isNotEmpty())
                        shared->text = shared->text.dropLastCharacters (1);
                }
                else if (s.isNotEmpty() && s != "\n")
                {
                    shared->text += s;
                }

                const juce::Font f (juce::FontOptions (22.0f));
                editor.setText (shared->text, false);
                editor.applyFontToAllText (f);
                editor.setCaretPosition (shared->text.length());
            };
            keyboard.onEnter = [this] { finish (true); };
            keyboard.onClose = [this] { finish (false); };
        }

        void finish (bool commit)
        {
            const juce::String text = shared->text;
            // Must call owner method (public) — not private fields from local class
            juce::MessageManager::callAsync ([ownerPtr = juce::Component::SafePointer<MainComponent> (&owner),
                                              commit, text]()
            {
                if (ownerPtr != nullptr)
                    ownerPtr->dismissNameOverlay (commit, text);
            });
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.88f));
            auto card = getLocalBounds().reduced (getWidth() / 30, getHeight() / 16);
            g.setColour (juce::Colour (0xff1a1a1a));
            g.fillRoundedRectangle (card.toFloat(), 14.0f);
        }

        void resized() override
        {
            auto card = getLocalBounds().reduced (getWidth() / 30, getHeight() / 16);
            auto top = card.removeFromTop (48).reduced (12, 4);
            cancelBtn.setBounds (top.removeFromRight (110).reduced (4));
            okBtn.setBounds (top.removeFromRight (110).reduced (4));
            titleLab.setBounds (top);
            editor.setBounds (card.removeFromTop (52).reduced (16, 4));
            card.removeFromTop (8);
            keyboard.setBounds (card.reduced (8, 4));
        }
    };

    auto* overlay = new Overlay (*this, shared, title);
    overlay->setBounds (getLocalBounds());
    nameOverlay.reset (overlay);
    addAndMakeVisible (overlay);
    overlay->toFront (true);

    juce::MessageManager::callAsync ([ed = &overlay->editor] { ed->grabKeyboardFocus(); });
}

void MainComponent::newPreset()
{
    editorWindow = nullptr;
    selectedIndex = -1;
    if (parameterPanel)
    {
        parameterPanel->clear();
        parameterPanel->setVisible (false);
    }
    audioEngine.setMuted (true);

    // Empty chain via blank XML
    juce::XmlElement empty ("QuadnonCortexPreset");
    empty.createNewChildElement ("PluginChain");
    audioEngine.getPluginChain().loadState (empty);
    // Clear midi maps for fresh board? Keep globals - only clear plugin maps by loading empty midi section
    juce::XmlElement midiClear ("MidiMaps");
    audioEngine.getMidiLearnManager().loadFromXml (empty);

    audioEngine.setMuted (false);
    currentPresetIndex = -1;
    presetBox.setSelectedItemIndex (-1, juce::dontSendNotification);
    rebuildBlocks();
    resized();
}

void MainComponent::savePreset()
{
    juce::String initial = "My Preset";
    if (juce::isPositiveAndBelow (currentPresetIndex, presetFiles.size()))
        initial = presetFiles[currentPresetIndex].getFileNameWithoutExtension();
    else if (AppSettings::get().lastPreset.isNotEmpty())
        initial = juce::File (AppSettings::get().lastPreset).getFileNameWithoutExtension();

    showPresetNameDialog ("Save Preset", initial, [this] (const juce::String& nameIn)
    {
        auto name = nameIn.trim();
        if (name.isEmpty())
            name = "Preset";
        name = name.replaceCharacters ("/\\:*?\"<>|", "---------");

        auto dir = AppSettings::get().getPresetsDir();
        dir.createDirectory();

        auto file = dir.getChildFile (name + ".xml");

        juce::XmlElement root ("QuadnonCortexPreset");
        audioEngine.getPluginChain().saveState (root);
        audioEngine.getMidiLearnManager().saveToXml (root);
        root.setAttribute ("inputGain",  audioEngine.getInputGain());
        root.setAttribute ("outputGain", audioEngine.getOutputGain());

        if (! root.writeTo (file))
            return;

        AppSettings::get().lastPreset = file.getFileName();
        AppSettings::get().save();

        refreshPresetList();

        currentPresetIndex = -1;
        for (int i = 0; i < presetFiles.size(); ++i)
        {
            if (presetFiles[i].getFileName() == file.getFileName())
            {
                currentPresetIndex = i;
                presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
                presetBox.setText (file.getFileNameWithoutExtension(), juce::dontSendNotification);
                break;
            }
        }
    });
}

void MainComponent::renamePreset()
{
    // Prefer the currently selected / last-loaded preset name
    juce::File oldFile;
    if (juce::isPositiveAndBelow (currentPresetIndex, presetFiles.size()))
        oldFile = presetFiles[currentPresetIndex];
    else if (AppSettings::get().lastPreset.isNotEmpty())
    {
        oldFile = AppSettings::get().getPresetsDir().getChildFile (AppSettings::get().lastPreset);
        // Keep index in sync so next/prev still work
        for (int i = 0; i < presetFiles.size(); ++i)
            if (presetFiles[i] == oldFile)
            {
                currentPresetIndex = i;
                break;
            }
    }

    if (! oldFile.existsAsFile())
        return;

    const juce::String oldName = oldFile.getFileNameWithoutExtension();

    showPresetNameDialog ("Rename Preset", oldName,
                          [this, oldFile] (const juce::String& nameIn)
    {
        auto name = nameIn.trim();
        if (name.isEmpty())
            name = "Preset";
        name = name.replaceCharacters ("/\\:*?\"<>|", "---------");

        auto newFile = AppSettings::get().getPresetsDir().getChildFile (name + ".xml");

        // Same name → nothing to do
        if (newFile == oldFile)
            return;

        // If target exists, pick a unique name
        int suffix = 2;
        while (newFile.existsAsFile())
        {
            newFile = AppSettings::get().getPresetsDir().getChildFile (
                name + " " + juce::String (suffix++) + ".xml");
        }

        // Copy then delete (more reliable than moveFileTo on Windows)
        const bool ok = oldFile.copyFileTo (newFile) && oldFile.deleteFile();
        if (! ok)
        {
            // Fallback: try move
            oldFile.moveFileTo (newFile);
        }

        if (! newFile.existsAsFile())
            return;

        AppSettings::get().lastPreset = newFile.getFileName();
        AppSettings::get().save();

        refreshPresetList();

        currentPresetIndex = -1;
        for (int i = 0; i < presetFiles.size(); ++i)
        {
            if (presetFiles[i] == newFile)
            {
                currentPresetIndex = i;
                presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
                break;
            }
        }

        // Force combo text to the new name
        if (currentPresetIndex >= 0)
            presetBox.setText (newFile.getFileNameWithoutExtension(), juce::dontSendNotification);
    });
}

void MainComponent::loadPreset (const juce::File& f)
{
    if (! f.existsAsFile()) return;

    // Close any open plugin editor first
    editorWindow = nullptr;

    // Clear UI selection so ParameterPanel releases parameter listeners
    selectedIndex = -1;
    if (parameterPanel)
    {
        parameterPanel->clear();
        parameterPanel->setVisible (false);
    }

    // Mute audio briefly while we swap the chain
    const bool wasMuted = audioEngine.isMuted();
    audioEngine.setMuted (true);

    if (auto xml = juce::XmlDocument::parse (f))
    {
        try
        {
            audioEngine.getPluginChain().loadState (*xml);
            audioEngine.getMidiLearnManager().loadFromXml (*xml);
            if (xml->hasAttribute ("inputGain"))
            {
                audioEngine.setInputGain ((float) xml->getDoubleAttribute ("inputGain"));
                inputFader.setValue (audioEngine.getInputGain(), juce::dontSendNotification);
            }
            if (xml->hasAttribute ("outputGain"))
            {
                audioEngine.setOutputGain ((float) xml->getDoubleAttribute ("outputGain"));
                outputFader.setValue (audioEngine.getOutputGain(), juce::dontSendNotification);
            }
        }
        catch (...) {}

        AppSettings::get().lastPreset = f.getFileName();
        AppSettings::get().save();
    }

    rebuildBlocks();
    audioEngine.setMuted (wasMuted);
    resized();
}

void MainComponent::presetNext()
{
    if (presetFiles.isEmpty()) return;
    currentPresetIndex = (currentPresetIndex + 1) % presetFiles.size();
    presetBox.setSelectedItemIndex (currentPresetIndex, juce::dontSendNotification);
    loadPreset (presetFiles[currentPresetIndex]);
}

void MainComponent::presetPrev()
{
    if (presetFiles.isEmpty()) return;
    currentPresetIndex = (currentPresetIndex - 1 + presetFiles.size()) % presetFiles.size();
    presetBox.setSelectedItemIndex (currentPresetIndex, juce::dontSendNotification);
    loadPreset (presetFiles[currentPresetIndex]);
}



bool MainComponent::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("PluginBlock:");
}

void MainComponent::itemDragEnter (const SourceDetails& d)
{
    itemDragMove (d);
}

void MainComponent::itemDragMove (const SourceDetails& d)
{
    // Screen-space hit test against trash button
    const auto screenPos = localPointToGlobal (d.localPosition.toInt());
    const bool over = trashZone.isVisible() && trashZone.getScreenBounds().contains (screenPos);

    if (over != dragOverTrash)
    {
        dragOverTrash = over;
        trashZone.setColour (juce::TextButton::buttonColourId,
                             over ? juce::Colour (0xffff1744) : juce::Colour (0xffc0392b));
        if (juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
            blocks[dragSourceIndex]->setDeleteHover (over);
    }
}

void MainComponent::itemDragExit (const SourceDetails&)
{
    dragOverTrash = false;
    trashZone.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
    if (juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
        blocks[dragSourceIndex]->setDeleteHover (false);
}

void MainComponent::itemDropped (const SourceDetails& d)
{
    const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();

    const auto screenPos = localPointToGlobal (d.localPosition.toInt());
    const bool over = trashZone.isVisible()
                      && (trashZone.getScreenBounds().expanded (12).contains (screenPos)
                          || trashZone.getBounds().expanded (12).contains (d.localPosition.toInt()));

    if (over || dragOverTrash)
        removePlugin (from);

    if (juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
        blocks[dragSourceIndex]->setDeleteHover (false);

    dragSourceIndex = -1;
    dragOverTrash = false;
    showTrash = false;
    trashZone.setVisible (false);
    trashZone.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
    resized();
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    // Click empty board / background → deselect (ignore clicks on blocks themselves)
    if (dynamic_cast<PluginBlockComponent*> (e.eventComponent) != nullptr)
        return;

    if (e.eventComponent == this || e.eventComponent == &blocksContent
        || e.eventComponent == &blocksViewport)
    {
        selectedIndex = -1;
        for (auto* b : blocks) b->setSelected (false);
        if (parameterPanel)
        {
            parameterPanel->clear();
            parameterPanel->setVisible (false);
        }
        resized();
    }
}

void MainComponent::showColourPicker (int pluginIndex)
{
    static const juce::uint32 palette[] = {
        0xffe67e22, 0xffe74c3c, 0xff9b59b6, 0xff3498db,
        0xff1abc9c, 0xff2ecc71, 0xfff1c40f, 0xffe91e63,
        0xff00bcd4, 0xffff5722, 0xff607d8b, 0xff795548,
        0xffffffff, 0xff000000, 0xffff9800, 0xff8bc34a
    };

    auto* panel = new juce::Component();
    panel->setSize (280, 200);
    const int cell = 56;
    for (int i = 0; i < 16; ++i)
    {
        auto* btn = new juce::TextButton();
        btn->setColour (juce::TextButton::buttonColourId, juce::Colour (palette[i]));
        btn->setBounds ((i % 4) * cell + 8, (i / 4) * cell + 8, cell - 8, cell - 8);
        const auto col = juce::Colour (palette[i]);
        btn->onClick = [this, pluginIndex, col, panel]
        {
            audioEngine.getPluginChain().setBlockColour (pluginIndex, col);
            rebuildBlocks();
            if (auto* dw = panel->findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        panel->addAndMakeVisible (btn);
    }

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (panel);
    opts.dialogTitle = "Block Colour";
    opts.dialogBackgroundColour = juce::Colour (0xff1a1a1a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = false;
    opts.resizable = false;
    opts.launchAsync();
}

void MainComponent::timerCallback()
{
    if (tuner != nullptr)
        tuner->setSampleRate (audioEngine.getSampleRate());
    smoothInPeak  = juce::jmax (audioEngine.getInputPeak(),  smoothInPeak  * 0.85f);
    smoothOutPeak = juce::jmax (audioEngine.getOutputPeak(), smoothOutPeak * 0.85f);

    // While dragging a block, poll mouse vs trash (works even if drop target is a block)
    if (showTrash)
    {
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        const bool over = trashZone.getScreenBounds().expanded (16).contains (mouse);
        if (over != dragOverTrash)
        {
            dragOverTrash = over;
            trashZone.setColour (juce::TextButton::buttonColourId,
                                 over ? juce::Colour (0xffff1744) : juce::Colour (0xffc0392b));
            if (juce::isPositiveAndBelow (dragSourceIndex, blocks.size()))
                blocks[dragSourceIndex]->setDeleteHover (over);
        }
    }

    repaint();
    if (parameterPanel && audioEngine.getMidiLearnManager().isLearning())
        parameterPanel->repaint();
}
