#include "NativeNamPanel.h"
#include "Theme.h"



//==============================================================================
NativeNamPanel::ControlRow::ControlRow (const juce::String& name, juce::AudioProcessorParameter* param,
                                        MidiLearnManager& mgr, int pluginIdx, int paramIdx,
                                        std::function<void(ControlRow*)> onMidi,
                                        std::function<void(ControlRow*)> onValue)
    : parameter (param), learnManager (mgr), pluginIndex (pluginIdx), paramIndex (paramIdx),
      openMidi (std::move (onMidi)), openValue (std::move (onValue))
{
    auto& th = Theme::get();
    isToggle = (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr);
    if (param != nullptr)
        defaultNorm = param->getDefaultValue();

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, th.text);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    valueLabel.setColour (juce::Label::textColourId, th.accent);
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    resetBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xba")); // ↺
    resetBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    resetBtn.setColour (juce::TextButton::textColourOffId, th.textDim);
    resetBtn.onClick = [this]
    {
        if (parameter == nullptr) return;
        parameter->setValueNotifyingHost (defaultNorm);
        syncFromParam();
    };
    addAndMakeVisible (resetBtn);

    if (isToggle)
    {
        switchToggle.onChange = [this] (bool on)
        {
            if (parameter == nullptr) return;
            parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
            syncFromParam();
        };
        addAndMakeVisible (switchToggle);
        knob.setVisible (false);
        resetBtn.setVisible (false);
    }
    else
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        knob.setRange (0.0, 1.0, 0.0);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
        knob.setColour (juce::Slider::rotarySliderFillColourId, th.accent);
        knob.setColour (juce::Slider::rotarySliderOutlineColourId, th.surfaceAlt);
        knob.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        knob.addListener (this);
        addAndMakeVisible (knob);
        switchToggle.setVisible (false);
    }
    syncFromParam();
}

void NativeNamPanel::ControlRow::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (th.surface.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 14.0f);
}

void NativeNamPanel::ControlRow::resized()
{
    // Match ParameterPanel cells: name top, control centre, value left / reset right
    auto r = getLocalBounds().reduced (8, 6);
    nameLabel.setBounds (r.removeFromTop (22));
    auto bot = r.removeFromBottom (26);
    if (! isToggle)
        resetBtn.setBounds (bot.removeFromRight (26));
    valueLabel.setBounds (bot); // left-aligned under dial
    valueLabel.setJustificationType (juce::Justification::centredLeft);
    // Keep dial / switch centred in remaining area (aligned under name)
    if (isToggle)
        switchToggle.setBounds (r.withSizeKeepingCentre (56, 32));
    else
        knob.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) - 4,
                                                 juce::jmin (r.getWidth(), r.getHeight()) - 4));
}

void NativeNamPanel::ControlRow::sliderValueChanged (juce::Slider*)
{
    if (parameter == nullptr) return;
    parameter->setValueNotifyingHost ((float) knob.getValue());
    valueLabel.setText (parameter->getText ((float) knob.getValue(), 24), juce::dontSendNotification);
    updateLearnButton();
}

void NativeNamPanel::ControlRow::syncFromParam()
{
    if (parameter == nullptr) return;
    const float v = parameter->getValue();
    if (isToggle)
        switchToggle.setOn (v >= 0.5f, juce::dontSendNotification);
    else
        knob.setValue ((double) v, juce::dontSendNotification);
    valueLabel.setText (parameter->getText (v, 24), juce::dontSendNotification);
    updateLearnButton();
}

void NativeNamPanel::ControlRow::updateLearnButton()
{
    auto& th = Theme::get();
    const bool has = learnManager.findBinding (pluginIndex, paramIndex) != nullptr;
    const bool learning = learnManager.isLearning()
                          && learnManager.getLearnPlugin() == pluginIndex
                          && learnManager.getLearnParam() == paramIndex;
    if (learning)
        nameLabel.setColour (juce::Label::textColourId, th.warning);
    else if (has)
        nameLabel.setColour (juce::Label::textColourId, th.success);
    else
        nameLabel.setColour (juce::Label::textColourId, th.text);
}

void NativeNamPanel::ControlRow::mouseDown (const juce::MouseEvent& e)
{
    if (nameLabel.getBounds().contains (e.getPosition()))
    {
        if (openMidi) openMidi (this);
        return;
    }
    if (valueLabel.getBounds().contains (e.getPosition()) && ! isToggle)
    {
        if (openValue) openValue (this);
        return;
    }
}

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

    configViewport.setViewedComponent (&configPage, false);
    configViewport.setScrollBarsShown (true, true);
    configViewport.setScrollBarThickness (22);
    addAndMakeVisible (configViewport);
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
                           bool = false)
    {
        auto row = std::make_unique<ControlRow> (
            name, p, midiLearn, pluginIndex, idx,
            [this] (ControlRow* c) { openMidiFor (c); },
            [this] (ControlRow* c) { openValueFor (c); });
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

    // Lite applies slim size when the switch changes
    if (liteRow != nullptr)
    {
        liteRow->switchToggle.onChange = [this] (bool on)
        {
            if (processor.liteModeParam != nullptr)
                processor.liteModeParam->setValueNotifyingHost (on ? 1.0f : 0.0f);
            processor.applySlimMode();
            if (liteRow != nullptr)
                liteRow->syncFromParam();
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
        r->valueLabel.setColour (juce::Label::textColourId, th.accent);
        r->knob.setColour (juce::Slider::rotarySliderFillColourId, th.accent);
        r->knob.setColour (juce::Slider::rotarySliderOutlineColourId, th.surfaceAlt);
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

    // Relayout when section bypass toggles (collapse / expand controls)
    static int lastBypassBits = -1;
    const int bits = ((processor.bypassPedalParam && processor.bypassPedalParam->get() >= 0.5f) ? 1 : 0)
                   | ((processor.bypassAmpParam   && processor.bypassAmpParam->get()   >= 0.5f) ? 2 : 0)
                   | ((processor.bypassCabParam   && processor.bypassCabParam->get()   >= 0.5f) ? 4 : 0);
    if (bits != lastBypassBits)
    {
        lastBypassBits = bits;
        resized();
    }

    if ((tick % 8) == 0)
        refreshMidiButtons();
}

void NativeNamPanel::setTab (int t)
{
    currentTab = t;
    configViewport.setVisible (t == 0);
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
    configViewport.setVisible (false);
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
    configViewport.setVisible (currentTab == 0);
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

    // Vertical column dividers aligned with PEDAL | AMP | CAB headers
    if (currentTab == 0 && ! libOverlay.isVisible() && configViewport.isVisible())
    {
        g.setColour (th.text.withAlpha (0.12f));
        const auto vb = configViewport.getBounds();
        const int scrollX = configViewport.getViewPositionX();
        const int scrollY = configViewport.getViewPositionY();
        const int top = vb.getY() + 4;
        // End lines above the global row (content Y → panel Y)
        int bot = vb.getBottom() - 4;
        if (dividerBottomY > 0)
            bot = juce::jmin (bot, vb.getY() + dividerBottomY - scrollY);
        if (bot > top)
        {
            for (auto dx : dividerXs)
            {
                const int x = vb.getX() + dx - scrollX;
                if (x > vb.getX() + 2 && x < vb.getRight() - 2)
                    g.drawLine ((float) x, (float) top, (float) x, (float) bot, 1.0f);
            }
        }
    }

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
        configViewport.setBounds (r);
        configViewport.setVisible (true);
        t3kPage.setVisible (false);

        const int viewW = juce::jmax (360, configViewport.getWidth() - 24);
        const int colW  = juce::jmax (1, viewW / 3);
        constexpr int cellW = 120;
        constexpr int cellH = 140;
        constexpr int gap = 4;
        constexpr int headerH = 96;
        constexpr int pad = 6;

        const bool pedalOn = processor.bypassPedalParam == nullptr
                             || processor.bypassPedalParam->get() < 0.5f;
        const bool ampOn   = processor.bypassAmpParam == nullptr
                             || processor.bypassAmpParam->get() < 0.5f;

        // Fixed header columns — never move with toggles
        {
            auto header = juce::Rectangle<int> (0, 0, viewW, headerH);
            auto layoutSlotHeader = [] (juce::Rectangle<int> c, juce::Label& title, juce::Label& name,
                                        juce::TextButton& load, juce::TextButton& clear)
            {
                title.setBounds (c.removeFromTop (18));
                name.setBounds (c.removeFromTop (22));
                auto row = c.removeFromTop (36);
                clear.setBounds (row.removeFromRight (36).reduced (2));
                load.setBounds (row.reduced (2, 2));
            };
            layoutSlotHeader (header.removeFromLeft (colW).reduced (4, 0),
                              pedalTitle, pedalName, pedalBtn, pedalClear);
            layoutSlotHeader (header.removeFromLeft (colW).reduced (4, 0),
                              ampTitle, ampName, ampBtn, ampClear);
            layoutSlotHeader (header.reduced (4, 0),
                              cabTitle, cabName, cabBtn, cabClear);
        }

        // Vertical dividers locked to column edges (under headers)
        dividerXs.clear();
        dividerXs.add (colW);
        dividerXs.add (colW * 2);

        // Place controls horizontally *within* a fixed column, wrapping inside it
        auto placeInColumn = [&] (int col, const std::vector<std::pair<ControlRow*, bool>>& items) -> int
        {
            const int colX = col * colW + pad;
            const int colRight = (col + 1) * colW - pad;
            int x = colX;
            int y = headerH + 8;
            int maxY = y;
            for (auto& pr : items)
            {
                auto* row = pr.first;
                if (row == nullptr) continue;
                row->setVisible (pr.second);
                if (! pr.second) continue;
                if (x + cellW > colRight && x > colX)
                {
                    x = colX;
                    y += cellH + gap;
                }
                row->setBounds (x, y, cellW, cellH);
                x += cellW + gap;
                maxY = juce::jmax (maxY, y + cellH);
            }
            return maxY;
        };

        // Hide everything first so removed toggles don't leave ghosts
        for (auto* row : { bypassPedalRow.get(), pedalMixRow.get(),
                           bypassAmpRow.get(), ampGainRow.get(), ampLowRow.get(),
                           ampMidRow.get(), ampHighRow.get(), bypassCabRow.get(),
                           liteRow.get(), inGainRow.get(), outGainRow.get() })
            if (row != nullptr) row->setVisible (false);

        // PEDAL column (under Load Pedal)
        const int y0 = placeInColumn (0, {
            { bypassPedalRow.get(), true },
            { pedalMixRow.get(),    pedalOn },
        });
        // AMP column (under Load Amp)
        const int y1 = placeInColumn (1, {
            { bypassAmpRow.get(), true },
            { ampGainRow.get(),   ampOn },
            { ampLowRow.get(),    ampOn },
            { ampMidRow.get(),    ampOn },
            { ampHighRow.get(),   ampOn },
        });
        // CAB column (under Load Cab)
        const int y2 = placeInColumn (2, {
            { bypassCabRow.get(), true },
        });

        // Stop vertical dividers above the global row
        dividerBottomY = juce::jmax (y0, juce::jmax (y1, y2)) + 8;

        // GLOBALS pinned below everything — at least under the tallest column,
        // and pushed to the bottom of the visible panel when there is spare room
        int yG = juce::jmax (y0, juce::jmax (y1, y2)) + 28;
        const int viewH = configViewport.getHeight();
        yG = juce::jmax (yG, viewH - cellH - 20);
        ControlRow* globals[] = { liteRow.get(), inGainRow.get(), outGainRow.get() };
        int gx = pad;
        for (auto* row : globals)
        {
            if (row == nullptr) continue;
            row->setVisible (true);
            row->setBounds (gx, yG, cellW, cellH);
            gx += cellW + gap;
        }
        yG += cellH + 16;

        configPage.setSize (viewW, juce::jmax (yG, viewH));
    }


    else
    {
        configViewport.setVisible (false);
        t3kPage.setVisible (true);
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


//==============================================================================
NativeNamPanel::ValueOverlay::ValueOverlay()
{
    auto& th = Theme::get();
    titleLab.setJustificationType (juce::Justification::centred);
    titleLab.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    titleLab.setColour (juce::Label::textColourId, th.text);
    titleLab.setText ("Edit value", juce::dontSendNotification);
    addAndMakeVisible (titleLab);

    editor.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId, th.surfaceAlt);
    editor.setColour (juce::TextEditor::textColourId, th.text);
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, th.accent);
    editor.setSelectAllWhenFocused (false);
    addAndMakeVisible (editor);

    th.applyButton (okBtn, true);
    th.applyButton (cancelBtn, false);
    addAndMakeVisible (okBtn);
    addAndMakeVisible (cancelBtn);
    addAndMakeVisible (keyboard);

    keyboard.attachEditor (&editor);
    okBtn.onClick = [this]
    {
        if (onDone) onDone (editor.getText());
    };
    cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
    keyboard.onEnter = [this] { if (onDone) onDone (editor.getText()); };
}

void NativeNamPanel::ValueOverlay::setInitial (const juce::String& t)
{
    editor.setText (t, false);
    editor.selectAll();
    editor.grabKeyboardFocus();
}

void NativeNamPanel::ValueOverlay::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.overlay);
}

void NativeNamPanel::ValueOverlay::resized()
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

NativeNamPanel::MidiOverlay::MidiOverlay()
{
    auto& th = Theme::get();
    setLookAndFeel (&th.softLaf);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, th.text);
    addAndMakeVisible (title);

    auto prep = [&] (juce::TextButton& b, bool primary = false)
    {
        b.setLookAndFeel (&th.softLaf);
        th.applyButton (b, primary);
    };
    prep (learnBtn, true);
    prep (clearBtn, false);
    prep (modeInstant, false);
    prep (modeToggle, false);
    prep (closeBtn, false);
    modeInstant.setClickingTogglesState (true);
    modeToggle.setClickingTogglesState (true);
    modeInstant.setRadioGroupId (8801);
    modeToggle.setRadioGroupId (8801);
    modeInstant.setColour (juce::TextButton::buttonOnColourId, th.accent);
    modeToggle.setColour (juce::TextButton::buttonOnColourId, th.accent);
    learnBtn.setButtonText ("LEARN");
    clearBtn.setButtonText ("Clear MIDI");
    modeInstant.setButtonText ("Instant / Hold");
    modeToggle.setButtonText ("Toggle");
    closeBtn.setButtonText ("Close");
    for (auto* b : { &learnBtn, &clearBtn, &modeInstant, &modeToggle, &closeBtn })
        addAndMakeVisible (b);
}

NativeNamPanel::MidiOverlay::~MidiOverlay()
{
    setLookAndFeel (nullptr);
    for (auto* b : { &learnBtn, &clearBtn, &modeInstant, &modeToggle, &closeBtn })
        b->setLookAndFeel (nullptr);
}

void NativeNamPanel::MidiOverlay::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.setColour (th.overlay);
    g.fillAll();
    auto card = getLocalBounds().withSizeKeepingCentre (
        juce::jmin (340, getWidth() - 32),
        juce::jmin (360, getHeight() - 32)).toFloat();
    g.setColour (th.surface);
    g.fillRoundedRectangle (card, 20.0f);
    g.setColour (th.surfaceAlt.withAlpha (0.9f));
    g.drawRoundedRectangle (card.reduced (0.5f), 20.0f, 1.0f);
}

void NativeNamPanel::MidiOverlay::resized()
{
    auto c = getLocalBounds().withSizeKeepingCentre (
        juce::jmin (340, getWidth() - 32),
        juce::jmin (360, getHeight() - 32)).reduced (18);
    title.setBounds (c.removeFromTop (40));
    c.removeFromTop (6);
    auto slot = [&] (juce::TextButton& b) { b.setBounds (c.removeFromTop (46).reduced (0, 4)); };
    slot (learnBtn); slot (clearBtn);
    c.removeFromTop (10);
    slot (modeInstant); slot (modeToggle);
    closeBtn.setBounds (c.removeFromBottom (46).reduced (0, 4));
}

void NativeNamPanel::closeOverlays()
{
    valueOverlay.reset();
    midiOverlay.reset();
}

void NativeNamPanel::openValueFor (ControlRow* cell)
{
    if (cell == nullptr || cell->parameter == nullptr || cell->isToggle) return;
    closeOverlays();
    auto* ov = new ValueOverlay();
    valueOverlay.reset (ov);
    ov->setInitial (cell->parameter->getText (cell->parameter->getValue(), 24));
    juce::Component::SafePointer<NativeNamPanel> safe (this);
    juce::Component::SafePointer<ControlRow> safeCell (cell);
    ov->onDone = [safe, safeCell] (juce::String text)
    {
        if (safe == nullptr) return;
        if (safeCell != nullptr && safeCell->parameter != nullptr)
        {
            // Try parse as normalised or use text conversion
            auto* p = safeCell->parameter;
            const float v = p->getValueForText (text);
            p->setValueNotifyingHost (v);
            safeCell->syncFromParam();
        }
        safe->closeOverlays();
    };
    ov->onCancel = [safe] { if (safe != nullptr) safe->closeOverlays(); };
    if (auto* parent = getTopLevelComponent())
    {
        parent->addAndMakeVisible (ov);
        ov->setBounds (parent->getLocalBounds());
        ov->toFront (true);
    }
}

void NativeNamPanel::openMidiFor (ControlRow* cell)
{
    if (cell == nullptr) return;
    closeOverlays();
    auto* pop = new MidiOverlay();
    midiOverlay.reset (pop);
    pop->title.setText (cell->nameLabel.getText(), juce::dontSendNotification);
    const auto mode = midiLearn.getBindingMode (cell->pluginIndex, cell->paramIndex);
    pop->modeInstant.setToggleState (mode == MidiLearnManager::MidiMode::Instant, juce::dontSendNotification);
    pop->modeToggle.setToggleState (mode == MidiLearnManager::MidiMode::Toggle, juce::dontSendNotification);

    juce::Component::SafePointer<NativeNamPanel> safe (this);
    juce::Component::SafePointer<ControlRow> safeCell (cell);
    pop->learnBtn.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.startLearn (safeCell->pluginIndex, safeCell->paramIndex);
        safeCell->updateLearnButton();
        safe->closeOverlays();
    };
    pop->clearBtn.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.clearBinding (safeCell->pluginIndex, safeCell->paramIndex);
        safeCell->updateLearnButton();
        safe->closeOverlays();
    };
    pop->modeInstant.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.setBindingMode (safeCell->pluginIndex, safeCell->paramIndex,
                                        MidiLearnManager::MidiMode::Instant);
    };
    pop->modeToggle.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.setBindingMode (safeCell->pluginIndex, safeCell->paramIndex,
                                        MidiLearnManager::MidiMode::Toggle);
    };
    pop->closeBtn.onClick = [safe] { if (safe != nullptr) safe->closeOverlays(); };

    if (auto* parent = getTopLevelComponent())
    {
        parent->addAndMakeVisible (pop);
        pop->setBounds (parent->getLocalBounds());
        pop->toFront (true);
    }
}
