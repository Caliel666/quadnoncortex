#include "MainComponent.h"
#include "DevLog.h"
#include "Theme.h"
#include "NativeNam/Tone3000Client.h"

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
        presetNameLabel.setColour (juce::Label::textColourId, th.text);
        presetNameLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        for (auto* b : { &tabPedal, &tabTuner })
            th.applyButton (*b);
        th.applyToggleTab (tabPedal, currentTab == 0);
        th.applyToggleTab (tabTuner, currentTab == 1);
    }

    // Big Quad Cortex-style preset name (tap opens spin picker)
    presetNameLabel.setText ("No Preset", juce::dontSendNotification);
    presetNameLabel.setFont (juce::FontOptions (72.0f, juce::Font::bold));
    presetNameLabel.setJustificationType (juce::Justification::centredLeft);
    presetNameLabel.setInterceptsMouseClicks (true, false);
    presetNameLabel.setRepaintsOnMouseActivity (true);
    addAndMakeVisible (presetNameLabel);
    presetNameLabel.addMouseListener (this, false);

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

    blocksContent.onPaint = [this] (juce::Graphics& g)
    {
        paintConnectionLines (g, blocksContent.getLocalBounds());
    };

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
                        updatePresetNameDisplay();
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
    DEV_LOG ("~MainComponent ENTER");
    setLookAndFeel (nullptr);
    stopTimer();
    if (editorWindow != nullptr)
    {
        DEV_LOGF ("~MainComponent: closing editorWindow=%p content=%p",
                 (void*) editorWindow.get(), (void*) editorWindow->getContentComponent());
        editorWindow->clearContentComponent();
    }
    editorWindow = nullptr;
    DEV_LOG ("~MainComponent: editorWindow nulled");
    scanOverlay = nullptr;
    DEV_LOG ("~MainComponent: calling audioEngine.shutdown()");
    audioEngine.shutdown();
    DEV_LOG ("~MainComponent EXIT");
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
        presetNameLabel.setColour (juce::Label::textColourId, th.text);

        for (auto* b : { &tabPedal, &tabTuner })
            th.applyButton (*b);
        th.applyToggleTab (tabPedal, currentTab == 0);
        th.applyToggleTab (tabTuner, currentTab == 1);
        for (auto* b : { &prevPresetBtn, &nextPresetBtn, &newPresetBtn, &savePresetBtn,
                         &renamePresetBtn, &addButton, &settingsBtn })
            b->repaint();

        if (parameterPanel)
        {
            parameterPanel->setLookAndFeel (&th.softLaf);
            parameterPanel->sendLookAndFeelChange();
            parameterPanel->repaint();
        }
        if (tuner) { tuner->sendLookAndFeelChange(); tuner->repaint(); }
        if (pluginBrowser) { pluginBrowser->sendLookAndFeelChange(); pluginBrowser->repaint(); }
        for (auto* b : blocks) b->repaint();
        blocksContent.repaint();
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
    DEV_LOGF ("showPluginEditor ENTER index=%d", index);
    auto* inst = audioEngine.getPluginChain().getPluginInstance (index);
    if (inst == nullptr || ! inst->hasEditor())
    {
        DEV_LOGF ("showPluginEditor ABORT: inst=%p hasEditor=%d", (void*) inst, inst ? inst->hasEditor() : -1);
        return;
    }
    DEV_LOGF ("showPluginEditor: inst=%p name='%s' activeEditor=%p",
             (void*) inst, inst->getName().toRawUTF8(), (void*) inst->getActiveEditor());

    // Close any existing editor window first
    if (editorWindow != nullptr)
    {
        DEV_LOGF ("showPluginEditor: closing existing editorWindow=%p", (void*) editorWindow.get());
        auto* prevContent = editorWindow->getContentComponent();
        DEV_LOGF ("showPluginEditor: clearContentComponent on editorWindow=%p content=%p",
                 (void*) editorWindow.get(), (void*) prevContent);
        editorWindow->clearContentComponent();
        DEV_LOGF ("showPluginEditor: clearContentComponent done, content now=%p, inst activeEditor=%p",
                 (void*) editorWindow->getContentComponent(), (void*) inst->getActiveEditor());
        editorWindow = nullptr;
        DEV_LOG ("showPluginEditor: old editorWindow destroyed");
    }

    auto* ed = inst->createEditorIfNeeded();
    DEV_LOGF ("showPluginEditor: createEditorIfNeeded returned ed=%p activeEditor now=%p",
             (void*) ed, (void*) inst->getActiveEditor());
    DEV_LOGF ("showPluginEditor: ed->getWidth=%d ed->getHeight=%d", ed ? ed->getWidth() : -1, ed ? ed->getHeight() : -1);
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
            DEV_LOGF ("EditorWin::closeButtonPressed this=%p content=%p",
                     (void*) this, (void*) getContentComponent());
            // The owner destroys this window (and its owned editor) on the next
            // message-loop turn. Destroying it here would delete `this` while
            // this callback is still on the stack.
            setVisible (false);
            DEV_LOGF ("EditorWin::closeButtonPressed: setVisible(false) done, calling onClosed");
            if (onClosed)
                onClosed();
        }
    };

    auto* win = new EditorWin (inst->getName(), ed);
    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    DEV_LOGF ("showPluginEditor: created EditorWin=%p ed=%p", (void*) win, (void*) ed);
    win->onClosed = [safeThis, win]
    {
        DEV_LOGF ("EditorWin::onClosed FIRE safeThis=%p win=%p", (void*) (MainComponent*) safeThis, (void*) win);
        juce::MessageManager::callAsync ([safeThis, win]
        {
            // A preset load may already have disposed of this window.
            DEV_LOGF ("EditorWin::onClosed ASYNC FIRE safeThis=%p editorWindow=%p win=%p",
                     (void*) (MainComponent*) safeThis, (void*) (safeThis ? safeThis->editorWindow.get() : nullptr), (void*) win);
            if (safeThis != nullptr && safeThis->editorWindow.get() == win)
            {
                DEV_LOGF ("EditorWin::onClosed ASYNC: NULLING editorWindow (was %p)", (void*) win);
                safeThis->editorWindow = nullptr;
                DevLog::log ("plugin editor closed and destroyed");
            }
            else
            {
                DEV_LOGF ("EditorWin::onClosed ASYNC: SKIP safeThis=%p match=%d",
                         (void*) (MainComponent*) safeThis,
                         safeThis ? (safeThis->editorWindow.get() == win) : -1);
            }
        });
    };
    editorWindow.reset (win);
    DEV_LOGF ("showPluginEditor EXIT: editorWindow now %p", (void*) editorWindow.get());
    DevLog::log ("plugin editor opened: " + inst->getName());
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
        left.removeFromTop (topBarH);
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

    // ---- Top bar: animated collapse when editing parameters ----
    const int fullTopBarH = juce::jlimit (kTopBarMin, 220, getHeight() * 24 / 100);
    if (! topBarCollapsed)
        targetTopBarH = (float) fullTopBarH;
    topBarH = (int) currentTopBarH;
    if (topBarH < 4)
    {
        // Fully collapsed — skip top bar layout, hide all children
        for (auto* c : { (juce::Component*) &prevPresetBtn, (juce::Component*) &nextPresetBtn,
                          (juce::Component*) &newPresetBtn, (juce::Component*) &savePresetBtn,
                          (juce::Component*) &renamePresetBtn, (juce::Component*) &addButton,
                          (juce::Component*) &settingsBtn, (juce::Component*) &presetNameLabel })
            c->setVisible (false);
    }
    else
    {
        for (auto* c : { (juce::Component*) &prevPresetBtn, (juce::Component*) &nextPresetBtn,
                          (juce::Component*) &newPresetBtn, (juce::Component*) &savePresetBtn,
                          (juce::Component*) &renamePresetBtn, (juce::Component*) &addButton,
                          (juce::Component*) &settingsBtn, (juce::Component*) &presetNameLabel })
            c->setVisible (true);

        auto top = r.removeFromTop (topBarH).reduced (16, 10);

        // Layout:  [ ^v ] [ PRESET NAME .............. ] [ icon grid ]
        const int gap = 8;
        const int idealCell = juce::jlimit (52, 80, (top.getHeight() - 2 * gap) / 3);
        // Don't shrink below 48px (same idea as the up/down pairH clamping)
        const int cell = juce::jmax (48, idealCell);

    // Right icon grid (equal squares) — capped height, won't shrink to zero
    //   [ save ] [ gear ]
    //   [  ren ] [  +   ]
    //   [ NEW  ]
    const int gridW = cell * 2 + gap;
    auto gridArea = top.removeFromRight (gridW);
    const int gridH = cell * 3 + gap * 2;
    auto grid = gridArea.withSizeKeepingCentre (gridW, gridH);
    top.removeFromRight (12);

    {
        auto colR = grid.removeFromRight (cell);
        auto colL = grid.removeFromLeft (cell);

        settingsBtn.setBounds (colR.removeFromTop (cell).reduced (2));
        colR.removeFromTop (gap);
        addButton.setBounds (colR.removeFromTop (cell).reduced (2));

        savePresetBtn.setBounds (colL.removeFromTop (cell).reduced (2));
        colL.removeFromTop (gap);
        renamePresetBtn.setBounds (colL.removeFromTop (cell).reduced (2));
        colL.removeFromTop (gap);
        newPresetBtn.setBounds (colL.removeFromTop (cell).reduced (2));
    }

    // LEFT: up/down chevrons (flat, no tile)
    const int navW = juce::jlimit (48, 72, cell);
    auto nav = top.removeFromLeft (navW);
    top.removeFromLeft (8);
    {
        const int pairH = juce::jmin (nav.getHeight(), cell * 2 + gap);
        auto band = nav.withSizeKeepingCentre (nav.getWidth(), pairH);
        prevPresetBtn.setBounds (band.removeFromTop (band.getHeight() / 2));
        nextPresetBtn.setBounds (band);
    }

    // Preset name fills the middle
    presetNameLabel.setBounds (top);
    updatePresetNameDisplay();
    } // end topBarH >= 4

    // Trash sits at the bottom center, above the tab bar
    if (showTrash)
        trashZone.setBounds (getWidth() / 2 - 48, getHeight() - kTabH - 64, 96, 56);

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

        const int n = blocks.size();
        const int gap = 16;
        const int availW = juce::jmax (80, r.getWidth() - 8);
        const int rawAvailH = juce::jmax (80, r.getHeight() - 8);
        // When param panel is showing, cap block area so blocks stay compact
        // and param panel sits right below them instead of at the bottom.
        const bool willShowParams = selectedIndex >= 0;
        const int availH = willShowParams ? juce::jmin (rawAvailH, 200) : rawAvailH;

        // Calculate block size: single row if possible, shrink when many plugins
        int blockH, blockW;
        if (n == 0)
        {
            blockH = 96; blockW = 96;
        }
        else
        {
            // Start with an ideal size and fit as many as possible per row
            blockH = juce::jlimit (64, 136, availH / 2);
            blockW = juce::jlimit (80, 150, (int) (blockH * 1.14f));

            // How many actually fit in the width?
            int perRow = juce::jmax (1, (availW - 24 + gap) / (blockW + gap));
            if (perRow > n) perRow = n;

            // Recalculate block size to fill width nicely
            blockW = (availW - 48 - gap * (perRow - 1)) / perRow;
            blockW = juce::jlimit (80, 150, blockW);
            blockH = juce::jlimit (64, 136, (int) (blockW / 1.14f));

            // If blocks would need multiple rows, check if they fit vertically
            int numRows = (n + perRow - 1) / perRow;
            int neededH = numRows * blockH + (numRows - 1) * gap + 40;
            if (neededH > availH && numRows > 1)
            {
                // Shrink block height to fit
                blockH = (availH - 40 - (numRows - 1) * gap) / numRows;
                blockH = juce::jmax (56, blockH);
                blockW = juce::jlimit (80, 150, (int) (blockH * 1.14f));
                // Recalc perRow with new blockW
                perRow = juce::jmax (1, (availW - 24 + gap) / (blockW + gap));
                if (perRow > n) perRow = n;
                blockW = (availW - 48 - gap * (perRow - 1)) / perRow;
                blockW = juce::jlimit (80, 150, blockW);
                numRows = (n + perRow - 1) / perRow;
            }
        }

        // Determine which row the selected block is in
        int perRowFinal = (n > 0) ? juce::jmax (1, (availW - 24 + gap) / (blockW + gap)) : 1;
        if (perRowFinal > n) perRowFinal = n;
        int selectedRow = 0;
        if (selectedIndex >= 0 && selectedIndex < n)
            selectedRow = selectedIndex / perRowFinal;

        // Parameter panel: sits directly below the blocks, not pushed to the bottom.
        // The blocks viewport takes only the space it needs (up to 1 row),
        // and the parameter panel fills the rest.
        const bool showParams = selectedIndex >= 0;
        int paramH = 0;
        if (showParams)
            paramH = blockH + 56; // match one row of blocks height + header bar

        if (showParams)
        {
            // Give blocks viewport exactly one row height, rest goes to param panel
            auto blocksArea = r.removeFromTop (blockH + gap + 40);
            blocksViewport.setBounds (blocksArea.reduced (4));
            blocksViewport.setScrollBarsShown (false, false);
            parameterPanel->setBounds (r.getX(), r.getY(), r.getWidth(), r.getHeight() - 4);
            parameterPanel->setVisible (true);
        }
        else
        {
            blocksViewport.setBounds (r.reduced (4));
            blocksViewport.setScrollBarsShown (false, false);
            if (parameterPanel)
                parameterPanel->setVisible (false);
        }

        // Build rows dynamically
        struct Row { int start = 0, count = 0; };
        juce::Array<Row> rows;
        {
            int i = 0;
            while (i < n)
            {
                Row row;
                row.start = i;
                // Dynamic: fill as many as fit in width
                int rowUsed = 0;
                while (i < n)
                {
                    int nextW = (rowUsed == 0) ? blockW : blockW + gap;
                    if (rowUsed > 0 && rowUsed + blockW + gap > availW - 48)
                        break;
                    row.count++;
                    rowUsed += (row.count == 1) ? blockW : blockW + gap;
                    i++;
                }
                if (row.count == 0) { row.count = 1; i++; } // safety
                rows.add (row);
            }
        }

        // If only one partial row fits, allow vertical scrolling
        const int rowCount = rows.size();
        int contentH = juce::jmax (availH, rowCount * (blockH + gap) + 40);
        blocksContent.setSize (availW, contentH);

        // Center blocks horizontally in each row
        for (int ri = 0; ri < rows.size(); ++ri)
        {
            const auto& row = rows.getReference (ri);
            const int rowTotalW = row.count * blockW + (row.count - 1) * gap;
            int x = juce::jmax (12, (availW - rowTotalW) / 2);
            const int y = 20 + ri * (blockH + gap);
            for (int c = 0; c < row.count; ++c)
            {
                if (row.start + c < blocks.size())
                    blocks[row.start + c]->setBounds (x, y, blockW, blockH);
                x += blockW + gap;
            }
        }

        // Auto-scroll: move viewport so the selected block's row is at the top
        if (selectedIndex >= 0 && selectedIndex < n)
        {
            const int selRow = selectedIndex / perRowFinal;
            const int targetY = selRow * (blockH + gap);
            const int maxScroll = juce::jmax (0, contentH - blocksViewport.getHeight());
            const int scrollY = juce::jmin (targetY - 10, maxScroll);
            blocksViewport.setViewPosition (0, juce::jmax (0, scrollY));
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
    DEV_LOGF ("rebuildBlocks ENTER: editorWindow=%p numPlugins=%d",
             (void*) editorWindow.get(), audioEngine.getPluginChain().getNumPlugins());
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
        topBarCollapsed = false;
        targetTopBarH = (float) juce::jlimit (kTopBarMin, 220, getHeight() * 24 / 100);
        currentTopBarH = (float) kTopBarMin; // expand from minimum, not zero — avoids mid-animation invisibility
        resized();
        return;
    }

    selectedIndex = index;
    // Snap top bar immediately so there is only one animation (the param panel slide-in)
    topBarCollapsed = true;
    targetTopBarH = (float) kTopBarCollapsedH;
    currentTopBarH = (float) kTopBarCollapsedH;
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->setSelected (i == selectedIndex);

    if (parameterPanel)
    {
        if (selectedIndex >= 0)
        {
            auto& chain = audioEngine.getPluginChain();
            parameterPanel->setPlugin (chain.getPluginInstance (selectedIndex), selectedIndex,
                                       chain.isBypassed (selectedIndex),
                                       chain.isMono (selectedIndex),
                                       [this]
                                       {
                                           audioEngine.getPluginChain().toggleBypass (selectedIndex);
                                           rebuildBlocks();
                                           parameterPanel->updateBypass (
                                               audioEngine.getPluginChain().isBypassed (selectedIndex));
                                       },
                                       [this]
                                       {
                                           audioEngine.getPluginChain().toggleMono (selectedIndex);
                                           rebuildBlocks();
                                           parameterPanel->updateMono (
                                               audioEngine.getPluginChain().isMono (selectedIndex));
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

static inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

void MainComponent::removePlugin (int index)
{
    DEV_LOGF ("removePlugin ENTER index=%d", index);
    static int lastRemoved = -999;
    static juce::uint32 lastTime = 0;
    const auto now = juce::Time::getMillisecondCounter();
    if (index == lastRemoved && (now - lastTime) < 400)
    {
        DEV_LOG ("removePlugin: debounced");
        return; // debounce double-delete from drag end + drop
    }
    lastRemoved = index;
    lastTime = now;
    if (index < 0) return;

    // Close editor if it belongs to this plugin
    if (editorWindow != nullptr)
    {
        DEV_LOGF ("removePlugin: closing editorWindow=%p content=%p",
                 (void*) editorWindow.get(), (void*) editorWindow->getContentComponent());
        editorWindow->clearContentComponent();
        editorWindow = nullptr;
        DEV_LOG ("removePlugin: editorWindow destroyed");
    }

    if (selectedIndex == index)
    {
        selectedIndex = -1;
        topBarCollapsed = false;
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



//==============================================================================
struct PresetPickerOverlay : public juce::Component, private juce::Timer
{
    MainComponent& owner;
    int focusIndex = 0;
    float animOffset = 0.0f;
    float targetOffset = 0.0f;
    float fade = 0.0f;
    int dragStartY = 0;
    int dragStartIndex = 0;
    bool dragging = false;

    explicit PresetPickerOverlay (MainComponent& o) : owner (o)
    {
        focusIndex = juce::jlimit (0, juce::jmax (0, o.presetFiles.size() - 1),
                                   o.currentPresetIndex >= 0 ? o.currentPresetIndex : 0);
        startTimerHz (60);
        setOpaque (false);
    }

    juce::String nameAt (int i) const
    {
        if (! juce::isPositiveAndBelow (i, owner.presetFiles.size()))
            return {};
        return owner.presetFiles[i].getFileNameWithoutExtension();
    }

    void paint (juce::Graphics& g) override
    {
        auto& th = Theme::get();
        g.setColour (th.overlay.withMultipliedAlpha (fade));
        g.fillAll();

        const float W = (float) getWidth();
        const float H = (float) getHeight();
        const float midY = H * 0.5f;
        const float rowH = juce::jlimit (64.0f, 130.0f, H * 0.16f);

        // No coloured selection chrome — clarity from size + opacity only
        for (int delta = -3; delta <= 3; ++delta)
        {
            const int idx = focusIndex + delta;
            if (! juce::isPositiveAndBelow (idx, owner.presetFiles.size()))
                continue;

            const float y = midY + ((float) delta * rowH) + animOffset;
            const float dist = std::abs ((y - midY) / rowH);
            const float scale = juce::jlimit (0.40f, 1.0f, 1.0f - dist * 0.32f);
            const float alpha = juce::jlimit (0.18f, 1.0f, 1.0f - dist * 0.40f) * fade;
            // Centre name is large; neighbours step down
            const float fontSize = (delta == 0 ? 72.0f : 36.0f) * scale;

            g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
            g.setColour ((delta == 0 ? th.text : th.textDim).withMultipliedAlpha (alpha));
            g.drawText (nameAt (idx),
                        juce::Rectangle<float> (0.0f, y - rowH * 0.5f, W, rowH),
                        juce::Justification::centred, true);
        }
    }

    void timerCallback() override
    {
        fade = juce::jmin (1.0f, fade + 0.12f);
        animOffset += (targetOffset - animOffset) * 0.28f;
        if (std::abs (animOffset - targetOffset) < 0.4f)
            animOffset = targetOffset;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        dragStartY = e.y;
        dragStartIndex = focusIndex;
        targetOffset = animOffset;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float rowH = juce::jlimit (64.0f, 130.0f, (float) getHeight() * 0.16f);
        animOffset = targetOffset = (float) (e.y - dragStartY);
        const int steps = (int) std::round (-animOffset / rowH);
        const int ni = juce::jlimit (0, owner.presetFiles.size() - 1, dragStartIndex + steps);
        if (ni != focusIndex)
        {
            const int diff = ni - focusIndex;
            focusIndex = ni;
            animOffset += (float) diff * rowH;
            targetOffset = animOffset;
            dragStartY = e.y;
            dragStartIndex = focusIndex;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        dragging = false;
        const float rowH = juce::jlimit (64.0f, 130.0f, (float) getHeight() * 0.16f);
        const float midY = (float) getHeight() * 0.5f;

        if (e.mouseWasClicked() && std::abs (e.getDistanceFromDragStartY()) < 12)
        {
            if (std::abs ((float) e.y - midY) < rowH * 0.7f)
            {
                commitAndClose();
                return;
            }
            step ((float) e.y < midY ? -1 : 1);
            return;
        }

        const int steps = (int) std::round (-animOffset / rowH);
        focusIndex = juce::jlimit (0, owner.presetFiles.size() - 1, dragStartIndex + steps);
        animOffset = targetOffset = 0.0f;
        repaint();
    }

    void step (int dir)
    {
        const int ni = juce::jlimit (0, owner.presetFiles.size() - 1, focusIndex + dir);
        if (ni == focusIndex) return;
        const float rowH = juce::jlimit (64.0f, 130.0f, (float) getHeight() * 0.16f);
        animOffset = (float) dir * rowH;
        targetOffset = 0.0f;
        focusIndex = ni;
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY > 0.05f) step (-1);
        else if (w.deltaY < -0.05f) step (1);
    }

    void commitAndClose()
    {
        const int idx = focusIndex;
        auto& o = owner;
        o.presetPickerOverlay.reset();
        if (juce::isPositiveAndBelow (idx, o.presetFiles.size()))
        {
            o.currentPresetIndex = idx;
            o.updatePresetNameDisplay();
            o.loadPreset (o.presetFiles[idx]);
        }
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            owner.presetPickerOverlay.reset();
            return true;
        }
        if (key == juce::KeyPress::upKey)   { step (-1); return true; }
        if (key == juce::KeyPress::downKey) { step (1);  return true; }
        if (key == juce::KeyPress::returnKey) { commitAndClose(); return true; }
        return false;
    }
};

void MainComponent::showPresetPickerOverlay()
{
    if (presetFiles.isEmpty())
        return;

    if (presetPickerOverlay != nullptr)
    {
        presetPickerOverlay.reset();
        return;
    }

    auto* picker = new PresetPickerOverlay (*this);
    picker->setBounds (getLocalBounds());
    addAndMakeVisible (picker);
    picker->toFront (true);
    picker->grabKeyboardFocus();
    presetPickerOverlay.reset (picker);
}

void MainComponent::refreshPresetList()
{
    presetFiles.clear();
    auto dir = AppSettings::get().getPresetsDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    // Alphabetical (so 01, 02, 03... sort naturally by name)
    files.sort();
    for (auto& f : files)
        presetFiles.add (f);
    updatePresetNameDisplay();
}

void MainComponent::updatePresetNameDisplay()
{
    juce::String name = "No Preset";
    if (juce::isPositiveAndBelow (currentPresetIndex, presetFiles.size()))
        name = presetFiles[currentPresetIndex].getFileNameWithoutExtension();
    else if (AppSettings::get().lastPreset.isNotEmpty())
        name = juce::File (AppSettings::get().lastPreset).getFileNameWithoutExtension();

    presetNameLabel.setText (name, juce::dontSendNotification);

    // Fill the top-bar height with the name (cap only for extreme ultrawide)
    const int availW = juce::jmax (80, presetNameLabel.getWidth() - 8);
    const int availH = juce::jmax (40, presetNameLabel.getHeight());
    float fs = juce::jmin (110.0f, (float) availH * 0.92f);
    if (availW > 40)
    {
        for (; fs > 24.0f; fs -= 1.5f)
        {
            juce::Font f (juce::FontOptions (fs, juce::Font::bold));
            juce::GlyphArrangement ga;
            ga.addLineOfText (f, name, 0.0f, 0.0f);
            if (ga.getBoundingBox (0, -1, true).getWidth() <= (float) availW)
                break;
        }
    }
    presetNameLabel.setFont (juce::FontOptions (fs, juce::Font::bold));
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
    // Close any open plugin editor before clearing chain
    if (editorWindow != nullptr)
    {
        DEV_LOGF ("newPreset: closing editorWindow=%p content=%p",
                 (void*) editorWindow.get(), (void*) editorWindow->getContentComponent());
        editorWindow->clearContentComponent();
        editorWindow = nullptr;
    }
    else
        audioEngine.getPluginChain().closeAllEditors();
    selectedIndex = -1;
    topBarCollapsed = false;
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
    updatePresetNameDisplay();
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
                updatePresetNameDisplay();
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
                updatePresetNameDisplay();
                break;
            }
        }

        // Force combo text to the new name
        updatePresetNameDisplay();
    });
}

void MainComponent::loadPreset (const juce::File& f)
{
    DEV_LOGF ("loadPreset ENTER file='%s'", f.getFullPathName().toRawUTF8());

    if (! f.existsAsFile())
    {
        DEV_LOG ("loadPreset ABORT: file does not exist");
        return;
    }
    if (presetLoading)
    {
        DevLog::log ("loadPreset SKIPPED (already loading): " + f.getFileName());
        return;
    }
    presetLoading = true;

    DevLog::log ("loadPreset BEGIN: " + f.getFullPathName());
    Tone3000Client::get().cancelLogin(); // stop any OAuth loopback before chain rebuild
    DEV_LOGF ("loadPreset: editorWindow=%p presetLoading=%d presetAnimating=%d",
             (void*) editorWindow.get(), presetLoading, presetAnimating);

    // Scan all plugin instances for any active editors BEFORE we touch anything
    {
        auto& chain = audioEngine.getPluginChain();
        for (int i = 0; i < chain.getNumPlugins(); ++i)
        {
            if (auto* inst = chain.getPluginInstance (i))
            {
                DEV_LOGF ("loadPreset: plugin[%d] '%s' inst=%p activeEditor=%p",
                         i, inst->getName().toRawUTF8(),
                         (void*) inst, (void*) inst->getActiveEditor());
            }
        }
    }

    presetAnimating = true;
    blocksViewport.setAlpha (0.25f);
    if (parameterPanel) parameterPanel->setAlpha (0.25f);

    audioEngine.getMidiLearnManager().cancelLearn();

    selectedIndex = -1;
    topBarCollapsed = false;
    if (parameterPanel)
    {
        parameterPanel->clear();
        parameterPanel->setVisible (false);
    }

    // 1) Close our editor window (releases owned AudioProcessorEditor)
    if (editorWindow != nullptr)
    {
        auto* content = editorWindow->getContentComponent();
        DEV_LOGF ("loadPreset step1: editorWindow=%p content=%p — calling clearContentComponent",
                 (void*) editorWindow.get(), (void*) content);
        DevLog::log ("loadPreset: closing editor window");
        editorWindow->clearContentComponent();
        DEV_LOG ("loadPreset step1: clearContentComponent DONE");
        editorWindow = nullptr;
        DEV_LOG ("loadPreset step1: editorWindow nulled");
    }
    else
    {
        DEV_LOG ("loadPreset step1: editorWindow is already null");
    }

    // Re-scan: are any editors still alive after clearContentComponent?
    {
        auto& chain = audioEngine.getPluginChain();
        for (int i = 0; i < chain.getNumPlugins(); ++i)
        {
            if (auto* inst = chain.getPluginInstance (i))
            {
                auto* ed = inst->getActiveEditor();
                if (ed != nullptr)
                    DEV_LOGF ("loadPreset WARNING: plugin[%d] '%s' still has activeEditor=%p after clearContentComponent!",
                             i, inst->getName().toRawUTF8(), (void*) ed);
            }
        }
    }

    // 2) Delete any remaining active editors on processors (belt and suspenders)
    DEV_LOG ("loadPreset step2: calling closeAllEditors()");
    audioEngine.getPluginChain().closeAllEditors();
    DEV_LOG ("loadPreset step2: closeAllEditors() returned");

    // Final scan: confirm NO editors remain
    {
        auto& chain = audioEngine.getPluginChain();
        for (int i = 0; i < chain.getNumPlugins(); ++i)
        {
            if (auto* inst = chain.getPluginInstance (i))
            {
                auto* ed = inst->getActiveEditor();
                if (ed != nullptr)
                    DEV_LOGF ("loadPreset CRITICAL: plugin[%d] '%s' STILL has activeEditor=%p after closeAllEditors!",
                             i, inst->getName().toRawUTF8(), (void*) ed);
            }
        }
    }

    const bool wasMuted = audioEngine.isMuted();
    audioEngine.setMuted (true);
    audioEngine.getPluginChain().setSuspended (true);
    DEV_LOGF ("loadPreset: muted=%d suspended=true, sleeping 50ms", wasMuted);

    // Let audio thread + editor teardown settle
    juce::Thread::sleep (50);
    DEV_LOG ("loadPreset: sleep done, posting callAsync");

    const juce::File presetFile = f;
    // Run the heavy unload/load on the next message-loop turn so editor
    // destructors have fully finished (NAM is sensitive to this).
    juce::MessageManager::callAsync ([this, presetFile, wasMuted]
    {
        DEV_LOGF ("loadPreset ASYNC ENTER: file='%s'", presetFile.getFileName().toRawUTF8());
        DEV_LOGF ("loadPreset ASYNC: this=%p editorWindow=%p",
                 (void*) this, (void*) editorWindow.get());

        // Re-check: did something re-open an editor between sleep and async fire?
        if (editorWindow != nullptr)
        {
            DEV_LOGF ("loadPreset ASYNC WARNING: editorWindow=%p reappeared before async body!",
                     (void*) editorWindow.get());
        }
        // Re-check: any active editors on plugins?
        {
            auto& chain = audioEngine.getPluginChain();
            for (int i = 0; i < chain.getNumPlugins(); ++i)
            {
                if (auto* inst = chain.getPluginInstance (i))
                {
                    auto* ed = inst->getActiveEditor();
                    if (ed != nullptr)
                        DEV_LOGF ("loadPreset ASYNC CRITICAL: plugin[%d] has activeEditor=%p at async entry!",
                                 i, (void*) ed);
                }
            }
        }

        try
        {
            if (auto xml = juce::XmlDocument::parse (presetFile))
            {
                DEV_LOG ("loadPreset ASYNC: XML parsed OK, calling loadState");
                audioEngine.getPluginChain().loadState (*xml);
                DEV_LOG ("loadPreset ASYNC: loadState returned");

                if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
                {
                    DevLog::log ("loadPreset prepare sr=" + juce::String (dev->getCurrentSampleRate())
                                 + " bs=" + juce::String (dev->getCurrentBufferSizeSamples()));
                    audioEngine.getPluginChain().prepare (dev->getCurrentSampleRate(),
                                                          dev->getCurrentBufferSizeSamples());
                    DEV_LOG ("loadPreset ASYNC: prepare done");
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

        DEV_LOG ("loadPreset ASYNC: calling rebuildBlocks");
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
            DEV_LOG ("loadPreset ASYNC: loading flag cleared");
        });
    });
    DEV_LOG ("loadPreset: callAsync posted, returning");
}

void MainComponent::presetNext()
{
    DEV_LOGF ("presetNext: presetLoading=%d presetFiles.size=%d currentIdx=%d",
             presetLoading, presetFiles.size(), currentPresetIndex);
    if (presetFiles.isEmpty() || presetLoading) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastPresetSwitchMs < 280) return;
    lastPresetSwitchMs = now;
    currentPresetIndex = (currentPresetIndex + 1) % presetFiles.size();
    updatePresetNameDisplay();
    DEV_LOGF ("presetNext: switching to index %d '%s'", currentPresetIndex,
             presetFiles[currentPresetIndex].getFileName().toRawUTF8());
    loadPreset (presetFiles[currentPresetIndex]);
}

void MainComponent::presetPrev()
{
    DEV_LOGF ("presetPrev: presetLoading=%d presetFiles.size=%d currentIdx=%d",
             presetLoading, presetFiles.size(), currentPresetIndex);
    if (presetFiles.isEmpty() || presetLoading) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastPresetSwitchMs < 280) return;
    lastPresetSwitchMs = now;
    currentPresetIndex = (currentPresetIndex - 1 + presetFiles.size()) % presetFiles.size();
    updatePresetNameDisplay();
    DEV_LOGF ("presetPrev: switching to index %d '%s'", currentPresetIndex,
             presetFiles[currentPresetIndex].getFileName().toRawUTF8());
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
    {
        DEV_LOGF ("mouseDown: closing visible editorWindow=%p content=%p",
                 (void*) editorWindow.get(), (void*) editorWindow->getContentComponent());
        editorWindow->clearContentComponent();
        editorWindow = nullptr;
        DEV_LOG ("mouseDown: editorWindow destroyed");
    }

    // Tap big preset name → open spin picker overlay
    if (e.eventComponent == &presetNameLabel
        || (e.eventComponent == this && presetNameLabel.getBounds().contains (e.getPosition())))
    {
        showPresetPickerOverlay();
        return;
    }

    // Click empty board / background → deselect (ignore clicks on blocks themselves)
    if (dynamic_cast<PluginBlockComponent*> (e.eventComponent) != nullptr)
        return;

    if (e.eventComponent == this || e.eventComponent == &blocksContent
        || e.eventComponent == &blocksViewport)
    {
        selectedIndex = -1;
        topBarCollapsed = false;
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

int MainComponent::getBlockRow (int index) const
{
    if (index < 0 || index >= blocks.size()) return 0;
    return blocks[index]->getY(); // row 0 starts at y=20, row 1 at y=20+blockH+gap, etc.
}

void MainComponent::paintConnectionLines (juce::Graphics& g, const juce::Rectangle<int>&)
{
    const int n = blocks.size();
    if (n <= 1) return;

    auto& th = Theme::get();
    const auto lineColour = th.text.withAlpha (0.12f);
    const auto monoLineColour = th.accent.withAlpha (0.18f);

    // Use a thin stroke
    g.setColour (lineColour);

    for (int i = 0; i < n - 1; ++i)
    {
        auto* srcBlock = blocks[i];
        auto* dstBlock = blocks[i + 1];
        if (srcBlock == nullptr || dstBlock == nullptr) continue;

        // Skip bypassed plugins — draw a line from last active to next active
        const bool srcBypassed = audioEngine.getPluginChain().isBypassed (i);
        const bool dstBypassed = audioEngine.getPluginChain().isBypassed (i + 1);

        // Dim the line if either end is bypassed
        const float bypassAlpha = (srcBypassed || dstBypassed) ? 0.04f : 1.0f;

        const bool srcMono = audioEngine.getPluginChain().isMono (i);
        const bool dstMono = audioEngine.getPluginChain().isMono (i + 1);

        // Connection points: right edge of source, left edge of destination
        const float srcX = (float) (srcBlock->getRight());
        const float dstX = (float) (dstBlock->getX());
        const float srcY = (float) (srcBlock->getY() + srcBlock->getHeight() / 2);
        const float dstY = (float) (dstBlock->getY() + dstBlock->getHeight() / 2);

        // Skip if blocks are in different rows (vertical gap too large)
        if (std::abs (srcY - dstY) > srcBlock->getHeight() * 1.5f)
            continue;

        const float stereoSpread = juce::jmin (8.0f, (float) srcBlock->getHeight() * 0.08f);

        // Determine line style based on source/dest mono state
        // Both stereo: 2 parallel lines
        // Either mono: 1 line (mono sums to center)
        const bool isMonoPath = srcMono || dstMono;

        if (isMonoPath)
        {
            // Single center line
            g.setColour (monoLineColour.withMultipliedAlpha (bypassAlpha));
            g.drawLine (srcX, srcY, dstX, dstY, 1.5f);
        }
        else
        {
            // Stereo: two parallel lines (L and R)
            g.setColour (lineColour.withMultipliedAlpha (bypassAlpha));
            g.drawLine (srcX, srcY - stereoSpread, dstX, dstY - stereoSpread, 1.0f);
            g.drawLine (srcX, srcY + stereoSpread, dstX, dstY + stereoSpread, 1.0f);
        }
    }
}

void MainComponent::timerCallback()
{
    if (tuner != nullptr)
        tuner->setSampleRate (audioEngine.getSampleRate());
    smoothInPeak  = juce::jmax (audioEngine.getInputPeak(),  smoothInPeak  * 0.85f);
    smoothOutPeak = juce::jmax (audioEngine.getOutputPeak(), smoothOutPeak * 0.85f);

    // Animate top bar collapse/expand
    const float fullH = (float) juce::jlimit (kTopBarMin, 220, getHeight() * 24 / 100);
    if (! topBarCollapsed)
        targetTopBarH = fullH;
    if (std::abs (currentTopBarH - targetTopBarH) > 0.5f)
    {
        currentTopBarH = lerp (currentTopBarH, targetTopBarH, 0.18f);
        resized();
    }
    else if (currentTopBarH != targetTopBarH)
    {
        currentTopBarH = targetTopBarH;
        resized();
    }

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
