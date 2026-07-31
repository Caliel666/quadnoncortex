#include "MainComponent.h"
#include "DevLog.h"
#include "Theme.h"

static const juce::uint32 kPalette[] = {
    0xffe67e22, 0xffe74c3c, 0xff9b59b6, 0xff3498db,
    0xff1abc9c, 0xff2ecc71, 0xfff1c40f, 0xffe91e63,
    0xff00bcd4, 0xffff5722, 0xff607d8b, 0xff795548
};

MainComponent::MainComponent()
{
    setLookAndFeel (&Theme::get().softLaf);
    Theme::get().applyToLookAndFeel();
    {
        auto& th = Theme::get();
        titleLabel.setColour (juce::Label::textColourId, th.text);
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        presetBox.setComponentID ("presetSelector");
        presetBox.setLookAndFeel (&th.softLaf);
        presetBox.setColour (juce::ComboBox::backgroundColourId, th.surfaceAlt);
        presetBox.setColour (juce::ComboBox::textColourId, th.text);
        presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        presetBox.setColour (juce::ComboBox::arrowColourId, th.textDim);
        for (auto* b : { &prevPresetBtn, &nextPresetBtn, &newPresetBtn, &savePresetBtn,
                         &renamePresetBtn, &addButton, &settingsBtn, &tabPedal, &tabTuner })
            th.applyButton (*b);
        th.applyButton (addButton, true);
        th.applyButton (savePresetBtn, true);
        th.applyToggleTab (tabPedal, currentTab == 0);
        th.applyToggleTab (tabTuner, currentTab == 1);
    }

    titleLabel.setText ("quadnoncortex", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
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

    addButton.onClick = [this] { if (pluginBrowser) pluginBrowser->show (-1); };
    addAndMakeVisible (addButton);
    settingsBtn.onClick = [this] { openSettings(); };
    addAndMakeVisible (settingsBtn);

    trashZone.setButtonText ("X");
    trashZone.setColour (juce::TextButton::buttonColourId, Theme::get().danger);
    trashZone.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    trashZone.setVisible (false);
    addChildComponent (trashZone);

    auto setupFader = [] (juce::Slider& s)
    {
        auto& th = Theme::get();
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 2.0, 0.01);
        s.setValue (1.0);
        s.setColour (juce::Slider::thumbColourId, th.accent);
        s.setColour (juce::Slider::trackColourId, th.accent);
        s.setColour (juce::Slider::backgroundColourId, th.surfaceAlt);
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
    inLabel.setColour (juce::Label::textColourId, Theme::get().textDim);
    outLabel.setColour (juce::Label::textColourId, Theme::get().textDim);
    inLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    outLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (inLabel);
    addAndMakeVisible (outLabel);

    tabPedal.setClickingTogglesState (true);
    tabTuner.setClickingTogglesState (true);
    tabPedal.setRadioGroupId (1);
    tabTuner.setRadioGroupId (1);
    tabPedal.setToggleState (true, juce::dontSendNotification);
    Theme::get().applyToggleTab (tabPedal, true);
    Theme::get().applyToggleTab (tabTuner, false);
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
    audioEngine.getMidiLearnManager().loadGlobalsFromSettings();

    refreshPresetList();

    startTimerHz (30);
    setSize (1024, 600);

    // Load last preset after first layout so block positions/sizes are valid
    juce::MessageManager::callAsync ([this]
    {
        if (AppSettings::get().lastPreset.isNotEmpty())
        {
            auto f = AppSettings::get().getPresetsDir().getChildFile (AppSettings::get().lastPreset);
            if (! f.existsAsFile())
            {
                // filename-only match
                for (auto& pf : presetFiles)
                    if (pf.getFileName() == AppSettings::get().lastPreset
                        || pf.getFileNameWithoutExtension() == AppSettings::get().lastPreset)
                    {
                        f = pf;
                        break;
                    }
            }
            if (f.existsAsFile())
            {
                for (int i = 0; i < presetFiles.size(); ++i)
                    if (presetFiles[i] == f || presetFiles[i].getFileName() == f.getFileName())
                    {
                        currentPresetIndex = i;
                        presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        break;
                    }
                loadPreset (f);
                rebuildBlocks();
                resized();
            }
        }
        startBootScan();
    });
}

MainComponent::~MainComponent()
{
    setLookAndFeel (nullptr);
    stopTimer();
    editorWindow = nullptr;
    scanOverlay = nullptr;
    audioEngine.shutdown();
}

void MainComponent::openSettings()
{
    if (settingsOverlay != nullptr)
    {
        auto* p = settingsOverlay.get();
        juce::Desktop::getInstance().getAnimator().fadeOut (p, 180);
        juce::Timer::callAfterDelay (190, [this]
        {
            settingsOverlay.reset();
        });
        return;
    }

    auto* panel = new SettingsComponent (audioEngine, audioEngine.getMidiLearnManager());
    panel->setLookAndFeel (&Theme::get().softLaf);
    panel->onThemeChanged = [this]
    {
        auto& th = Theme::get();
        th.applyToLookAndFeel();
        setLookAndFeel (&th.softLaf);
        sendLookAndFeelChange(); // recursive to all children

        for (auto* s : { &inputFader, &outputFader })
        {
            s->setColour (juce::Slider::thumbColourId, th.accent);
            s->setColour (juce::Slider::trackColourId, th.accent);
            s->setColour (juce::Slider::backgroundColourId, th.surfaceAlt);
        }
        inLabel.setColour (juce::Label::textColourId, th.textDim);
        outLabel.setColour (juce::Label::textColourId, th.textDim);
        titleLabel.setColour (juce::Label::textColourId, th.text);
        presetBox.setLookAndFeel (&th.softLaf);
        presetBox.setColour (juce::ComboBox::backgroundColourId, th.surfaceAlt);
        presetBox.setColour (juce::ComboBox::textColourId, th.text);
        presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        presetBox.setColour (juce::ComboBox::arrowColourId, th.textDim);

        for (auto* b : { &prevPresetBtn, &nextPresetBtn, &newPresetBtn, &savePresetBtn,
                         &renamePresetBtn, &addButton, &settingsBtn, &tabPedal, &tabTuner })
            th.applyButton (*b);
        th.applyButton (addButton, true);
        th.applyButton (savePresetBtn, true);
        th.applyToggleTab (tabPedal, currentTab == 0);
        th.applyToggleTab (tabTuner, currentTab == 1);

        if (parameterPanel)
        {
            parameterPanel->setLookAndFeel (&th.softLaf);
            parameterPanel->sendLookAndFeelChange();
            parameterPanel->repaint();
        }
        if (tuner) { tuner->sendLookAndFeelChange(); tuner->repaint(); }
        if (pluginBrowser) { pluginBrowser->sendLookAndFeelChange(); pluginBrowser->repaint(); }
        for (auto* b : blocks) b->repaint();
        repaint();
    };
    panel->onCloseRequested = [this] { openSettings(); }; // toggles closed

    panel->setBounds (getLocalBounds());
    panel->setAlpha (0.0f);
    addAndMakeVisible (panel);
    panel->toFront (true);
    settingsOverlay.reset (panel);
    juce::Desktop::getInstance().getAnimator().fadeIn (panel, 250);
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
    Theme::get().applyToggleTab (tabPedal, board);
    Theme::get().applyToggleTab (tabTuner, ! board);
    resized();
}

void MainComponent::handleGlobalMidi (const juce::String& action, float value)
{
    if (action == "tuner")
        setTab (value >= 0.5f ? 1 : 0); // absolute: ON = tuner, OFF = board
    else if (action == "presetNext")
        presetNext();
    else if (action == "presetPrev")
        presetPrev();
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
        std::function<void()> onClosed;

        EditorWin (const juce::String& name, juce::AudioProcessorEditor* content)
            : DocumentWindow (name, juce::Colour (0xff1a1d23),
                              DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (false);
            setTitleBarHeight (36);
            setContentOwned (content, true);
            setResizable (true, false);
            centreWithSize (juce::jmax (400, content->getWidth()),
                            juce::jmax (300, content->getHeight() + 40));
            setVisible (true);
            setAlwaysOnTop (true);
            toFront (true);
        }

        void closeButtonPressed() override
        {
            // The owner destroys this window (and its owned editor) on the next
            // message-loop turn. Destroying it here would delete `this` while
            // this callback is still on the stack.
            setVisible (false);
            if (onClosed)
                onClosed();
        }
    };

    auto* win = new EditorWin (inst->getName(), ed);
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    win->onClosed = [safeThis, win]
    {
        juce::MessageManager::callAsync ([safeThis, win]
        {
            // A preset load may already have disposed of this window.
            if (safeThis != nullptr && safeThis->editorWindow.get() == win)
            {
                safeThis->editorWindow = nullptr;
                DevLog::log ("plugin editor closed and destroyed");
            }
        });
    };
    editorWindow.reset (win);
}

void MainComponent::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    juce::ColourGradient backdrop (th.background.brighter (0.025f), 0.0f, 0.0f,
                                   th.background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (backdrop);
    g.fillAll();

    // Side VU meters (only on board tab)
    if (currentTab == 0 && inputFader.isVisible())
    {
        auto left  = getLocalBounds();
        left.removeFromTop (kTopBar);
        left.removeFromBottom (kTabH);
        auto right = left.removeFromRight (kSideW);
        left = left.removeFromLeft (kSideW);

        left.removeFromBottom (22);
        right.removeFromBottom (22);
        // Meter strip on the inner edge of each side column
        auto inMeter  = left.removeFromRight (14).reduced (2, 8);
        auto outMeter = right.removeFromLeft (14).reduced (2, 8);
        paintMeter (g, inMeter,  smoothInPeak);
        paintMeter (g, outMeter, smoothOutPeak);
    }
}

void MainComponent::paintMeter (juce::Graphics& g, juce::Rectangle<int> area, float peak)
{
    auto& th = Theme::get();
    g.setColour (th.surfaceAlt);
    g.fillRoundedRectangle (area.toFloat(), 5.0f);

    const float db = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-6f), -60.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
    if (norm <= 0.001f) return;

    auto fill = area.removeFromBottom (juce::jmax (2, (int) (area.getHeight() * norm)));
    juce::Colour c = peak >= 0.98f ? th.danger
                     : peak >= 0.7f  ? th.warning
                                     : th.success;
    g.setColour (c);
    g.fillRoundedRectangle (fill.toFloat(), 4.0f);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    // ---- Top bar ----
    auto top = r.removeFromTop (kTopBar).reduced (12, 7);
    titleLabel.setBounds (top.removeFromLeft (145).reduced (2, 4));
    settingsBtn.setBounds (top.removeFromRight (50).reduced (2));
    addButton.setBounds (top.removeFromRight (50).reduced (2));
    renamePresetBtn.setBounds (top.removeFromRight (52).reduced (2));
    savePresetBtn.setBounds (top.removeFromRight (58).reduced (2));
    newPresetBtn.setBounds (top.removeFromRight (52).reduced (2));

    const int presetBoxW = juce::jlimit (180, 360, top.getWidth() / 2);
    const int clusterW = 40 + 4 + presetBoxW + 4 + 40;
    auto centre = top.withSizeKeepingCentre (juce::jmin (clusterW, top.getWidth()), top.getHeight());
    prevPresetBtn.setBounds (centre.removeFromLeft (40).reduced (2));
    presetBox.setBounds (centre.removeFromLeft (presetBoxW).reduced (2, 0));
    nextPresetBtn.setBounds (centre.removeFromLeft (40).reduced (2));

    // Trash sits below the top bar so it doesn't cover presets
    if (showTrash)
        trashZone.setBounds (getWidth() / 2 - 48, kTopBar + 12, 96, 56);

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
        inLabel.setBounds (left.removeFromBottom (22));
        left.removeFromRight (16); // room for VU painted in paint()
        inputFader.setBounds (left.reduced (4, 6));
        outLabel.setBounds (right.removeFromBottom (22));
        right.removeFromLeft (16);
        outputFader.setBounds (right.reduced (4, 6));

        const bool showParams = selectedIndex >= 0;
        // Low screens: give parameters most of the height; shrink the board
        const int h = r.getHeight();
        const int paramH = ! showParams ? 0
            : (h < 420 ? juce::jmax (220, (h * 58) / 100)
                       : juce::jmax (180, (h * 42) / 100));
        if (parameterPanel)
        {
            parameterPanel->setBounds (r.removeFromBottom (paramH));
            parameterPanel->setVisible (showParams);
        }

        blocksViewport.setBounds (r.reduced (4));
        // No scrollbars needed when we fit everything
        blocksViewport.setScrollBarsShown (false, false);

        const int n = blocks.size();
        const int availW = juce::jmax (80, blocksViewport.getWidth());
        const int availH = juce::jmax (80, blocksViewport.getHeight());
        const int gap = 16;
        int blockH = showParams
            ? juce::jlimit (76, 112, (int) (availH * 0.62f))
            : juce::jlimit (96, 136, (int) (availH * 0.34f));
        int blockW = juce::jlimit (104, 150, (int) (blockH * 1.14f));
        const int maxPerRow = juce::jmax (1, juce::jmin (6, (availW - 32) / (blockW + gap)));

        // Pack into rows of at most 5
        struct Row { int start = 0, count = 0; };
        juce::Array<Row> rows;
        {
            int i = 0;
            while (i < n)
            {
                Row row;
                row.start = i;
                while (i < n && row.count < maxPerRow)
                {
                    row.count++;
                    i++;
                }
                rows.add (row);
            }
        }
        const int rowCount = juce::jmax (1, rows.size());
        const int fittedHeight = (availH - 40 - gap * (rowCount - 1)) / rowCount;
        blockH = juce::jlimit (64, blockH, fittedHeight);
        blockW = juce::jlimit (80, blockW,
                               (availW - 48 - gap * (maxPerRow - 1)) / maxPerRow);
       #if 0
        const int numRows = juce::jmax (1, rows.size());

        // Target: use most of the board. Single row → large tiles; more rows → share height.
        const float fillW = 0.88f; // fraction of width to use
        const float fillH = 0.82f;

        int blockH = (int) ((availH * fillH - gap * (numRows + 1)) / (float) numRows);
        // When params are open on a short screen, allow smaller blocks
        if (showParams && getHeight() < 500)
            blockH = juce::jlimit (56, 120, blockH);
        else
            blockH = juce::jlimit (100, 180, blockH);

        blocksContent.setSize (availW, availH);
        const int gridH = numRows * blockH + (numRows + 1) * gap;
        const int originY = juce::jmax (gap, (availH - gridH) / 2);

        for (int r = 0; r < rows.size(); ++r)
        {
            const auto& row = rows.getReference (r);
            const int rowBudget = (int) (availW * fillW) - gap * (row.count - 1);
            // Minimum width so short names stay large enough to tap
            const int minW = showParams && getHeight() < 500
                ? juce::jlimit (64, 120, blockH)
                : juce::jlimit (100, 160, blockH - 10);

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
       #endif

        blocksContent.setSize (availW, availH);
        for (int r = 0; r < rows.size(); ++r)
        {
            const auto& row = rows.getReference (r);
            int x = 24;
            const int y = 20 + r * (blockH + gap);
            for (int c = 0; c < row.count; ++c)
            {
                blocks[row.start + c]->setBounds (x, y, blockW, blockH);
                x += blockW + gap;
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
    if (nameOverlay)     nameOverlay->setBounds (getLocalBounds());
    if (settingsOverlay) settingsOverlay->setBounds (getLocalBounds());
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
            trashZone.setButtonText ("X");
            trashZone.setColour (juce::TextButton::buttonColourId, Theme::get().danger);
            for (auto* b : blocks) b->setDragging (false);
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
    // Tap same block again → hide parameter panel
    if (index >= 0 && index == selectedIndex)
    {
        selectedIndex = -1;
        for (int i = 0; i < blocks.size(); ++i)
            blocks[i]->setSelected (false);
        if (parameterPanel)
        {
            parameterPanel->clear();
            parameterPanel->setVisible (false);
        }
        resized();
        return;
    }

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
                                       [this] { showColourPicker (selectedIndex); },
                                       [this] { showPluginEditor (selectedIndex); });
        }
        else
        {
            parameterPanel->clear();
            parameterPanel->setVisible (false);
        }
    }
    resized();

    // Bounce / slide-in the parameter panel
    if (parameterPanel != nullptr && selectedIndex >= 0 && parameterPanel->isVisible())
    {
        auto finalBounds = parameterPanel->getBounds();
        parameterPanel->setBounds (finalBounds.translated (0, 48));
        parameterPanel->setAlpha (0.0f);
        juce::Desktop::getInstance().getAnimator().animateComponent (
            parameterPanel.get(), finalBounds, 1.0f, 340, false, 1.6, 0.3);
    }
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

    struct Shared { juce::String text; };
    auto shared = std::make_shared<Shared>();
    shared->text = initial;

    class Overlay : public juce::Component, private juce::Timer
    {
    public:
        MainComponent& owner;
        std::shared_ptr<Shared> shared;
        juce::Label titleLab, nameLab;
        juce::TextEditor editor;
        juce::TextButton okBtn { "OK" }, cancelBtn { "CANCEL" };
        OnScreenKeyboard keyboard;
        float fade = 0.0f;

        Overlay (MainComponent& o, std::shared_ptr<Shared> s, const juce::String& titleTxt)
            : owner (o), shared (std::move (s))
        {
            setInterceptsMouseClicks (true, true);

            auto& th = Theme::get();

            titleLab.setText (titleTxt, juce::dontSendNotification);
            titleLab.setFont (juce::FontOptions (18.0f, juce::Font::bold));
            titleLab.setColour (juce::Label::textColourId, th.textDim);
            titleLab.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (titleLab);

            const juce::Font f (juce::FontOptions (28.0f, juce::Font::bold));
            editor.setFont (f);
            editor.setText (shared->text, false);
            editor.applyFontToAllText (f);
            editor.setColour (juce::TextEditor::textColourId, th.text);
            editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::highlightColourId, th.accent.withAlpha (0.35f));
            editor.setColour (juce::CaretComponent::caretColourId, th.accent);
            editor.setMultiLine (false);
            editor.setSelectAllWhenFocused (true);
            editor.setWantsKeyboardFocus (true);
            editor.setJustification (juce::Justification::centred);
            editor.onTextChange = [this]
            {
                shared->text = editor.getText();
            };
            editor.onReturnKey = [this] { finish (true); };
            addAndMakeVisible (editor);

            th.applyButton (okBtn, true);
            th.applyButton (cancelBtn, false);
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
                const juce::Font f2 (juce::FontOptions (28.0f, juce::Font::bold));
                editor.setText (shared->text, false);
                editor.applyFontToAllText (f2);
                editor.setCaretPosition (shared->text.length());
            };
            keyboard.onEnter = [this] { finish (true); };

            startTimerHz (60);
        }

        void timerCallback() override
        {
            fade = juce::jmin (1.0f, fade + 0.08f);
            repaint();
            if (fade >= 1.0f) stopTimer();
        }

        void finish (bool commit)
        {
            const juce::String text = shared->text;
            juce::MessageManager::callAsync ([ownerPtr = juce::Component::SafePointer<MainComponent> (&owner),
                                              commit, text]()
            {
                if (ownerPtr != nullptr)
                    ownerPtr->dismissNameOverlay (commit, text);
            });
        }

        void paint (juce::Graphics& g) override
        {
            auto& th = Theme::get();
            // Full-screen dark glass overlay (PlayStation-style)
            g.setColour (th.overlay.withMultipliedAlpha (fade));
            g.fillAll();

            // Subtle vignette
            juce::ColourGradient vig (juce::Colours::transparentBlack, (float) getWidth() * 0.5f, (float) getHeight() * 0.4f,
                                      juce::Colours::black.withAlpha (0.35f * fade), (float) getWidth() * 0.5f, (float) getHeight(), true);
            g.setGradientFill (vig);
            g.fillAll();

            // Name field underline only (no chrome box)
            auto field = editor.getBounds().toFloat().expanded (8.0f, 4.0f);
            g.setColour (th.accent.withAlpha (0.85f * fade));
            g.fillRoundedRectangle (field.getX(), field.getBottom() - 2.0f, field.getWidth(), 2.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (getWidth() / 18, getHeight() / 14);
            titleLab.setBounds (r.removeFromTop (28));
            r.removeFromTop (12);
            editor.setBounds (r.removeFromTop (52).reduced (r.getWidth() / 10, 0));
            r.removeFromTop (16);

            auto actions = r.removeFromTop (44);
            const int bw = 130;
            okBtn.setBounds (actions.getCentreX() - bw - 8, actions.getY(), bw, 40);
            cancelBtn.setBounds (actions.getCentreX() + 8, actions.getY(), bw, 40);
            r.removeFromTop (12);
            keyboard.setBounds (r);
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
    audioEngine.getPluginChain().setSuspended (true);
    juce::Thread::sleep (30);

    juce::XmlElement empty ("QuadnonCortexPreset");
    empty.createNewChildElement ("PluginChain");
    audioEngine.getPluginChain().loadState (empty);
    audioEngine.getMidiLearnManager().loadFromXml (empty);

    if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
        audioEngine.getPluginChain().prepare (dev->getCurrentSampleRate(),
                                              dev->getCurrentBufferSizeSamples());

    audioEngine.getPluginChain().setSuspended (false);
    audioEngine.setMuted (false);
    DevLog::log ("newPreset complete");
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
    if (presetLoading)
    {
        DevLog::log ("loadPreset SKIPPED (already loading): " + f.getFileName());
        return;
    }
    presetLoading = true;

    DevLog::log ("loadPreset BEGIN: " + f.getFullPathName());

    presetAnimating = true;
    blocksViewport.setAlpha (0.25f);
    if (parameterPanel) parameterPanel->setAlpha (0.25f);

    audioEngine.getMidiLearnManager().cancelLearn();

    selectedIndex = -1;
    if (parameterPanel)
    {
        parameterPanel->clear();
        parameterPanel->setVisible (false);
    }

    // 1) Close our editor window (releases owned AudioProcessorEditor)
    if (editorWindow != nullptr)
    {
        DevLog::log ("loadPreset: closing editor window");
        editorWindow->clearContentComponent();
        editorWindow = nullptr;
    }

    // 2) Delete any remaining active editors on processors (belt and suspenders)
    audioEngine.getPluginChain().closeAllEditors();

    const bool wasMuted = audioEngine.isMuted();
    audioEngine.setMuted (true);
    audioEngine.getPluginChain().setSuspended (true);

    // Let audio thread + editor teardown settle
    juce::Thread::sleep (50);

    const juce::File presetFile = f;
    // Run the heavy unload/load on the next message-loop turn so editor
    // destructors have fully finished (NAM is sensitive to this).
    juce::MessageManager::callAsync ([this, presetFile, wasMuted]
    {
        try
        {
            if (auto xml = juce::XmlDocument::parse (presetFile))
            {
                audioEngine.getPluginChain().loadState (*xml);

                if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
                {
                    DevLog::log ("loadPreset prepare sr=" + juce::String (dev->getCurrentSampleRate())
                                 + " bs=" + juce::String (dev->getCurrentBufferSizeSamples()));
                    audioEngine.getPluginChain().prepare (dev->getCurrentSampleRate(),
                                                          dev->getCurrentBufferSizeSamples());
                }
                else
                {
                    DevLog::log ("loadPreset WARNING: no audio device");
                }

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

                AppSettings::get().lastPreset = presetFile.getFileName();
                AppSettings::get().save();
            }
            else
            {
                DevLog::log ("loadPreset FAILED to parse XML");
            }
        }
        catch (const std::exception& ex)
        {
            DevLog::log (juce::String ("loadPreset EXCEPTION: ") + ex.what());
        }
        catch (...)
        {
            DevLog::log ("loadPreset UNKNOWN EXCEPTION");
        }

        rebuildBlocks();
        audioEngine.getPluginChain().setSuspended (false);
        audioEngine.setMuted (wasMuted);
        resized();

        DevLog::log ("loadPreset END: " + juce::String (audioEngine.getPluginChain().getNumPlugins())
                     + " plugins active");

        juce::Desktop::getInstance().getAnimator().fadeIn (&blocksViewport, 280);
        juce::Timer::callAfterDelay (400, [this]
        {
            presetAnimating = false;
            presetLoading = false;
            DevLog::log ("loadPreset unlock (loading flag cleared)");
        });
    });
}

void MainComponent::presetNext()
{
    if (presetFiles.isEmpty() || presetLoading) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastPresetSwitchMs < 280) return;
    lastPresetSwitchMs = now;
    currentPresetIndex = (currentPresetIndex + 1) % presetFiles.size();
    presetBox.setSelectedItemIndex (currentPresetIndex, juce::dontSendNotification);
    loadPreset (presetFiles[currentPresetIndex]);
}

void MainComponent::presetPrev()
{
    if (presetFiles.isEmpty() || presetLoading) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastPresetSwitchMs < 280) return;
    lastPresetSwitchMs = now;
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
        trashZone.setButtonText (over ? "DROP" : "X");
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
    // Tap the main UI → close plugin editor (helps when native title bar is off-screen)
    if (editorWindow != nullptr && editorWindow->isVisible())
        editorWindow = nullptr;

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
