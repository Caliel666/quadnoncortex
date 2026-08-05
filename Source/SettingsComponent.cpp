#include "SettingsComponent.h"
#include "AppSettings.h"
#include "Theme.h"
#include "UpdateChecker.h"

//==============================================================================
SettingsComponent::MapRow::MapRow (const juce::String& actionName, const juce::String& label,
                                   MidiLearnManager& mgr)
    : action (actionName), learn (mgr)
{
    auto& th = Theme::get();
    nameLabel.setText (label, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId, th.text);
    nameLabel.setFont (juce::FontOptions (17.0f));
    addAndMakeVisible (nameLabel);
    bindingLabel.setColour (juce::Label::textColourId, th.textDim);
    bindingLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (bindingLabel);
    learnBtn.onClick = [this]
    {
        if (learn.isLearning()) learn.cancelLearn();
        else learn.startLearnGlobal (action);
        refresh();
    };
    clearBtn.onClick = [this] { learn.clearGlobal (action); refresh(); };
    th.applyButton (learnBtn);
    th.applyButton (clearBtn, false, true);
    addAndMakeVisible (learnBtn);
    addAndMakeVisible (clearBtn);
    refresh();
}

void SettingsComponent::MapRow::refresh()
{
    auto& th = Theme::get();
    // Only THIS row is in learn mode — not every global map
    const bool learningThis = learn.isLearning()
                              && learn.getLearnGlobalAction() == action;

    if (learningThis)
    {
        bindingLabel.setText ("Move a control...", juce::dontSendNotification);
        learnBtn.setButtonText ("...");
        learnBtn.setColour (juce::TextButton::buttonColourId, th.warning);
    }
    else if (auto* b = learn.findGlobal (action))
    {
        juce::String s = b->ccNumber >= 0
            ? ("Ch" + juce::String (b->midiChannel) + " CC" + juce::String (b->ccNumber))
            : ("Ch" + juce::String (b->midiChannel) + " Note " + juce::String (b->noteNumber));
        bindingLabel.setText (s, juce::dontSendNotification);
        learnBtn.setButtonText ("MIDI");
        learnBtn.setColour (juce::TextButton::buttonColourId, th.success);
    }
    else
    {
        bindingLabel.setText ("Not mapped", juce::dontSendNotification);
        learnBtn.setButtonText ("LEARN");
        learnBtn.setColour (juce::TextButton::buttonColourId, th.surfaceAlt);
    }
}

void SettingsComponent::MapRow::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.setColour (th.card);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2), 12.0f);
}

void SettingsComponent::MapRow::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    clearBtn.setBounds (r.removeFromRight (48));
    r.removeFromRight (6);
    learnBtn.setBounds (r.removeFromRight (100));
    r.removeFromRight (10);
    nameLabel.setBounds (r.removeFromLeft (150));
    bindingLabel.setBounds (r);
}

//==============================================================================
SettingsComponent::SettingsComponent (AudioEngine& engine, MidiLearnManager& learn)
    : audioEngine (engine), midiLearn (learn)
{
    auto& th = Theme::get();

    headerLabel.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, th.text);
    addAndMakeVisible (headerLabel);

    th.applyButton (closeBtn);
    closeBtn.onClick = [this]
    {
        if (onCloseRequested) onCloseRequested();
    };
    addAndMakeVisible (closeBtn);

    for (auto* tab : { &tabAudio, &tabMidi, &tabTheme })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (42);
        addAndMakeVisible (tab);
    }
    tabAudio.setToggleState (true, juce::dontSendNotification);
    tabAudio.onClick = [this] { setTab (0); };
    tabMidi.onClick  = [this] { setTab (1); };
    tabTheme.onClick = [this] { setTab (2); };

    // Full stock selector: channels, ASIO control panel, stereo pairs, etc.
    // MIDI inputs stay on the MIDI tab (showMidiInputOptions = false).
    // min 0 / max 2 → user can choose mono (1) or stereo (2) per bus
    // showChannelsAsStereoPairs = false → individual channel checkboxes (mono OK)
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        audioEngine.getDeviceManager(),
        0, 2,   // input channels
        0, 2,   // output channels
        false,  // MIDI inputs on MIDI tab
        false,  // no MIDI outputs
        false,  // NOT stereo-pairs only — allow mono channel picks
        false);

    deviceSelector->setLookAndFeel (&th.softLaf);
    deviceSelector->setItemHeight (40);

    audioTitle.setText ("Audio Device", juce::dontSendNotification);
    audioTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    audioTitle.setColour (juce::Label::textColourId, th.text);
    audioPage.addAndMakeVisible (audioTitle);

    audioViewport.setViewedComponent (deviceSelector.get(), false);
    audioViewport.setScrollBarsShown (true, false);
    audioPage.addAndMakeVisible (audioViewport);
    addAndMakeVisible (audioPage);

    // ---- MIDI ----
    mapsTitle.setText ("Global MIDI Maps", juce::dontSendNotification);
    mapsTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    mapsTitle.setColour (juce::Label::textColourId, th.text);
    midiPage.addAndMakeVisible (mapsTitle);

    midiInputsTitle.setText ("MIDI Inputs", juce::dontSendNotification);
    midiInputsTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    midiInputsTitle.setColour (juce::Label::textColourId, th.text);
    midiPage.addAndMakeVisible (midiInputsTitle);
    rebuildMidiInputList();

    auto addMap = [this] (const juce::String& a, const juce::String& l)
    {
        auto* row = mapRows.add (new MapRow (a, l, midiLearn));
        midiPage.addAndMakeVisible (row);
    };
    addMap ("presetNext", "Preset next");
    addMap ("presetPrev", "Preset previous");
    addAndMakeVisible (midiPage);
    midiPage.setVisible (false);

    // ---- Theme / General ----
    windowTitle.setText ("Window", juce::dontSendNotification);
    windowTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    windowTitle.setColour (juce::Label::textColourId, th.text);
    themePage.addAndMakeVisible (windowTitle);

    modeLabel.setText ("Mode", juce::dontSendNotification);
    modeLabel.setColour (juce::Label::textColourId, th.textDim);
    modeLabel.setFont (juce::FontOptions (14.0f));
    themePage.addAndMakeVisible (modeLabel);

    modeFullscreen.setClickingTogglesState (true);
    modeWindowed.setClickingTogglesState (true);
    modeFullscreen.setRadioGroupId (91);
    modeWindowed.setRadioGroupId (91);
    th.applyToggleTab (modeFullscreen, true);
    th.applyToggleTab (modeWindowed, false);
    modeFullscreen.onClick = [this]
    {
        AppSettings::get().windowFullscreen = true;
        refreshWindowControls();
    };
    modeWindowed.onClick = [this]
    {
        AppSettings::get().windowFullscreen = false;
        refreshWindowControls();
    };
    themePage.addAndMakeVisible (modeFullscreen);
    themePage.addAndMakeVisible (modeWindowed);

    resLabel.setText ("Fullscreen size", juce::dontSendNotification);
    resLabel.setColour (juce::Label::textColourId, th.textDim);
    resLabel.setFont (juce::FontOptions (14.0f));
    themePage.addAndMakeVisible (resLabel);
    resolutionBox.addItem ("Native (display)", 1);
    resolutionBox.addItem ("1280 x 720", 2);
    resolutionBox.addItem ("1280 x 800", 3);
    resolutionBox.addItem ("1366 x 768", 4);
    resolutionBox.addItem ("1600 x 900", 5);
    resolutionBox.addItem ("1920 x 1080", 6);
    resolutionBox.addItem ("2560 x 1440", 7);
    resolutionBox.setSelectedId (1, juce::dontSendNotification);
    themePage.addAndMakeVisible (resolutionBox);

    winSizeLabel.setText ("Windowed size", juce::dontSendNotification);
    winSizeLabel.setColour (juce::Label::textColourId, th.textDim);
    winSizeLabel.setFont (juce::FontOptions (14.0f));
    themePage.addAndMakeVisible (winSizeLabel);
    windowedSizeBox.addItem ("1024 x 600  (600p)", 1);
    windowedSizeBox.addItem ("1280 x 720", 2);
    windowedSizeBox.addItem ("1280 x 800", 3);
    windowedSizeBox.addItem ("1366 x 768", 4);
    windowedSizeBox.addItem ("1600 x 900", 5);
    windowedSizeBox.addItem ("1920 x 1080", 6);
    windowedSizeBox.setSelectedId (1, juce::dontSendNotification);
    themePage.addAndMakeVisible (windowedSizeBox);

    th.applyButton (applyWindowBtn, true);
    applyWindowBtn.onClick = [this] { applyWindowFromUi(); };
    themePage.addAndMakeVisible (applyWindowBtn);

    themeTitle.setText ("Appearance", juce::dontSendNotification);
    themeTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    themeTitle.setColour (juce::Label::textColourId, th.text);
    themeHint.setText ("Themes live in data/Themes as XML. Dark and Light are built-in.",
                       juce::dontSendNotification);
    themeHint.setColour (juce::Label::textColourId, th.textDim);
    themeHint.setFont (juce::FontOptions (14.0f));
    themePage.addAndMakeVisible (themeTitle);
    themePage.addAndMakeVisible (themeHint);
    themePage.addAndMakeVisible (themeBox);
    th.applyButton (applyThemeBtn, true);
    applyThemeBtn.onClick = [this]
    {
        const auto name = themeBox.getText();
        if (name.isNotEmpty() && Theme::get().load (name))
        {
            AppSettings::get().themeName = name;
            AppSettings::get().save();
            applyThemeColours();
            if (onThemeChanged) onThemeChanged();
            repaint();
        }
    };
    themePage.addAndMakeVisible (applyThemeBtn);

    versionLabel.setText ("Version " + UpdateChecker::currentVersion(),
                          juce::dontSendNotification);
    versionLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    versionLabel.setColour (juce::Label::textColourId, th.text);
    themePage.addAndMakeVisible (versionLabel);

    updateStatus.setText ("", juce::dontSendNotification);
    updateStatus.setFont (juce::FontOptions (14.0f));
    updateStatus.setColour (juce::Label::textColourId, th.textDim);
    themePage.addAndMakeVisible (updateStatus);

    th.applyButton (updateBtn, true);
    updateBtn.onClick = [this] { checkForUpdates(); };
    themePage.addAndMakeVisible (updateBtn);

    addAndMakeVisible (themePage);
    themePage.setVisible (false);
    refreshThemeList();
    refreshWindowControls();

    th.applyButton (saveButton, true);
    saveButton.onClick = [this]
    {
        audioEngine.saveDeviceState();
        AppSettings::get().themeName = Theme::get().currentName;
        // Keep window fields in sync if user changed combos without APPLY WINDOW
        applyWindowFromUi();
        AppSettings::get().save();
        saveButton.setButtonText ("SAVED");
        juce::Timer::callAfterDelay (1200, [this]
        {
            saveButton.setButtonText ("SAVE");
        });
        if (onSaved) onSaved();
    };
    addAndMakeVisible (saveButton);

    th.applyButton (exitButton, false, true);
    exitButton.onClick = [this]
    {
        auto* aw = new juce::AlertWindow ("Exit", "Close quadnoncortex?", juce::AlertWindow::WarningIcon);
        aw->addButton ("Exit", 1);
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [] (int result)
            {
                if (result == 1)
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }), true);
    };
    addAndMakeVisible (exitButton);

    {
        auto prev = midiLearn.onBindingsChanged;
        juce::Component::SafePointer<SettingsComponent> safe (this);
        midiLearn.onBindingsChanged = [safe, prev]
        {
            if (prev) prev();
            if (safe != nullptr)
                safe->refreshMapRows();
        };
    }

    applyThemeColours();
    setSize (720, 720);
}

void SettingsComponent::refreshMapRows()
{
    for (auto* r : mapRows)
        r->refresh();
}

void SettingsComponent::rebuildMidiInputList()
{
    midiInputToggles.clear();
    auto& dm = audioEngine.getDeviceManager();
    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        auto* tb = midiInputToggles.add (new juce::ToggleButton (dev.name));
        bool on = dm.isMidiInputDeviceEnabled (dev.identifier);
        if (AppSettings::get().midiInputsLoaded)
        {
            auto it = AppSettings::get().midiInputEnabled.find (dev.identifier);
            if (it != AppSettings::get().midiInputEnabled.end())
                on = it->second;
        }
        tb->setToggleState (on, juce::dontSendNotification);
        const auto id = dev.identifier;
        tb->onClick = [this, id, tb]
        {
            const bool on = tb->getToggleState();
            auto& dm = audioEngine.getDeviceManager();
            dm.setMidiInputDeviceEnabled (id, on);
            AppSettings::get().setMidiInputEnabled (id, on);
            // Register / unregister callback so enable actually routes MIDI
            if (on)
                dm.addMidiInputDeviceCallback (id, &audioEngine);
            else
                dm.removeMidiInputDeviceCallback (id, &audioEngine);
            // Persist immediately (do not prune missing devices here)
            AppSettings::get().storeAudioDeviceState (dm);
        };
        tb->setColour (juce::ToggleButton::textColourId, Theme::get().text);
        midiPage.addAndMakeVisible (tb);
    }
}

void SettingsComponent::refreshThemeList()
{
    themeBox.clear (juce::dontSendNotification);
    auto names = Theme::get().listThemes();
    int id = 1, selected = 0;
    for (auto& n : names)
    {
        themeBox.addItem (n, id);
        if (n == Theme::get().currentName)
            selected = id;
        ++id;
    }
    if (selected > 0)
        themeBox.setSelectedId (selected, juce::dontSendNotification);
}

void SettingsComponent::applyThemeColours()
{
    auto& th = Theme::get();
    th.applyToLookAndFeel();
    th.applyToggleTab (tabAudio, currentTab == 0);
    th.applyToggleTab (tabMidi,  currentTab == 1);
    th.applyToggleTab (tabTheme, currentTab == 2);
    th.applyButton (saveButton, true);
    th.applyButton (exitButton, false, true);
    th.applyButton (applyThemeBtn, true);
    th.applyButton (closeBtn);
    headerLabel.setColour (juce::Label::textColourId, th.text);
    mapsTitle.setColour (juce::Label::textColourId, th.text);
    midiInputsTitle.setColour (juce::Label::textColourId, th.text);
    themeTitle.setColour (juce::Label::textColourId, th.text);
    themeHint.setColour (juce::Label::textColourId, th.textDim);
    audioTitle.setColour (juce::Label::textColourId, th.text);
    versionLabel.setColour (juce::Label::textColourId, th.text);
    updateStatus.setColour (juce::Label::textColourId, th.textDim);
    th.applyButton (updateBtn, true);
    th.applyButton (applyWindowBtn, true);
    windowTitle.setColour (juce::Label::textColourId, th.text);
    modeLabel.setColour (juce::Label::textColourId, th.textDim);
    resLabel.setColour (juce::Label::textColourId, th.textDim);
    winSizeLabel.setColour (juce::Label::textColourId, th.textDim);
    th.applyToggleTab (modeFullscreen, AppSettings::get().windowFullscreen);
    th.applyToggleTab (modeWindowed, ! AppSettings::get().windowFullscreen);
    for (auto* box : { &resolutionBox, &windowedSizeBox, &themeBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, th.surfaceAlt);
        box->setColour (juce::ComboBox::textColourId, th.text);
        box->setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        box->setColour (juce::ComboBox::arrowColourId, th.textDim);
    }

    themeBox.setColour (juce::ComboBox::backgroundColourId, th.surfaceAlt);
    themeBox.setColour (juce::ComboBox::textColourId, th.text);
    themeBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    themeBox.setColour (juce::ComboBox::arrowColourId, th.textDim);

    if (deviceSelector != nullptr)
    {
        deviceSelector->setLookAndFeel (&th.softLaf);
        deviceSelector->sendLookAndFeelChange();
        deviceSelector->repaint();
    }

    for (auto* r : mapRows) r->refresh();
    for (auto* tb : midiInputToggles)
        tb->setColour (juce::ToggleButton::textColourId, th.text);
}

void SettingsComponent::setTab (int t)
{
    currentTab = t;
    audioPage.setVisible (t == 0);
    midiPage.setVisible (t == 1);
    themePage.setVisible (t == 2);
    tabAudio.setToggleState (t == 0, juce::dontSendNotification);
    tabMidi.setToggleState (t == 1, juce::dontSendNotification);
    tabTheme.setToggleState (t == 2, juce::dontSendNotification);
    applyThemeColours();
    resized();
}


void SettingsComponent::refreshWindowControls()
{
    auto& s = AppSettings::get();
    modeFullscreen.setToggleState (s.windowFullscreen, juce::dontSendNotification);
    modeWindowed.setToggleState (! s.windowFullscreen, juce::dontSendNotification);
    Theme::get().applyToggleTab (modeFullscreen, s.windowFullscreen);
    Theme::get().applyToggleTab (modeWindowed, ! s.windowFullscreen);

    int fsId = 1;
    if (s.fullscreenWidth == 1280 && s.fullscreenHeight == 720) fsId = 2;
    else if (s.fullscreenWidth == 1280 && s.fullscreenHeight == 800) fsId = 3;
    else if (s.fullscreenWidth == 1366 && s.fullscreenHeight == 768) fsId = 4;
    else if (s.fullscreenWidth == 1600 && s.fullscreenHeight == 900) fsId = 5;
    else if (s.fullscreenWidth == 1920 && s.fullscreenHeight == 1080) fsId = 6;
    else if (s.fullscreenWidth == 2560 && s.fullscreenHeight == 1440) fsId = 7;
    resolutionBox.setSelectedId (fsId, juce::dontSendNotification);

    int winId = 1;
    if (s.windowedWidth == 1280 && s.windowedHeight == 720) winId = 2;
    else if (s.windowedWidth == 1280 && s.windowedHeight == 800) winId = 3;
    else if (s.windowedWidth == 1366 && s.windowedHeight == 768) winId = 4;
    else if (s.windowedWidth == 1600 && s.windowedHeight == 900) winId = 5;
    else if (s.windowedWidth == 1920 && s.windowedHeight == 1080) winId = 6;
    windowedSizeBox.setSelectedId (winId, juce::dontSendNotification);

    resolutionBox.setEnabled (s.windowFullscreen);
    windowedSizeBox.setEnabled (! s.windowFullscreen);
}

void applyAppWindowSettings(); // Main.cpp

void SettingsComponent::applyWindowFromUi()
{
    auto& s = AppSettings::get();
    s.windowFullscreen = modeFullscreen.getToggleState();

    switch (resolutionBox.getSelectedId())
    {
        case 2: s.fullscreenWidth = 1280; s.fullscreenHeight = 720; break;
        case 3: s.fullscreenWidth = 1280; s.fullscreenHeight = 800; break;
        case 4: s.fullscreenWidth = 1366; s.fullscreenHeight = 768; break;
        case 5: s.fullscreenWidth = 1600; s.fullscreenHeight = 900; break;
        case 6: s.fullscreenWidth = 1920; s.fullscreenHeight = 1080; break;
        case 7: s.fullscreenWidth = 2560; s.fullscreenHeight = 1440; break;
        default: s.fullscreenWidth = 0; s.fullscreenHeight = 0; break;
    }
    switch (windowedSizeBox.getSelectedId())
    {
        case 2: s.windowedWidth = 1280; s.windowedHeight = 720; break;
        case 3: s.windowedWidth = 1280; s.windowedHeight = 800; break;
        case 4: s.windowedWidth = 1366; s.windowedHeight = 768; break;
        case 5: s.windowedWidth = 1600; s.windowedHeight = 900; break;
        case 6: s.windowedWidth = 1920; s.windowedHeight = 1080; break;
        default: s.windowedWidth = 1024; s.windowedHeight = 600; break;
    }
    s.save();
    applyAppWindowSettings();
    applyWindowBtn.setButtonText ("APPLIED");
    juce::Timer::callAfterDelay (1000, [safe = juce::Component::SafePointer<SettingsComponent> (this)]
    {
        if (safe != nullptr) safe->applyWindowBtn.setButtonText ("APPLY WINDOW");
    });
}

void SettingsComponent::checkForUpdates()
{
    updateBtn.setEnabled (false);
    updateBtn.setButtonText ("CHECKING...");
    updateStatus.setText ("Contacting GitHub...", juce::dontSendNotification);

    juce::Component::SafePointer<SettingsComponent> safe (this);
    juce::Thread::launch ([safe]
    {
        auto result = UpdateChecker::checkForUpdate();
        juce::MessageManager::callAsync ([safe, result]
        {
            if (safe == nullptr) return;
            safe->updateBtn.setEnabled (true);
            safe->updateBtn.setButtonText ("CHECK FOR UPDATES");
            safe->updateStatus.setText (result.message, juce::dontSendNotification);

            if (! result.updateAvailable || result.downloadUrl.isEmpty())
                return;

           #if JUCE_WINDOWS
            auto* aw = new juce::AlertWindow ("Update available",
                result.message + "\n\nDownload and install " + result.latestTag + " now?\nThe app will restart.",
                juce::AlertWindow::QuestionIcon);
            aw->addButton ("Update", 1);
            aw->addButton ("Later", 0);
            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [safe, result] (int choice)
                {
                    if (choice != 1 || safe == nullptr) return;
                    safe->updateStatus.setText ("Downloading...", juce::dontSendNotification);
                    safe->updateBtn.setEnabled (false);
                    juce::Thread::launch ([safe, result]
                    {
                        auto err = UpdateChecker::installWindowsUpdate (result.downloadUrl, result.assetName);
                        juce::MessageManager::callAsync ([safe, err]
                        {
                            if (safe == nullptr) return;
                            if (err.isNotEmpty())
                            {
                                safe->updateStatus.setText (err, juce::dontSendNotification);
                                safe->updateBtn.setEnabled (true);
                                return;
                            }
                            safe->updateStatus.setText ("Installing — restarting...", juce::dontSendNotification);
                            juce::JUCEApplication::getInstance()->systemRequestedQuit();
                        });
                    });
                }), true);
           #else
            safe->updateStatus.setText (result.message + " Open GitHub releases to download.",
                                        juce::dontSendNotification);
           #endif
        });
    });
}

void SettingsComponent::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.background);
    g.setColour (th.textDim.withAlpha (0.15f));
    g.fillRect (16, 56, getWidth() - 32, 1);
}

void SettingsComponent::resized()
{
    auto r = getLocalBounds().reduced (18);
    auto header = r.removeFromTop (52);
    closeBtn.setBounds (header.removeFromRight (110).reduced (2));
    headerLabel.setBounds (header);

    auto bottom = r.removeFromBottom (56);
    exitButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).reduced (3));
    saveButton.setBounds (bottom.reduced (3));
    r.removeFromBottom (12);

    auto tabs = r.removeFromTop (52);
    const int tw = tabs.getWidth() / 3;
    tabAudio.setBounds (tabs.removeFromLeft (tw).reduced (4));
    tabMidi.setBounds (tabs.removeFromLeft (tw).reduced (4));
    tabTheme.setBounds (tabs.reduced (4));
    r.removeFromTop (12);

    if (currentTab == 0)
    {
        audioPage.setBounds (r);
        auto m = audioPage.getLocalBounds();
        audioTitle.setBounds (m.removeFromTop (32));
        m.removeFromTop (8);
        audioViewport.setBounds (m);
        if (deviceSelector != nullptr)
        {
            const int viewH = audioViewport.getHeight();
            const int viewW = audioViewport.getWidth();
            // Short but wide: use full width + denser rows so less scrolling
            const bool shortWide = viewH < 480 && viewW > 700;
            deviceSelector->setItemHeight (shortWide ? 28 : 40);
            const int contentW = shortWide ? juce::jmax (viewW - 8, 600)
                                          : juce::jmin (620, juce::jmax (360, viewW));
            const int contentH = juce::jmax (viewH, shortWide ? 700 : 1100);
            deviceSelector->setBounds (0, 0, contentW, contentH);
        }
    }
    else if (currentTab == 1)
    {
        midiPage.setBounds (r);
        auto m = midiPage.getLocalBounds();
        midiInputsTitle.setBounds (m.removeFromTop (32));
        for (auto* tb : midiInputToggles)
            tb->setBounds (m.removeFromTop (44).reduced (4, 2));
        m.removeFromTop (16);
        mapsTitle.setBounds (m.removeFromTop (32));
        for (auto* row : mapRows)
            row->setBounds (m.removeFromTop (60));
    }
    else
    {
        themePage.setBounds (r);
        auto m = themePage.getLocalBounds();

        // Window controls (top)
        windowTitle.setBounds (m.removeFromTop (28));
        m.removeFromTop (6);
        modeLabel.setBounds (m.removeFromTop (22));
        auto modeRow = m.removeFromTop (48);
        modeFullscreen.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).reduced (4));
        modeWindowed.setBounds (modeRow.reduced (4));
        m.removeFromTop (10);
        resLabel.setBounds (m.removeFromTop (22));
        resolutionBox.setBounds (m.removeFromTop (44).removeFromLeft (juce::jmin (420, m.getWidth())).reduced (0, 2));
        m.removeFromTop (8);
        winSizeLabel.setBounds (m.removeFromTop (22));
        windowedSizeBox.setBounds (m.removeFromTop (44).removeFromLeft (juce::jmin (420, m.getWidth())).reduced (0, 2));
        m.removeFromTop (10);
        applyWindowBtn.setBounds (m.removeFromTop (48).withWidth (200));
        m.removeFromTop (20);

        // Appearance
        themeTitle.setBounds (m.removeFromTop (28));
        themeHint.setBounds (m.removeFromTop (40));
        m.removeFromTop (6);
        auto row = m.removeFromTop (52);
        themeBox.setBounds (row.removeFromLeft (juce::jmin (420, row.getWidth() - 120)).reduced (0, 4));
        applyThemeBtn.setBounds (row.removeFromLeft (100).reduced (8, 4));
        m.removeFromTop (20);
        versionLabel.setBounds (m.removeFromTop (28));
        m.removeFromTop (8);
        updateBtn.setBounds (m.removeFromTop (48).withWidth (220));
        m.removeFromTop (10);
        updateStatus.setBounds (m.removeFromTop (48));
    }
}
