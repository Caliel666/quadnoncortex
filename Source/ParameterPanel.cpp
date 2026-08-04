#include "ParameterPanel.h"
#include "NativeNam/NativeNamProcessor.h"

//==============================================================================
ParamKnobCell::ParamKnobCell (int paramIdx, juce::AudioProcessorParameter* param,
                              MidiLearnManager& mgr, int pluginIdx,
                              std::function<void(ParamKnobCell*)> onOpenMidiMenu,
                              std::function<void(ParamKnobCell*)> onOpenValueEdit)
    : paramIndex (paramIdx), pluginIndex (pluginIdx), parameter (param),
      learnManager (mgr), openMidiMenu (std::move (onOpenMidiMenu)),
      openValueEdit (std::move (onOpenValueEdit))
{
    auto& th = Theme::get();

    isToggle = param->isBoolean()
               || (param->getNumSteps() == 2 && param->getAllValueStrings().size() <= 2);
    isChoice = ! isToggle && param->isDiscrete() && param->getNumSteps() > 2
               && param->getNumSteps() < 32;

    defaultNorm = param->getDefaultValue();

    nameLabel.setText (param->getName (28), juce::dontSendNotification);
    nameLabel.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, th.text);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    valueLabel.setColour (juce::Label::textColourId, th.accent);
    valueLabel.setJustificationType (juce::Justification::centredLeft);
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    resetBtn.setButtonText (juce::String::fromUTF8 ("\xE2\x86\xBA"));
    resetBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    resetBtn.setColour (juce::TextButton::textColourOffId, th.textDim);
    resetBtn.onClick = [this]
    {
        if (parameter == nullptr) return;
        parameter->setValueNotifyingHost (defaultNorm);
        setFromNormalised (defaultNorm);
    };
    addAndMakeVisible (resetBtn);

    if (isToggle)
    {
        switchToggle.onChange = [this] (bool on)
        {
            if (parameter == nullptr) return;
            parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
            setFromNormalised (on ? 1.0f : 0.0f);
        };
        addAndMakeVisible (switchToggle);
        knob.setVisible (false);
        choiceBox.setVisible (false);
    }
    else if (isChoice)
    {
        const auto labels = param->getAllValueStrings();
        if (labels.isEmpty())
        {
            for (int i = 0; i < param->getNumSteps(); ++i)
                choiceBox.addItem (param->getText ((float) i / (float) juce::jmax (1, param->getNumSteps() - 1), 24), i + 1);
        }
        else
        {
            for (int i = 0; i < labels.size(); ++i)
                choiceBox.addItem (labels[i], i + 1);
        }
        choiceBox.setColour (juce::ComboBox::backgroundColourId, th.surfaceAlt);
        choiceBox.setColour (juce::ComboBox::outlineColourId, th.text.withAlpha (0.12f));
        choiceBox.setColour (juce::ComboBox::textColourId, th.text);
        choiceBox.onChange = [this]
        {
            if (parameter == nullptr) return;
            const int n = juce::jmax (1, parameter->getNumSteps() - 1);
            const int idx = choiceBox.getSelectedItemIndex();
            parameter->setValueNotifyingHost (n > 0 ? (float) idx / (float) n : 0.0f);
            setFromNormalised (parameter->getValue());
        };
        addAndMakeVisible (choiceBox);
        knob.setVisible (false);
        switchToggle.setVisible (false);
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
        choiceBox.setVisible (false);
    }

    setFromNormalised (param->getValue());
    refreshMidiIndicator();
}

void ParamKnobCell::sliderValueChanged (juce::Slider* s)
{
    if (s != &knob || parameter == nullptr) return;
    parameter->setValueNotifyingHost ((float) knob.getValue());
    valueLabel.setText (parameter->getText ((float) knob.getValue(), 24),
                        juce::dontSendNotification);
}

void ParamKnobCell::setFromNormalised (float v)
{
    if (isToggle)
        switchToggle.setOn (v >= 0.5f, juce::dontSendNotification);
    else if (isChoice && parameter != nullptr)
    {
        const int n = juce::jmax (1, parameter->getNumSteps() - 1);
        const int idx = juce::jlimit (0, juce::jmax (0, choiceBox.getNumItems() - 1),
                                      (int) std::round (v * (float) n));
        choiceBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    }
    else
        knob.setValue (v, juce::dontSendNotification);

    if (parameter != nullptr)
        valueLabel.setText (parameter->getText (v, 24), juce::dontSendNotification);
}

void ParamKnobCell::refreshMidiIndicator()
{
    const bool has = learnManager.findBinding (pluginIndex, paramIndex) != nullptr;
    const bool learning = learnManager.isLearning()
                          && learnManager.getLearnPlugin() == pluginIndex
                          && learnManager.getLearnParam() == paramIndex;
    auto& th = Theme::get();
    if (learning)
        nameLabel.setColour (juce::Label::textColourId, th.warning);
    else if (has)
        nameLabel.setColour (juce::Label::textColourId, th.success);
    else
        nameLabel.setColour (juce::Label::textColourId, th.text);
}

void ParamKnobCell::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (th.surface.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (th.text.withAlpha (0.07f));
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
}

void ParamKnobCell::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    nameLabel.setBounds (r.removeFromTop (24));
    auto bottom = r.removeFromBottom (22);
    resetBtn.setBounds (bottom.removeFromRight (26));
    valueLabel.setBounds (bottom);

    if (isToggle)
        switchToggle.setBounds (r.withSizeKeepingCentre (54, 32));
    else if (isChoice)
        choiceBox.setBounds (r.withSizeKeepingCentre (juce::jmin (128, r.getWidth() - 8), 36));
    else
        knob.setBounds (r.reduced (6));
}

void ParamKnobCell::mouseDown (const juce::MouseEvent& e)
{
    if (nameLabel.getBounds().contains (e.getPosition()))
    {
        if (openMidiMenu) openMidiMenu (this);
        return;
    }
    if (valueLabel.getBounds().contains (e.getPosition()) && ! isToggle)
    {
        if (openValueEdit) openValueEdit (this);
        return;
    }
}

//==============================================================================
void ParameterPanel::styleHeaderButton (juce::TextButton& b, bool primary)
{
    Theme::get().applyButton (b, primary);
    b.setColour (juce::TextButton::textColourOffId, Theme::get().text);
    b.setColour (juce::TextButton::textColourOnId, Theme::get().text);
}

ParameterPanel::ParameterPanel (MidiLearnManager& learnMgr)
    : midiLearn (learnMgr)
{
    titleLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Theme::get().text);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    styleHeaderButton (bypassButton);
    styleHeaderButton (monoButton);
    styleHeaderButton (bypassLearnButton);
    styleHeaderButton (bypassClearButton);
    styleHeaderButton (colourButton);

    bypassButton.onClick = [this] { if (bypassCallback) bypassCallback(); };
    monoButton.onClick = [this] { if (monoToggleCallback) monoToggleCallback(); };
    colourButton.onClick = [this] { if (colourCallback) colourCallback(); };
    openEditorButton.onClick = [this] { if (openEditorCallback) openEditorCallback(); };

    bypassLearnButton.onClick = [this]
    {
        if (currentPluginIndex < 0) return;
        if (midiLearn.isLearning()
            && midiLearn.getLearnPlugin() == currentPluginIndex
            && midiLearn.getLearnParam() == -2)
            midiLearn.cancelLearn();
        else
            midiLearn.startLearn (currentPluginIndex, -2);
        refreshMidiButtons();
    };
    bypassClearButton.onClick = [this]
    {
        if (currentPluginIndex >= 0)
            midiLearn.clearBinding (currentPluginIndex, -2);
        refreshMidiButtons();
    };

    addAndMakeVisible (openEditorButton);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (monoButton);
    addAndMakeVisible (bypassLearnButton);
    addAndMakeVisible (bypassClearButton);
    addAndMakeVisible (colourButton);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (kScrollBar);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (viewport);

    {
        auto prev = midiLearn.onBindingsChanged;
        juce::Component::SafePointer<ParameterPanel> safe (this);
        midiLearn.onBindingsChanged = [safe, prev]
        {
            if (prev) prev();
            if (safe != nullptr)
                safe->refreshMidiButtons();
        };
    }
}

ParameterPanel::~ParameterPanel() { clear(); }

void ParameterPanel::refreshMidiButtons()
{
    for (auto* c : cells)
        c->refreshMidiIndicator();

    if (currentPluginIndex < 0) return;

    const bool learningBypass = midiLearn.isLearning()
                                && midiLearn.getLearnPlugin() == currentPluginIndex
                                && midiLearn.getLearnParam() == -2;
    const bool has = midiLearn.findBinding (currentPluginIndex, -2) != nullptr;
    auto& th = Theme::get();
    if (learningBypass)
    {
        bypassLearnButton.setButtonText ("...");
        bypassLearnButton.setColour (juce::TextButton::buttonColourId, th.warning);
    }
    else if (has)
    {
        bypassLearnButton.setButtonText ("MIDI");
        bypassLearnButton.setColour (juce::TextButton::buttonColourId, th.success);
    }
    else
    {
        bypassLearnButton.setButtonText ("LEARN");
        styleHeaderButton (bypassLearnButton);
    }
    bypassClearButton.setEnabled (has);
    bypassClearButton.setAlpha (has ? 1.0f : 0.4f);
}

void ParameterPanel::setPlugin (juce::AudioPluginInstance* instance, int pluginIndex,
                                bool bypassed, bool mono, std::function<void()> onBypass,
                                std::function<void()> onMonoToggle,
                                std::function<void()> onColour,
                                std::function<void()> onOpenEditor)
{
    clear();

    currentPlugin = instance;
    currentPluginIndex = pluginIndex;
    bypassCallback = std::move (onBypass);
    monoToggleCallback = std::move (onMonoToggle);
    colourCallback = std::move (onColour);
    openEditorCallback = std::move (onOpenEditor);

    if (instance == nullptr)
    {
        titleLabel.setText ({}, juce::dontSendNotification);
        setVisible (false);
        return;
    }

    titleLabel.setText (instance->getName(), juce::dontSendNotification);
    updateBypass (bypassed);
    updateMono (mono);
    openEditorButton.setEnabled (instance->hasEditor());
    refreshMidiButtons();

    if (auto* nam = dynamic_cast<NativeNamProcessor*> (instance))
    {
        nativeNamPanel.reset (new NativeNamPanel (*nam, midiLearn, pluginIndex));
        content.addAndMakeVisible (nativeNamPanel.get());
        setVisible (true);
        resized();
        return;
    }

    juce::Component::SafePointer<ParameterPanel> safe (this);
    const auto& params = instance->getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        if (auto* p = params[i])
        {
            if (! p->isAutomatable()) continue;
            auto* cell = cells.add (new ParamKnobCell (
                i, p, midiLearn, pluginIndex,
                [safe] (ParamKnobCell* c) { if (safe != nullptr) safe->openMidiMenuFor (c); },
                [safe] (ParamKnobCell* c) { if (safe != nullptr) safe->openValueEditFor (c); }));
            content.addAndMakeVisible (cell);
            p->addListener (this);
        }
    }

    setVisible (true);
    resized();
    refreshMidiButtons();
}

void ParameterPanel::clear()
{
    closeOverlays();
    if (currentPlugin != nullptr)
        for (auto* p : currentPlugin->getParameters())
            if (p != nullptr) p->removeListener (this);
    cells.clear();
    nativeNamPanel.reset();
    currentPlugin = nullptr;
    currentPluginIndex = -1;
    bypassCallback = nullptr;
    monoToggleCallback = nullptr;
    colourCallback = nullptr;
    openEditorCallback = nullptr;
    titleLabel.setText ({}, juce::dontSendNotification);
    bypassButton.setButtonText ("BYPASS");
    monoButton.setButtonText ("STEREO");
}

void ParameterPanel::updateParamValue (int paramIndex, float value)
{
    for (auto* c : cells)
        if (c->paramIndex == paramIndex)
            c->setFromNormalised (value);
}

void ParameterPanel::updateBypass (bool bypassed)
{
    bypassButton.setButtonText (bypassed ? "BYPASSED" : "BYPASS");
    if (bypassed)
        bypassButton.setColour (juce::TextButton::buttonColourId, Theme::get().warning);
    else
        styleHeaderButton (bypassButton);
}

void ParameterPanel::updateMono (bool mono)
{
    monoButton.setButtonText (mono ? "MONO" : "STEREO");
}

void ParameterPanel::parameterValueChanged (int parameterIndex, float newValue)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ParameterPanel> (this),
                                      parameterIndex, newValue]
    {
        if (safe == nullptr) return;
        safe->updateParamValue (parameterIndex, newValue);
        // Section masters may have toggled — relayout
        safe->rebuildGrid();
    });
}

void ParameterPanel::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.setColour (th.surface.withAlpha (0.94f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 16.0f);
}

void ParameterPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (kHeaderH);

    // Left: window-icon UI button
    openEditorButton.setBounds (header.removeFromLeft (kHeaderH - 4).reduced (2));
    colourButton.setBounds (header.removeFromLeft (64).reduced (3, 6));

    // Right: STEREO | BYPASS | LEARN | X
    const int btnH = header.getHeight() - 10;
    auto placeRight = [&] (juce::TextButton& b, int w)
    {
        b.setBounds (header.removeFromRight (w).reduced (3, (header.getHeight() - btnH) / 2));
    };
    // LEARN/X removed from header — MIDI for bypass is on the plugin block
    placeRight (bypassButton, 88);
    placeRight (monoButton, 78);
    bypassLearnButton.setVisible (false);
    bypassClearButton.setVisible (false);

    // Centre: plugin name
    titleLabel.setBounds (header);

    viewport.setBounds (r);
    viewport.setScrollBarThickness (kScrollBar);

    if (nativeNamPanel != nullptr)
    {
        content.setSize (juce::jmax (1, viewport.getWidth() - kScrollBar), viewport.getHeight());
        nativeNamPanel->setBounds (content.getLocalBounds());
        return;
    }

    rebuildGrid();
}


void ParameterPanel::updateCellVisibility()
{
    // Infer "section" masters from * Active / Active * Section toggles.
    // When a master is off, only the master stays visible; siblings under its prefix hide.
    struct Master {
        int index = -1;
        juce::String prefix; // e.g. "Gate", "Overdrive-1", "Pre FX"
        bool on = true;
    };
    std::vector<Master> masters;

    auto makePrefix = [] (juce::String name) -> juce::String
    {
        name = name.trim();
        // "Active Pre FX Section" / "Active Amp Section"
        if (name.startsWithIgnoreCase ("Active "))
        {
            name = name.substring (7).trim();
            if (name.endsWithIgnoreCase (" Section"))
                name = name.dropLastCharacters (8).trim();
            return name;
        }
        // "Gate Active", "Overdrive-1 Active"
        if (name.endsWithIgnoreCase (" Active"))
            return name.dropLastCharacters (7).trim();
        if (name.endsWithIgnoreCase (" Enable") || name.endsWithIgnoreCase (" Enabled"))
            return name.upToLastOccurrenceOf (" ", false, false).trim();
        // "Bypass Pedal", "Bypass Gate" — master when ON hides section (inverted sense)
        if (name.startsWithIgnoreCase ("Bypass "))
            return name.fromFirstOccurrenceOf (" ", false, false).trim();
        // Bare "Gate" toggle is the section master for Gate Threshold / Hard Gate / Attack / Release
        if (name.equalsIgnoreCase ("Gate"))
            return "Gate";
        return {};
    };

    for (auto* c : cells)
    {
        if (c == nullptr || c->parameter == nullptr) continue;
        if (! c->isToggle) continue;
        const auto name = c->parameter->getName (64);
        const auto prefix = makePrefix (name);
        if (prefix.isEmpty()) continue;
        Master m;
        m.index = c->paramIndex;
        m.prefix = prefix;
        const auto rawName = c->parameter->getName (64);
        // Bypass* ON means section is inactive
        if (rawName.startsWithIgnoreCase ("Bypass "))
            m.on = c->parameter->getValue() < 0.5f;
        else
            m.on = c->parameter->getValue() >= 0.5f;
        masters.push_back (m);
    }

    // Longer prefixes first so "Overdrive-1" wins over "Overdrive"
    std::sort (masters.begin(), masters.end(),
               [] (const Master& a, const Master& b) { return a.prefix.length() > b.prefix.length(); });

    auto isChildOf = [] (const juce::String& paramName, const juce::String& prefix) -> bool
    {
        if (prefix.isEmpty()) return false;
        if (paramName.equalsIgnoreCase (prefix))
            return false; // the master itself
        // "Gate Threshold", "Gate Attack"
        if (paramName.startsWithIgnoreCase (prefix + " "))
            return true;
        if (paramName.startsWithIgnoreCase (prefix + "-"))
            return true;
        // "Hard Gate" under master "Gate" — prefix appears as a whole word
        if (prefix.equalsIgnoreCase ("Gate")
            && paramName.containsWholeWordIgnoreCase ("Gate"))
            return true;
        return false;
    };

    auto isMasterName = [&] (const juce::String& name) -> bool
    {
        for (const auto& m : masters)
        {
            const auto n = name.trim();
            if (n.endsWithIgnoreCase (" Active") && n.dropLastCharacters (7).trim().equalsIgnoreCase (m.prefix))
                return true;
            if (n.startsWithIgnoreCase ("Active ") && n.containsIgnoreCase (m.prefix))
                return true;
        }
        return false;
    };

    for (auto* c : cells)
    {
        if (c == nullptr || c->parameter == nullptr)
        {
            if (c) c->setVisible (true);
            continue;
        }
        const auto name = c->parameter->getName (64);
        bool visible = true;

        for (const auto& m : masters)
        {
            // Never hide the master itself
            if (c->paramIndex == m.index)
                continue;

            if (isChildOf (name, m.prefix) ||
                (name.startsWithIgnoreCase (m.prefix) && name != m.prefix
                 && ! isMasterName (name)
                 && (name[m.prefix.length()] == ' ' || name[m.prefix.length()] == '-'
                     || name.startsWithIgnoreCase (m.prefix + " "))))
            {
                // Exclude other masters that share a shorter prefix
                bool isOtherMaster = false;
                for (const auto& m2 : masters)
                    if (m2.index == c->paramIndex) { isOtherMaster = true; break; }
                if (isOtherMaster) continue;

                if (! m.on)
                {
                    visible = false;
                    break;
                }
            }
        }

        c->setVisible (visible);
    }
}

void ParameterPanel::rebuildGrid()
{
    updateCellVisibility();

    const int vw = juce::jmax (1, viewport.getWidth() - kScrollBar);
    const int cols = juce::jmax (1, vw / kCellW);
    int place = 0;
    for (auto* c : cells)
    {
        if (c == nullptr || ! c->isVisible()) continue;
        const int col = place % cols;
        const int row = place / cols;
        c->setBounds (col * kCellW, row * kCellH, kCellW, kCellH);
        ++place;
    }
    const int rowsN = place > 0 ? (place + cols - 1) / cols : 0;
    content.setSize (vw, juce::jmax (viewport.getHeight(), rowsN * kCellH + 8));
}

void ParameterPanel::openMidiMenuFor (ParamKnobCell* cell)
{
    if (cell == nullptr) return;
    closeOverlays();
    menuTarget = cell;

    auto* pop = new MidiMenuPopup();
    pop->title.setText (cell->parameter != nullptr ? cell->parameter->getName (40) : "MIDI",
                        juce::dontSendNotification);

    const bool has = midiLearn.findBinding (cell->pluginIndex, cell->paramIndex) != nullptr;
    const auto mode = midiLearn.getBindingMode (cell->pluginIndex, cell->paramIndex);
    pop->modeInstant.setToggleState (mode == MidiLearnManager::MidiMode::Instant, juce::dontSendNotification);
    pop->modeToggle.setToggleState (mode == MidiLearnManager::MidiMode::Toggle, juce::dontSendNotification);
    pop->clearBtn.setEnabled (has);
    pop->clearBtn.setAlpha (has ? 1.0f : 0.4f);

    juce::Component::SafePointer<ParameterPanel> safe (this);
    juce::Component::SafePointer<ParamKnobCell> safeCell (cell);

    pop->learnBtn.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.startLearn (safeCell->pluginIndex, safeCell->paramIndex);
        safe->refreshMidiButtons();
        safe->closeOverlays();
    };
    pop->clearBtn.onClick = [safe, safeCell]
    {
        if (safe == nullptr || safeCell == nullptr) return;
        safe->midiLearn.clearBinding (safeCell->pluginIndex, safeCell->paramIndex);
        safe->refreshMidiButtons();
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
    else
    {
        addAndMakeVisible (pop);
        pop->setBounds (getLocalBounds());
    }
    midiPopup.reset (pop);
}

void ParameterPanel::openBypassMidiMenuFor (int pluginIndex)
{
    if (pluginIndex < 0) return;
    closeOverlays();
    menuTarget = nullptr;

    auto* pop = new MidiMenuPopup();
    pop->title.setText ("Plugin Bypass", juce::dontSendNotification);

    const bool has = midiLearn.findBinding (pluginIndex, -2) != nullptr;
    const auto mode = midiLearn.getBindingMode (pluginIndex, -2);
    pop->modeInstant.setToggleState (mode == MidiLearnManager::MidiMode::Instant, juce::dontSendNotification);
    pop->modeToggle.setToggleState (mode == MidiLearnManager::MidiMode::Toggle, juce::dontSendNotification);
    pop->clearBtn.setEnabled (has);
    pop->clearBtn.setAlpha (has ? 1.0f : 0.4f);

    juce::Component::SafePointer<ParameterPanel> safe (this);
    const int pIdx = pluginIndex;

    pop->learnBtn.onClick = [safe, pIdx]
    {
        if (safe == nullptr) return;
        safe->midiLearn.startLearn (pIdx, -2);
        safe->refreshMidiButtons();
        safe->closeOverlays();
    };
    pop->clearBtn.onClick = [safe, pIdx]
    {
        if (safe == nullptr) return;
        safe->midiLearn.clearBinding (pIdx, -2);
        safe->refreshMidiButtons();
        safe->closeOverlays();
    };
    pop->modeInstant.onClick = [safe, pIdx]
    {
        if (safe == nullptr) return;
        safe->midiLearn.setBindingMode (pIdx, -2, MidiLearnManager::MidiMode::Instant);
    };
    pop->modeToggle.onClick = [safe, pIdx]
    {
        if (safe == nullptr) return;
        safe->midiLearn.setBindingMode (pIdx, -2, MidiLearnManager::MidiMode::Toggle);
    };
    pop->closeBtn.onClick = [safe] { if (safe != nullptr) safe->closeOverlays(); };

    midiPopup.reset (pop);
    if (auto* parent = getTopLevelComponent())
    {
        parent->addAndMakeVisible (pop);
        pop->setBounds (parent->getLocalBounds());
        pop->toFront (true);
    }
    else
    {
        addAndMakeVisible (pop);
        pop->setBounds (getLocalBounds());
        pop->toFront (true);
    }
}


void ParameterPanel::openValueEditFor (ParamKnobCell* cell)
{
    if (cell == nullptr || cell->parameter == nullptr || cell->isToggle) return;
    closeOverlays();
    menuTarget = cell;

    auto* ov = new ValueEditOverlay();
    ov->titleLab.setText (cell->parameter->getName (32), juce::dontSendNotification);
    ov->setInitial (cell->parameter->getText (cell->parameter->getValue(), 24));

    juce::Component::SafePointer<ParameterPanel> safe (this);
    juce::Component::SafePointer<ParamKnobCell> safeCell (cell);
    ov->onDone = [safe, safeCell] (juce::String text)
    {
        if (safe != nullptr)
            safe->closeOverlays();
        if (text.isEmpty() || safeCell == nullptr || safeCell->parameter == nullptr)
            return;
        const float norm = safeCell->parameter->getValueForText (text);
        safeCell->parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        safeCell->setFromNormalised (safeCell->parameter->getValue());
    };

    if (auto* parent = getTopLevelComponent())
    {
        parent->addAndMakeVisible (ov);
        ov->setBounds (parent->getLocalBounds());
        ov->toFront (true);
        juce::MessageManager::callAsync ([ed = &ov->editor] { ed->grabKeyboardFocus(); });
    }
    else
    {
        addAndMakeVisible (ov);
        ov->setBounds (getLocalBounds());
    }
    valueOverlay.reset (ov);
}

void ParameterPanel::closeOverlays()
{
    if (valueOverlay != nullptr)
    {
        if (auto* p = valueOverlay->getParentComponent())
            p->removeChildComponent (valueOverlay.get());
        valueOverlay.reset();
    }
    if (midiPopup != nullptr)
    {
        if (auto* p = midiPopup->getParentComponent())
            p->removeChildComponent (midiPopup.get());
        midiPopup.reset();
    }
    menuTarget = nullptr;
}
