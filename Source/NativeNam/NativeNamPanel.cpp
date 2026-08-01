#include "NativeNamPanel.h"
#include "Theme.h"

//==============================================================================
NativeNamPanel::ControlRow::ControlRow (const juce::String& name, juce::AudioProcessorParameter* param,
                                        MidiLearnManager& mgr, int pluginIdx, int paramIdx,
                                        bool showMidiButtons)
    : parameter (param), learnManager (mgr), pluginIndex (pluginIdx), paramIndex (paramIdx),
      showMidi (showMidiButtons)
{
    auto& th = Theme::get();

    // Name label
    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, th.text);
    addAndMakeVisible (nameLabel);

    // Value label (shows current value)
    valueLabel.setFont (juce::FontOptions (14.0f));
    valueLabel.setColour (juce::Label::textColourId, th.textDim);
    valueLabel.setJustificationType (juce::Justification::centredRight);
    if (param != nullptr)
        valueLabel.setText (param->getText (param->getValue(), 32), juce::dontSendNotification);
    addAndMakeVisible (valueLabel);

    isToggle = (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr);

    if (isToggle)
    {
        toggle.setButtonText (name); // toggle shows its own label
        toggle.setToggleState (param->getValue() >= 0.5f, juce::dontSendNotification);
        toggle.onClick = [this]
        {
            if (parameter != nullptr)
                parameter->setValueNotifyingHost (toggle.getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible (toggle);
        // Hide name/value when using toggle (toggle has its own text)
        nameLabel.setVisible (false);
        valueLabel.setVisible (false);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::thumbColourId, th.accent);
        slider.setColour (juce::Slider::trackColourId, th.accent);
        slider.setColour (juce::Slider::backgroundColourId, th.surfaceAlt);
        if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
        {
            auto r = pf->getNormalisableRange();
            slider.setRange ((double) r.start, (double) r.end, (double) juce::jmax (0.01f, r.interval));
            slider.setValue ((double) pf->get(), juce::dontSendNotification);
            slider.onValueChange = [this]
            {
                if (parameter == nullptr) return;
                *dynamic_cast<juce::AudioParameterFloat*> (parameter) = (float) slider.getValue();
                valueLabel.setText (parameter->getText (parameter->getValue(), 32),
                                   juce::dontSendNotification);
            };
        }
        else
        {
            slider.setRange (0.0, 1.0, 0.01);
            slider.setValue ((double) param->getValue(), juce::dontSendNotification);
            slider.onValueChange = [this]
            {
                if (parameter == nullptr) return;
                parameter->setValueNotifyingHost ((float) slider.getValue());
                valueLabel.setText (parameter->getText (parameter->getValue(), 32),
                                   juce::dontSendNotification);
            };
        }
        addAndMakeVisible (slider);
    }

    // MIDI learn buttons — only added when showMidi is true
    if (showMidi)
    {
        th.applyButton (learnBtn);
        learnBtn.onClick = [this]
        {
            if (learnManager.isLearning()) learnManager.cancelLearn();
            else learnManager.startLearn (pluginIndex, paramIndex);
            updateLearnButton();
        };
        addAndMakeVisible (learnBtn);

        th.applyButton (clearBtn);
        clearBtn.onClick = [this]
        {
            learnManager.clearBinding (pluginIndex, paramIndex);
            updateLearnButton();
        };
        addAndMakeVisible (clearBtn);
    }
    updateLearnButton();
}

void NativeNamPanel::ControlRow::updateLearnButton()
{
    const bool hasMidi = learnManager.findBinding (pluginIndex, paramIndex) != nullptr;
    const bool learningThis = learnManager.isLearning()
                              && learnManager.getLearnPlugin() == pluginIndex
                              && learnManager.getLearnParam() == paramIndex;
    if (learningThis)
    {
        learnBtn.setButtonText ("...");
        learnBtn.setColour (juce::TextButton::buttonColourId, Theme::get().warning);
    }
    else if (hasMidi)
    {
        learnBtn.setButtonText ("MIDI");
        learnBtn.setColour (juce::TextButton::buttonColourId, Theme::get().success);
    }
    else
    {
        learnBtn.setButtonText ("LEARN");
        learnBtn.setColour (juce::TextButton::buttonColourId, Theme::get().surfaceAlt);
    }
    clearBtn.setEnabled (hasMidi);
    clearBtn.setAlpha (hasMidi ? 1.0f : 0.35f);
}

void NativeNamPanel::ControlRow::syncFromParam()
{
    if (parameter == nullptr) return;
    if (isToggle)
        toggle.setToggleState (parameter->getValue() >= 0.5f, juce::dontSendNotification);
    else if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (parameter))
    {
        slider.setValue ((double) pf->get(), juce::dontSendNotification);
        valueLabel.setText (parameter->getText (parameter->getValue(), 32),
                           juce::dontSendNotification);
    }
}

void NativeNamPanel::ControlRow::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    // Subtle card background with rounded corners
    g.setColour (th.surfaceAlt.withAlpha (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f);
}

void NativeNamPanel::ControlRow::resized()
{
    // Match ParameterPanel::ParamRow proportions — tall touch-friendly rows
    auto r = getLocalBounds().reduced (8, 6);

    if (isToggle)
    {
        if (showMidi)
        {
            const int btnH = juce::jmin (44, r.getHeight());
            const int btnY = r.getY() + (r.getHeight() - btnH) / 2;
            clearBtn.setBounds (r.removeFromRight (44).withHeight (btnH).withY (btnY));
            r.removeFromRight (6);
            learnBtn.setBounds (r.removeFromRight (88).withHeight (btnH).withY (btnY));
            r.removeFromRight (8);
            toggle.setBounds (r);
        }
        else
        {
            toggle.setBounds (r);
        }
        return;
    }

    // Name on top-left, value on top-right (same as ParamRow)
    auto topRow = r.removeFromTop (22);
    valueLabel.setBounds (topRow.removeFromRight (90));
    nameLabel.setBounds (topRow);

    r.removeFromTop (4);

    if (showMidi)
    {
        const int btnH = juce::jmin (40, r.getHeight());
        const int btnY = r.getY() + (r.getHeight() - btnH) / 2;
        clearBtn.setBounds (r.removeFromRight (44).withHeight (btnH).withY (btnY));
        r.removeFromRight (6);
        learnBtn.setBounds (r.removeFromRight (88).withHeight (btnH).withY (btnY));
        r.removeFromRight (8);
        slider.setBounds (r);
    }
    else
    {
        slider.setBounds (r);
    }
}

//==============================================================================
NativeNamPanel::NativeNamPanel (NativeNamProcessor& proc, MidiLearnManager& learnMgr, int pluginIdx)
    : juce::AudioProcessorEditor (proc),
      processor (proc), midiLearn (learnMgr), pluginIndex (pluginIdx)
{
    auto& th = Theme::get();

    for (auto* t : { &tabConfig, &tabT3k })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (77);
        addAndMakeVisible (t);
    }
    tabConfig.setToggleState (true, juce::dontSendNotification);
    tabConfig.onClick = [this] { setTab (0); };
    tabT3k.onClick    = [this] { setTab (1); };
    th.applyToggleTab (tabConfig, true);
    th.applyToggleTab (tabT3k, false);

    addAndMakeVisible (configPage);
    addAndMakeVisible (t3kPage);
    t3kPage.setVisible (false);

    auto styleTitle = [&] (juce::Label& l)
    {
        l.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, th.text);
        l.setJustificationType (juce::Justification::centred);
        configPage.addAndMakeVisible (l);
    };
    styleTitle (pedalTitle); styleTitle (ampTitle); styleTitle (cabTitle);

    auto styleName = [&] (juce::Label& l)
    {
        l.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, th.text);
        l.setJustificationType (juce::Justification::centred);
        l.setText ("No model loaded", juce::dontSendNotification);
        l.setMinimumHorizontalScale (0.5f);
        configPage.addAndMakeVisible (l);
    };
    styleName (pedalName); styleName (ampName); styleName (cabName);

    auto styleLoad = [&] (juce::TextButton& b)
    {
        th.applyButton (b, true);
        configPage.addAndMakeVisible (b);
    };
    styleLoad (pedalBtn); styleLoad (ampBtn); styleLoad (cabBtn);
    pedalBtn.onClick = [this] { openLibraryBrowser (NamLibrary::Kind::Pedal); };
    ampBtn.onClick   = [this] { openLibraryBrowser (NamLibrary::Kind::Amp); };
    cabBtn.onClick   = [this] { openLibraryBrowser (NamLibrary::Kind::Cab); };

    for (auto* c : { &pedalClear, &ampClear, &cabClear })
    {
        th.applyButton (*c, false, true);
        configPage.addAndMakeVisible (c);
    }
    pedalClear.onClick = [this] { processor.clearModel (NativeNamProcessor::Slot::Pedal); refreshModelLabels(); };
    ampClear.onClick   = [this] { processor.clearModel (NativeNamProcessor::Slot::Amp);   refreshModelLabels(); };
    cabClear.onClick   = [this] { processor.clearModel (NativeNamProcessor::Slot::Cab);   refreshModelLabels(); };

    // Param indices match add order in NativeNamProcessor ctor:
    // 0 input, 1 output, 2 byp pedal, 3 byp amp, 4 byp cab, 5 lite,
    // 6 pedal mix, 7 amp gain, 8 amp low, 9 amp mid, 10 amp high
    auto makeRow = [this] (const juce::String& name, juce::AudioProcessorParameter* p, int idx,
                          bool midi = false)
    {
        auto row = std::make_unique<ControlRow> (name, p, midiLearn, pluginIndex, idx, midi);
        configPage.addAndMakeVisible (row.get());
        return row;
    };
    bypassPedalRow = makeRow ("Bypass Pedal", processor.bypassPedalParam, 2, true);
    bypassAmpRow   = makeRow ("Bypass Amp",   processor.bypassAmpParam,   3, true);
    bypassCabRow   = makeRow ("Bypass Cab",   processor.bypassCabParam,   4, true);
    pedalMixRow    = makeRow ("Mix",          processor.pedalMixParam,    6);
    ampGainRow     = makeRow ("Gain",         processor.ampGainParam,     7);
    ampLowRow      = makeRow ("Bass",         processor.ampLowParam,      8);
    ampMidRow      = makeRow ("Middle",       processor.ampMidParam,      9);
    ampHighRow     = makeRow ("Treble",       processor.ampHighParam,    10);
    liteRow        = makeRow ("NAM2 Lite",    processor.liteModeParam,    5);
    inGainRow      = makeRow ("Input",        processor.inputGainParam,   0);
    outGainRow     = makeRow ("Output",       processor.outputGainParam,  1);

    // Lite applies slim size
    if (liteRow != nullptr)
    {
        // re-hook toggle to also apply slim
        liteRow->toggle.onClick = [this]
        {
            *processor.liteModeParam = liteRow->toggle.getToggleState();
            processor.applySlimMode();
        };
    }

    // library overlay
    libOverlay.setVisible (false);
    libOverlay.setOpaque (false);
    addChildComponent (libOverlay);
    libTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    libTitle.setColour (juce::Label::textColourId, th.text);
    libOverlay.addAndMakeVisible (libTitle);
    th.applyButton (libClose);
    libClose.onClick = [this] { closeLibraryBrowser(); };
    libOverlay.addAndMakeVisible (libClose);
    libViewport.setViewedComponent (&libContent, false);
    libOverlay.addAndMakeVisible (libViewport);

    // ---- TONE3000 ----
    t3kPage.addAndMakeVisible (t3kStatus);
    t3kStatus.setColour (juce::Label::textColourId, th.text);
    t3kKeyEditor.setTextToShowWhenEmpty ("t3k_pub_… publishable key", th.textDim);
    t3kKeyEditor.setText (Tone3000Client::get().getPublishableKey(), juce::dontSendNotification);
    t3kPage.addAndMakeVisible (t3kKeyEditor);
    th.applyButton (t3kSaveKey, true);
    t3kSaveKey.onClick = [this]
    {
        Tone3000Client::get().setPublishableKey (t3kKeyEditor.getText().trim());
        t3kStatus.setText ("Key saved", juce::dontSendNotification);
    };
    t3kPage.addAndMakeVisible (t3kSaveKey);

    // t3kPasteKeyBtn draws a clipboard vector icon (see ClipboardButton in header)
    t3kPage.addAndMakeVisible (t3kPasteKeyBtn);
    t3kPasteKeyBtn.onClick = [this]
    {
        auto clip = juce::SystemClipboard::getTextFromClipboard().trim();
        if (clip.isNotEmpty())
        {
            t3kKeyEditor.setText (clip, juce::dontSendNotification);
            Tone3000Client::get().setPublishableKey (clip);
            t3kStatus.setText ("Key pasted", juce::dontSendNotification);
        }
    };

    t3kPage.addAndMakeVisible (t3kPasteCodeBtn);
    t3kPasteCodeBtn.onClick = [this]
    {
        auto clip = juce::SystemClipboard::getTextFromClipboard().trim();
        if (clip.isEmpty()) return;
        // Accept full URL or bare code=
        auto code = clip;
        if (code.contains ("code="))
        {
            code = code.fromFirstOccurrenceOf ("code=", false, false);
            code = code.upToFirstOccurrenceOf ("&", false, false);
        }
        t3kCodePaste.setText (code, juce::dontSendNotification);
    };


    th.applyButton (t3kLogin, true);
    t3kLogin.onClick = [this]
    {
        t3kStatus.setText ("Opening browser...", juce::dontSendNotification);
        Tone3000Client::get().beginLogin ([this] (bool ok, juce::String msg)
        {
            t3kStatus.setText (msg, juce::dontSendNotification);
            updateAuthVisibility();
            if (ok) runCurrentSearch();
        });
    };
    t3kPage.addAndMakeVisible (t3kLogin);
    th.applyButton (t3kLogout);
    t3kLogout.onClick = [this]
    {
        Tone3000Client::get().logout();
        t3kStatus.setText ("Logged out", juce::dontSendNotification);
        updateAuthVisibility();
    };
    t3kPage.addAndMakeVisible (t3kLogout);

    t3kCodePaste.setTextToShowWhenEmpty ("Paste code= from browser URL if auto-login fails", th.textDim);
    t3kCodePaste.setVisible (true);
    t3kPage.addAndMakeVisible (t3kCodePaste);
    th.applyButton (t3kCodeSubmit, true);
    t3kCodeSubmit.setVisible (true);
    t3kCodeSubmit.onClick = [this]
    {
        auto code = t3kCodePaste.getText().trim();
        if (code.containsIgnoreCase ("code="))
            code = code.fromFirstOccurrenceOf ("code=", false, false)
                       .upToFirstOccurrenceOf ("&", false, false)
                       .upToFirstOccurrenceOf (" ", false, false);
        if (code.isEmpty())
        {
            t3kStatus.setText ("Paste the code from the browser URL", juce::dontSendNotification);
            return;
        }
        t3kStatus.setText ("Signing in...", juce::dontSendNotification);
        Tone3000Client::get().completeLoginWithCode (code, [this] (bool ok, juce::String msg)
        {
            t3kStatus.setText (msg, juce::dontSendNotification);
            if (ok) t3kCodePaste.clear();
            updateAuthVisibility();
            if (ok) runCurrentSearch();
        });
    };
    t3kPage.addAndMakeVisible (t3kCodeSubmit);

    t3kHint.setText ("Optional: paste code= from the browser URL if auto-login is slow.",
                     juce::dontSendNotification);
    t3kHint.setColour (juce::Label::textColourId, th.textDim);
    t3kHint.setJustificationType (juce::Justification::centredLeft);
    t3kPage.addAndMakeVisible (t3kHint);

    t3kSearch.setReadOnly (true);
    t3kSearch.setTextToShowWhenEmpty ("Search tones...", Theme::get().textDim);
    t3kSearch.addMouseListener (this, false);
    t3kPage.addAndMakeVisible (t3kSearch);
    t3kGear.clear();
    t3kGear.addItem ("All gears", 1);
    t3kGear.addItem ("amp", 2);
    t3kGear.addItem ("pedal", 3);
    t3kGear.addItem ("cab", 4);
    t3kGear.addItem ("amp-cab", 5);
    t3kGear.setSelectedId (2);
    t3kPage.addAndMakeVisible (t3kGear);

    t3kSort.clear();
    t3kSort.addItem ("Trending", 1);
    t3kSort.addItem ("Newest", 2);
    t3kSort.addItem ("Downloads", 3);
    t3kSort.addItem ("Best match", 4);
    t3kSort.setSelectedId (1);
    t3kPage.addAndMakeVisible (t3kSort);

    t3kSource.clear();
    t3kSource.addItem ("Catalog", 1);
    t3kSource.addItem ("Favorites", 2);
    t3kSource.addItem ("My tones", 3);
    t3kSource.setSelectedId (1);
    t3kSource.onChange = [this] { t3kPageIndex = 1; runCurrentSearch(); };
    t3kPage.addAndMakeVisible (t3kSource);

    th.applyButton (t3kSearchBtn, true);
    t3kSearchBtn.onClick = [this] { t3kPageIndex = 1; runCurrentSearch(); };
    t3kPage.addAndMakeVisible (t3kSearchBtn);

    th.applyButton (t3kPrevPage);
    th.applyButton (t3kNextPage);
    t3kPrevPage.onClick = [this] { if (t3kPageIndex > 1) { --t3kPageIndex; runCurrentSearch(); } };
    t3kNextPage.onClick = [this] { if (t3kPageIndex < t3kTotalPages) { ++t3kPageIndex; runCurrentSearch(); } };
    t3kPage.addAndMakeVisible (t3kPrevPage);
    t3kPage.addAndMakeVisible (t3kNextPage);
    t3kPageLabel.setJustificationType (juce::Justification::centred);
    t3kPageLabel.setColour (juce::Label::textColourId, th.textDim);
    t3kPage.addAndMakeVisible (t3kPageLabel);

    t3kViewport.setViewedComponent (&t3kContent, false);
    t3kPage.addAndMakeVisible (t3kViewport);

    if (Tone3000Client::get().isLoggedIn())
        juce::MessageManager::callAsync ([this] { if (isShowing()) runCurrentSearch(); });

    if (Tone3000Client::get().isLoggedIn())
        t3kStatus.setText ("Logged in as " + Tone3000Client::get().getUserDisplayName(),
                           juce::dontSendNotification);
    else
        t3kStatus.setText ("Not connected", juce::dontSendNotification);
    updateAuthVisibility();

    {
        auto prev = midiLearn.onBindingsChanged;
        juce::Component::SafePointer<NativeNamPanel> safe (this);
        midiLearn.onBindingsChanged = [safe, prev]
        {
            if (prev) prev();
            if (safe != nullptr) safe->refreshMidiButtons();
        };
    }

    processor.onModelsChanged = [this] { refreshModelLabels(); };
    refreshModelLabels();
    startTimerHz (8);
    setSize (720, 560);
}

NativeNamPanel::~NativeNamPanel()
{
    stopTimer();
    processor.onModelsChanged = nullptr;
    // Do NOT cancel a global login in progress from panel dtor during preset switch
}

void NativeNamPanel::refreshMidiButtons()
{
    for (auto* r : { bypassPedalRow.get(), bypassAmpRow.get(), bypassCabRow.get(),
                     pedalMixRow.get(), ampGainRow.get(), ampLowRow.get(), ampMidRow.get(),
                     ampHighRow.get(), liteRow.get(), inGainRow.get(), outGainRow.get() })
        if (r != nullptr) r->updateLearnButton();
}

void NativeNamPanel::applyTheme()
{
    auto& th = Theme::get();
    th.applyToggleTab (tabConfig, currentTab == 0);
    th.applyToggleTab (tabT3k, currentTab == 1);
    for (auto* l : { &pedalTitle, &ampTitle, &cabTitle })
        l->setColour (juce::Label::textColourId, th.text);
    for (auto* l : { &pedalName, &ampName, &cabName })
    {
        l->setColour (juce::Label::textColourId, th.text);
        l->setFont (juce::FontOptions (18.0f, juce::Font::bold));
    }
    for (auto* b : { &pedalBtn, &ampBtn, &cabBtn })
        th.applyButton (*b, true);
    for (auto* b : { &pedalClear, &ampClear, &cabClear })
        th.applyButton (*b, false, true);
    th.applyButton (libClose);
    th.applyButton (t3kSaveKey, true);
    th.applyButton (t3kLogin, true);
    th.applyButton (t3kLogout);
    th.applyButton (t3kCodeSubmit, true);
    th.applyButton (t3kSearchBtn, true);
    t3kStatus.setColour (juce::Label::textColourId, th.text);
    t3kHint.setColour (juce::Label::textColourId, th.textDim);
    libTitle.setColour (juce::Label::textColourId, th.text);
    t3kKeyEditor.setColour (juce::TextEditor::backgroundColourId, th.surfaceAlt);
    t3kKeyEditor.setColour (juce::TextEditor::textColourId, th.text);
    t3kKeyEditor.setColour (juce::TextEditor::outlineColourId, th.surfaceAlt.brighter (0.15f));
    t3kSearch.setColour (juce::TextEditor::backgroundColourId, th.surfaceAlt);
    t3kSearch.setColour (juce::TextEditor::textColourId, th.text);
    t3kCodePaste.setColour (juce::TextEditor::backgroundColourId, th.surfaceAlt);
    t3kCodePaste.setColour (juce::TextEditor::textColourId, th.text);
    for (auto* r : { bypassPedalRow.get(), bypassAmpRow.get(), bypassCabRow.get(),
                     pedalMixRow.get(), ampGainRow.get(), ampLowRow.get(), ampMidRow.get(),
                     ampHighRow.get(), liteRow.get(), inGainRow.get(), outGainRow.get() })
    {
        if (r == nullptr) continue;
        r->nameLabel.setColour (juce::Label::textColourId, th.text);
        r->valueLabel.setColour (juce::Label::textColourId, th.textDim);
        th.applyButton (r->learnBtn);
        th.applyButton (r->clearBtn);
        r->updateLearnButton();
    }
    repaint();
}

void NativeNamPanel::timerCallback()
{
    // Theme only when not interacting with overlays (avoids flicker)
    static int tick = 0;
    if ((++tick % 15) == 0 && ! libOverlay.isVisible())
        applyTheme();
    for (auto* r : { bypassPedalRow.get(), bypassAmpRow.get(), bypassCabRow.get(),
                     pedalMixRow.get(), ampGainRow.get(), ampLowRow.get(), ampMidRow.get(),
                     ampHighRow.get(), liteRow.get(), inGainRow.get(), outGainRow.get() })
        if (r != nullptr) r->syncFromParam();
    if ((tick % 8) == 0)
        refreshMidiButtons();
}

void NativeNamPanel::setTab (int t)
{
    currentTab = t;
    configPage.setVisible (t == 0);
    t3kPage.setVisible (t == 1);
    Theme::get().applyToggleTab (tabConfig, t == 0);
    Theme::get().applyToggleTab (tabT3k, t == 1);
    resized();
}

void NativeNamPanel::refreshModelLabels()
{
    auto set = [] (juce::Label& l, const juce::String& n)
    {
        l.setText (n.isEmpty() ? "No model loaded" : n, juce::dontSendNotification);
    };
    set (pedalName, processor.getModelName (NativeNamProcessor::Slot::Pedal));
    set (ampName,   processor.getModelName (NativeNamProcessor::Slot::Amp));
    set (cabName,   processor.getModelName (NativeNamProcessor::Slot::Cab));
}

void NativeNamPanel::openLibraryBrowser (NamLibrary::Kind kind)
{
    browsingKind = kind;
    const char* titles[] = { "Select Pedal", "Select Amp", "Select Cab IR" };
    libTitle.setText (titles[(int) kind], juce::dontSendNotification);
    // Hide main pages so they cannot draw through the popup
    configPage.setVisible (false);
    t3kPage.setVisible (false);
    rebuildLibraryList (kind);
    libOverlay.setVisible (true);
    libOverlay.toFront (true);
    resized();
    repaint();
}

void NativeNamPanel::closeLibraryBrowser()
{
    libOverlay.setVisible (false);
    configPage.setVisible (currentTab == 0);
    t3kPage.setVisible (currentTab == 1);
    resized();
    repaint();
}

void NativeNamPanel::rebuildLibraryList (NamLibrary::Kind kind)
{
    libRows.clear();
    libContent.removeAllChildren();
    auto entries = NamLibrary::scan (kind);
    int y = 0;
    const int rowW = 440;
    if (entries.isEmpty())
    {
        libTitle.setText (juce::String (libTitle.getText()) + "  (empty - put files in data/NAM/"
                          + NamLibrary::folderLabel (kind) + ")",
                          juce::dontSendNotification);
    }
    for (auto& e : entries)
    {
        auto* b = libRows.add (new juce::TextButton (e.name));
        Theme::get().applyButton (*b);
        b->setBounds (4, y, rowW, 44);
        libContent.addAndMakeVisible (b);
        auto entry = e;
        b->onClick = [this, entry] { selectLibraryEntry (entry); };
        y += 50;
    }
    libContent.setSize (rowW + 8, juce::jmax (y, 80));
}

void NativeNamPanel::selectLibraryEntry (const NamLibrary::Entry& e)
{
    NativeNamProcessor::Slot slot = NativeNamProcessor::Slot::Amp;
    if (browsingKind == NamLibrary::Kind::Pedal) slot = NativeNamProcessor::Slot::Pedal;
    if (browsingKind == NamLibrary::Kind::Cab)   slot = NativeNamProcessor::Slot::Cab;
    if (! processor.loadModel (slot, e.file))
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Load Failed",
                                                processor.getLastError());
    refreshModelLabels();
    closeLibraryBrowser();
}

NamLibrary::Kind NativeNamPanel::kindFromGear (const juce::String& gear, const juce::String& format)
{
    return NamLibrary::kindFromGear (gear, format);
}

void NativeNamPanel::runCurrentSearch()
{
    const auto q = t3kSearch.getText().trim();
    juce::String gear = t3kGear.getText();
    if (gear == "All gears") gear = {};
    juce::String sort = "trending";
    switch (t3kSort.getSelectedId())
    {
        case 2: sort = "newest"; break;
        case 3: sort = "downloads-all-time"; break;
        case 4: sort = "best-match"; break;
        default: sort = "trending"; break;
    }
    t3kStatus.setText ("Loading...", juce::dontSendNotification);
    auto done = [this] (Tone3000Client::SearchResult result) { showSearchResults (result); };

    const int src = t3kSource.getSelectedId();
    if (src == 2) // Favorites
        Tone3000Client::get().listFavoritedTones (t3kPageIndex, 10, done);
    else if (src == 3) // My tones
        Tone3000Client::get().listCreatedTones (t3kPageIndex, 10, done);
    else
        Tone3000Client::get().searchTones (q, gear, sort, t3kPageIndex, 10, 2, done);
}

void NativeNamPanel::doSearch()
{
    t3kPageIndex = 1;
    runCurrentSearch();
}

void NativeNamPanel::showSearchResults (const Tone3000Client::SearchResult& result)
{
    lastTones = result.tones;
    t3kRows.clear();
    t3kContent.removeAllChildren();
    if (result.error.isNotEmpty())
    {
        t3kStatus.setText (result.error, juce::dontSendNotification);
        t3kPageLabel.setText ({}, juce::dontSendNotification);
        return;
    }
    t3kTotalPages = juce::jmax (1, result.totalPages);
    t3kPageIndex = juce::jmax (1, result.page);
    t3kStatus.setText (juce::String (result.total) + " tones", juce::dontSendNotification);
    t3kPageLabel.setText ("Page " + juce::String (t3kPageIndex) + " / " + juce::String (t3kTotalPages),
                          juce::dontSendNotification);
    t3kPrevPage.setEnabled (t3kPageIndex > 1);
    t3kNextPage.setEnabled (t3kPageIndex < t3kTotalPages);

    int y = 0;
    const int rowW = juce::jmax (320, t3kViewport.getWidth() - 8);
    for (int i = 0; i < lastTones.size(); ++i)
    {
        auto& ti = lastTones.getReference (i);
        juce::String label = ti.name.isNotEmpty() ? ti.name : "(untitled)";
        if (ti.userName.isNotEmpty())
            label += " - " + ti.userName;
        if (ti.gears.isNotEmpty())
            label += " [" + ti.gears + "]";

        auto* b = t3kRows.add (new juce::TextButton (label));
        Theme::get().applyButton (*b);
        b->setBounds (0, y, rowW, 44);
        t3kContent.addAndMakeVisible (b);

        const int id = ti.id;
        juce::String gears = ti.gears;
        if (gears.isEmpty())
        {
            gears = t3kGear.getText();
            if (gears == "All gears") gears = {};
        }
        const juce::String fmt = ti.format;
        b->onClick = [this, id, gears, fmt]
        {
            t3kStatus.setText ("Fetching models...", juce::dontSendNotification);
            Tone3000Client::get().listModels (id, 2,
                [this, gears, fmt] (juce::Array<Tone3000Client::ModelInfo> models, juce::String err)
            {
                if (err.isNotEmpty() || models.isEmpty())
                {
                    t3kStatus.setText (err.isNotEmpty() ? err : "No models", juce::dontSendNotification);
                    return;
                }
                auto m = models.getFirst();
                const juce::String modelFmt = m.format.isNotEmpty() ? m.format : fmt;
                t3kStatus.setText ("Downloading " + m.name + "...", juce::dontSendNotification);
                Tone3000Client::get().downloadModel (m, [this, gears, modelFmt] (juce::File f, juce::String e2)
                {
                    if (e2.isNotEmpty())
                    {
                        t3kStatus.setText (e2, juce::dontSendNotification);
                        return;
                    }
                    const auto kind = NamLibrary::kindFromGear (gears, modelFmt);
                    auto dest = NamLibrary::importFile (f, kind);
                    if (! dest.existsAsFile())
                    {
                        t3kStatus.setText ("Failed to save into " + juce::String (NamLibrary::folderLabel (kind)),
                                           juce::dontSendNotification);
                        return;
                    }
                    // Remove temp Downloads copy if different
                    if (f.getFullPathName() != dest.getFullPathName())
                        f.deleteFile();
                    t3kStatus.setText (juce::String ("Saved to ") + NamLibrary::folderLabel (kind)
                                       + ": " + dest.getFileName(), juce::dontSendNotification);
                });
            });
        };
        y += 48;
    }
    t3kContent.setSize (rowW, juce::jmax (y, 80));
}

void NativeNamPanel::downloadTone (int toneId, const juce::String& gears)
{
    t3kStatus.setText ("Fetching models...", juce::dontSendNotification);
    Tone3000Client::get().listModels (toneId, 2, [this, gears] (juce::Array<Tone3000Client::ModelInfo> models, juce::String err)
    {
        if (err.isNotEmpty() || models.isEmpty())
        {
            t3kStatus.setText (err.isNotEmpty() ? err : "No models", juce::dontSendNotification);
            return;
        }
        auto m = models.getFirst();
        Tone3000Client::get().downloadModel (m, [this, gears] (juce::File f, juce::String e2)
        {
            if (e2.isNotEmpty()) { t3kStatus.setText (e2, juce::dontSendNotification); return; }
            const auto kind = NamLibrary::kindFromGear (gears, {});
            auto dest = NamLibrary::importFile (f, kind);
            if (f.getFullPathName() != dest.getFullPathName())
                f.deleteFile();
            t3kStatus.setText (juce::String ("Saved to ") + NamLibrary::folderLabel (kind)
                               + ": " + dest.getFileName(), juce::dontSendNotification);
        });
    });
}

void NativeNamPanel::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.surface);

    if (libOverlay.isVisible())
    {
        // Full dim (config/t3k are hidden)
        g.setColour (th.surface.darker (0.25f));
        g.fillRect (getLocalBounds());

        const int pw = juce::jmin (480, getWidth() - 48);
        const int ph = juce::jmin (400, getHeight() - 48);
        auto card = juce::Rectangle<float> ((float) (getWidth() - pw) * 0.5f,
                                            (float) (getHeight() - ph) * 0.5f,
                                            (float) pw, (float) ph);
        g.setColour (th.card);
        g.fillRoundedRectangle (card, 14.0f);
        g.setColour (th.surfaceAlt.brighter (0.1f));
        g.drawRoundedRectangle (card, 14.0f, 1.5f);
    }
}

void NativeNamPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto tabs = r.removeFromTop (40);
    tabConfig.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (4));
    tabT3k.setBounds (tabs.reduced (4));
    r.removeFromTop (6);

    if (currentTab == 0)
    {
        configPage.setBounds (r);
        auto m = configPage.getLocalBounds().reduced (4);

        // Three columns for slots
        const int colW = m.getWidth() / 3;
        auto pedalCol = m.removeFromLeft (colW).reduced (4);
        auto ampCol   = m.removeFromLeft (colW).reduced (4);
        auto cabCol   = m.reduced (4);

        auto layoutSlotHeader = [] (juce::Rectangle<int>& c, juce::Label& title, juce::Label& name,
                                    juce::TextButton& load, juce::TextButton& clear)
        {
            title.setBounds (c.removeFromTop (20));
            name.setBounds (c.removeFromTop (40));
            auto row = c.removeFromTop (40);
            clear.setBounds (row.removeFromRight (40).reduced (2));
            load.setBounds (row.reduced (2));
            c.removeFromTop (4);
        };
        layoutSlotHeader (pedalCol, pedalTitle, pedalName, pedalBtn, pedalClear);
        layoutSlotHeader (ampCol,   ampTitle,   ampName,   ampBtn,   ampClear);
        layoutSlotHeader (cabCol,   cabTitle,   cabName,   cabBtn,   cabClear);

        auto placeRow = [] (juce::Rectangle<int>& c, ControlRow* row)
        {
            if (row != nullptr)
            {
                // Match ParameterPanel row height (kRowHeight = 96) for touch
                const int h = row->isToggle ? 56 : 88;
                row->setBounds (c.removeFromTop (h).reduced (0, 3));
            }
        };

        // Under each column
        placeRow (pedalCol, bypassPedalRow.get());
        placeRow (pedalCol, pedalMixRow.get());

        placeRow (ampCol, bypassAmpRow.get());
        placeRow (ampCol, ampGainRow.get());
        placeRow (ampCol, ampLowRow.get());
        placeRow (ampCol, ampMidRow.get());
        placeRow (ampCol, ampHighRow.get());

        placeRow (cabCol, bypassCabRow.get());

        // Global settings below the columns (tall rows)
        auto global = configPage.getLocalBounds().removeFromBottom (280).reduced (8, 4);
        placeRow (global, liteRow.get());
        placeRow (global, inGainRow.get());
        placeRow (global, outGainRow.get());
    }
    else
    {
        t3kPage.setBounds (r);
        auto m = t3kPage.getLocalBounds().reduced (6);
        t3kStatus.setBounds (m.removeFromTop (28));
        auto keyRow = m.removeFromTop (36);
        t3kSaveKey.setBounds (keyRow.removeFromRight (80).reduced (2));
        t3kPasteKeyBtn.setBounds (keyRow.removeFromRight (36).reduced (2));
        t3kKeyEditor.setBounds (keyRow.reduced (2));
        auto auth = m.removeFromTop (40);
        t3kLogout.setBounds (auth.removeFromRight (100).reduced (2));
        t3kLogin.setBounds (auth.removeFromRight (120).reduced (2));
        auto codeRow = m.removeFromTop (36);
        t3kCodeSubmit.setBounds (codeRow.removeFromRight (90).reduced (2));
        t3kPasteCodeBtn.setBounds (codeRow.removeFromRight (36).reduced (2));
        t3kCodePaste.setBounds (codeRow.reduced (2));
        t3kHint.setBounds (m.removeFromTop (20));
        auto search = m.removeFromTop (40);
        t3kSearchBtn.setBounds (search.removeFromRight (90).reduced (2));
        t3kSource.setBounds (search.removeFromRight (110).reduced (2));
        t3kSort.setBounds (search.removeFromRight (120).reduced (2));
        t3kGear.setBounds (search.removeFromRight (100).reduced (2));
        t3kSearch.setBounds (search.reduced (2));
        auto pager = m.removeFromBottom (36);
        t3kNextPage.setBounds (pager.removeFromRight (44).reduced (2));
        t3kPrevPage.setBounds (pager.removeFromRight (44).reduced (2));
        t3kPageLabel.setBounds (pager);
        t3kViewport.setBounds (m);
    }

    if (kbOverlay != nullptr)
    {
        if (auto* parent = kbOverlay->getParentComponent())
            kbOverlay->setBounds (parent->getLocalBounds());
        else
            kbOverlay->setBounds (getLocalBounds());
    }

    if (libOverlay.isVisible())
    {
        libOverlay.setBounds (getLocalBounds());
        const int pw = juce::jmin (480, getWidth() - 48);
        const int ph = juce::jmin (400, getHeight() - 48);
        auto card = juce::Rectangle<int> ((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
        auto top = card.removeFromTop (48).reduced (10, 6);
        libClose.setBounds (top.removeFromRight (90).reduced (2));
        libTitle.setBounds (top);
        libViewport.setBounds (card.reduced (14, 10));
        const int rw = juce::jmax (200, libViewport.getWidth() - 8);
        for (auto* b : libRows)
            b->setSize (rw, b->getHeight());
        libContent.setSize (rw + 8, libContent.getHeight());
    }
}


int NativeNamPanel::getPreferredHeight() const
{
    // tabs + config/t3k content with tall ParameterPanel-style rows
    const int tabH = 46;
    if (currentTab == 0)
    {
        // Slot headers ~104; amp column has most controls (1 toggle + 4 sliders)
        // toggles 56, sliders 88
        const int ampCol = 56 + 4 * 88; // ~408
        const int global = 56 + 2 * 88; // lite + in + out
        return tabH + 8 + 104 + ampCol + global + 24;
    }
    else
    {
        return tabH + 8 + 28 + 40 + 40 + 40 + 24 + 40 + 36 + 200;
    }
}

void NativeNamPanel::updateAuthVisibility()
{
    const bool loggedIn = Tone3000Client::get().isLoggedIn();
    t3kHint.setVisible (! loggedIn);
    t3kCodePaste.setVisible (! loggedIn);
    t3kCodeSubmit.setVisible (! loggedIn);
    t3kPasteKeyBtn.setVisible (! loggedIn);
    t3kPasteCodeBtn.setVisible (! loggedIn);
    t3kLogin.setVisible (! loggedIn);
    t3kLogout.setVisible (loggedIn);
    // Key can stay visible for re-entry
    resized();
}

void NativeNamPanel::openSearchKeyboard()
{
    if (kbOverlay != nullptr)
        return;
    auto* ov = new KeyboardOverlay();
    ov->setInitial (t3kSearch.getText());
    ov->onDone = [this] (juce::String text) { closeSearchKeyboard (text); };

    // Place the overlay on the desktop (top-level window) so it covers the whole screen,
    // not just the NativeNamPanel/ParameterPanel bounds.
    if (auto* parent = getTopLevelComponent())
    {
        parent->addChildComponent (ov);
        ov->setBounds (parent->getLocalBounds());
        parent->addAndMakeVisible (ov);
        ov->toFront (true);
    }
    else
    {
        // Fallback: add as child of this panel
        ov->setBounds (getLocalBounds());
        addAndMakeVisible (ov);
        ov->toFront (true);
    }
    kbOverlay.reset (ov);
}

void NativeNamPanel::closeSearchKeyboard (const juce::String& text)
{
    if (kbOverlay != nullptr)
    {
        // If we added it to a top-level parent, remove from that parent
        if (auto* parent = kbOverlay->getParentComponent())
            parent->removeChildComponent (kbOverlay.get());
    }
    t3kSearch.setText (text, juce::dontSendNotification);
    kbOverlay.reset();
    t3kPageIndex = 1;
    runCurrentSearch();
}


void NativeNamPanel::mouseDown (const juce::MouseEvent& e)
{
    if (currentTab != 1 || kbOverlay != nullptr)
        return;
    // Click on the search field (or its area on the T3K page)
    if (e.eventComponent == &t3kSearch
        || (e.eventComponent == this && t3kSearch.getScreenBounds().contains (e.getScreenPosition()))
        || (e.eventComponent == &t3kPage && t3kSearch.getBounds().contains (e.getEventRelativeTo (&t3kPage).getPosition())))
    {
        openSearchKeyboard();
    }
}
