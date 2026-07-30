#include "ParameterPanel.h"

ParameterPanel::ParamRow::ParamRow (int paramIdx, juce::AudioProcessorParameter* param,
                                    MidiLearnManager& mgr, int pluginIdx)
    : paramIndex (paramIdx), pluginIndex (pluginIdx), parameter (param), learnManager (mgr)
{
    nameLabel.setText (param->getName (48), juce::dontSendNotification);
    nameLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (nameLabel);

    valueLabel.setFont (juce::FontOptions (14.0f));
    valueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    valueLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (valueLabel);

    // Detect boolean / toggle parameters
    isToggle = param->isBoolean()
               || (param->getNumSteps() == 2)
               || (param->getAllValueStrings().size() == 2);

    if (isToggle)
    {
        toggle.setToggleState (param->getValue() >= 0.5f, juce::dontSendNotification);
        toggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4fc3f7));
        toggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        toggle.onClick = [this]
        {
            if (parameter != nullptr)
                parameter->setValueNotifyingHost (toggle.getToggleState() ? 1.0f : 0.0f);
            valueLabel.setText (parameter->getText (parameter->getValue(), 32),
                                juce::dontSendNotification);
        };
        addAndMakeVisible (toggle);
        slider.setVisible (false);
    }
    else
    {
        slider.setRange (0.0, 1.0, 0.0);
        slider.setValue (param->getValue(), juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff4fc3f7));
        slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff37474f));
        slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff263238));
        addAndMakeVisible (slider);
        toggle.setVisible (false);
    }

    valueLabel.setText (param->getText (param->getValue(), 32), juce::dontSendNotification);

    learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
    learnButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    learnButton.onClick = [this]
    {
        if (learnManager.isLearning()) learnManager.cancelLearn();
        else learnManager.startLearn (pluginIndex, paramIndex);
        updateLearnButton();
    };
    addAndMakeVisible (learnButton);

    clearMidiButton.setButtonText ("X");
    clearMidiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
    clearMidiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    clearMidiButton.setTooltip ("Clear MIDI assignment");
    clearMidiButton.onClick = [this]
    {
        learnManager.clearBinding (pluginIndex, paramIndex);
        updateLearnButton();
    };
    addAndMakeVisible (clearMidiButton);
    updateLearnButton();
}

ParameterPanel::ParamRow::~ParamRow() {}

void ParameterPanel::ParamRow::setFromNormalised (float v)
{
    if (isToggle)
        toggle.setToggleState (v >= 0.5f, juce::dontSendNotification);
    else
        slider.setValue (v, juce::dontSendNotification);

    if (parameter != nullptr)
        valueLabel.setText (parameter->getText (v, 32), juce::dontSendNotification);
}

void ParameterPanel::ParamRow::updateLearnButton()
{
    const bool hasMidi = learnManager.findBinding (pluginIndex, paramIndex) != nullptr;
    // Only THIS row shows "..." — not every parameter (that looked like a crash/bug)
    const bool learningThis = learnManager.isLearning()
                              && learnManager.getLearnPlugin() == pluginIndex
                              && learnManager.getLearnParam() == paramIndex;

    if (learningThis)
    {
        learnButton.setButtonText ("...");
        learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffe67e22));
    }
    else if (hasMidi)
    {
        learnButton.setButtonText ("MIDI");
        learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
    }
    else
    {
        learnButton.setButtonText ("LEARN");
        learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
    }

    clearMidiButton.setVisible (true);
    clearMidiButton.setEnabled (hasMidi);
    clearMidiButton.setAlpha (hasMidi ? 1.0f : 0.35f);
}

void ParameterPanel::ParamRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 12.0f);
}

void ParameterPanel::ParamRow::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const int btnY = r.getY() + (r.getHeight() - 36) / 2;
    clearMidiButton.setBounds (r.removeFromRight (36).withHeight (36).withY (btnY));
    r.removeFromRight (4);
    learnButton.setBounds (r.removeFromRight (72).withHeight (36).withY (btnY));
    r.removeFromRight (8);
    valueLabel.setBounds (r.removeFromRight (90).removeFromTop (22));
    nameLabel.setBounds (r.removeFromTop (22));
    if (isToggle)
        toggle.setBounds (r.withWidth (80));
    else
        slider.setBounds (r);
}

//==============================================================================
ParameterPanel::ParameterPanel (MidiLearnManager& learnMgr) : midiLearn (learnMgr)
{
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    bypassButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff546e7a));
    bypassButton.onClick = [this] { if (bypassCallback) bypassCallback(); };
    addAndMakeVisible (bypassButton);
    bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
    bypassLearnButton.onClick = [this]
    {
        if (currentPluginIndex < 0) return;
        if (midiLearn.isLearning()) midiLearn.cancelLearn();
        else midiLearn.startLearn (currentPluginIndex, -2); // -2 = bypass
        const bool learningBypass = midiLearn.isLearning()
                                    && midiLearn.getLearnPlugin() == currentPluginIndex
                                    && midiLearn.getLearnParam() == -2;
        if (learningBypass)
        {
            bypassLearnButton.setButtonText ("...");
            bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffe67e22));
            bypassClearButton.setEnabled (false);
            bypassClearButton.setAlpha (0.35f);
        }
        else if (midiLearn.findBinding (currentPluginIndex, -2) != nullptr)
        {
            bypassLearnButton.setButtonText ("MIDI");
            bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
            bypassClearButton.setEnabled (true);
            bypassClearButton.setAlpha (1.0f);
        }
        else
        {
            bypassLearnButton.setButtonText ("LEARN");
            bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
            bypassClearButton.setEnabled (false);
            bypassClearButton.setAlpha (0.35f);
        }
    };
    addAndMakeVisible (bypassLearnButton);

    bypassClearButton.setButtonText ("X");
    bypassClearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
    bypassClearButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    bypassClearButton.setVisible (true);
    bypassClearButton.setEnabled (false);
    bypassClearButton.setAlpha (0.35f);
    bypassClearButton.onClick = [this]
    {
        if (currentPluginIndex < 0) return;
        midiLearn.clearBinding (currentPluginIndex, -2);
        bypassLearnButton.setButtonText ("LEARN");
        bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
        bypassClearButton.setEnabled (false);
        bypassClearButton.setAlpha (0.35f);
    };
    addAndMakeVisible (bypassClearButton);

    colourButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5c6bc0));
    addAndMakeVisible (colourButton);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    {
        auto prev = midiLearn.onBindingsChanged;
        midiLearn.onBindingsChanged = [this, prev]
        {
            if (prev) prev();
            for (auto* row : rows)
                row->updateLearnButton();
            if (currentPluginIndex >= 0)
            {
                const bool learningBypass = midiLearn.isLearning()
                                           && midiLearn.getLearnPlugin() == currentPluginIndex
                                           && midiLearn.getLearnParam() == -2;
                const bool has = midiLearn.findBinding (currentPluginIndex, -2) != nullptr;
                if (learningBypass)
                {
                    bypassLearnButton.setButtonText ("...");
                    bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffe67e22));
                }
                else if (has)
                {
                    bypassLearnButton.setButtonText ("MIDI");
                    bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
                }
                else
                {
                    bypassLearnButton.setButtonText ("LEARN");
                    bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
                }
                bypassClearButton.setVisible (true);
                bypassClearButton.setEnabled (has && ! learningBypass);
                bypassClearButton.setAlpha ((has && ! learningBypass) ? 1.0f : 0.35f);
            }
        };
    }
}

ParameterPanel::~ParameterPanel()
{
    clear();
}

void ParameterPanel::setPlugin (juce::AudioPluginInstance* instance, int pluginIndex,
                                bool bypassed, std::function<void()> onBypass,
                                std::function<void()> onColour)
{
    clear();
    midiLearn.cancelLearn();
    currentPlugin = instance;
    currentPluginIndex = pluginIndex;
    bypassCallback = std::move (onBypass);
    colourCallback = std::move (onColour);
    colourButton.onClick = [this] { if (colourCallback) colourCallback(); };

    if (instance == nullptr) return;

    titleLabel.setText (instance->getName(), juce::dontSendNotification);
    updateBypass (bypassed);
    if (midiLearn.findBinding (pluginIndex, -2) != nullptr)
    {
        bypassLearnButton.setButtonText ("MIDI");
        bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
        bypassClearButton.setVisible (true);
            bypassClearButton.setEnabled (true);
            bypassClearButton.setAlpha (1.0f);
    }
    else
    {
        bypassLearnButton.setButtonText ("LEARN");
        bypassLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
        bypassClearButton.setVisible (true);
            bypassClearButton.setEnabled (false);
            bypassClearButton.setAlpha (0.35f);
    }

    const auto& params = instance->getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        p->addListener (this);
        auto* row = rows.add (new ParamRow (i, p, midiLearn, pluginIndex));
        row->slider.addListener (this);
        content.addAndMakeVisible (row);
    }
    resized();
}

void ParameterPanel::clear()
{
    if (currentPlugin != nullptr)
        for (auto* p : currentPlugin->getParameters())
            p->removeListener (this);

    rows.clear();
    currentPlugin = nullptr;
    currentPluginIndex = -1;
    titleLabel.setText ({}, juce::dontSendNotification);
    content.setSize (0, 0);
}

void ParameterPanel::parameterValueChanged (int parameterIndex, float newValue)
{
    // Called from audio/message thread – hop to message thread
    juce::MessageManager::callAsync ([this, parameterIndex, newValue]
    {
        updateParamValue (parameterIndex, newValue);
    });
}

void ParameterPanel::updateParamValue (int paramIndex, float value)
{
    for (auto* row : rows)
        if (row->paramIndex == paramIndex)
        {
            row->setFromNormalised (value);
            break;
        }
}

void ParameterPanel::updateBypass (bool bypassed)
{
    bypassButton.setButtonText (bypassed ? "BYPASSED" : "BYPASS");
    bypassButton.setColour (juce::TextButton::buttonColourId,
                            bypassed ? juce::Colour (0xffe74c3c) : juce::Colour (0xff546e7a));
}

void ParameterPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
}

void ParameterPanel::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (40).reduced (8, 4);
    bypassClearButton.setBounds (top.removeFromRight (36));
    top.removeFromRight (3);
    bypassLearnButton.setBounds (top.removeFromRight (64));
    top.removeFromRight (4);
    bypassButton.setBounds (top.removeFromRight (100));
    top.removeFromRight (6);
    colourButton.setBounds (top.removeFromRight (80));
    titleLabel.setBounds (top);

    viewport.setBounds (r);
    const int contentHeight = rows.size() * kRowHeight;
    content.setSize (viewport.getWidth() - 8, juce::jmax (contentHeight, viewport.getHeight()));
    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds (0, y, content.getWidth(), kRowHeight);
        y += kRowHeight;
    }
}

void ParameterPanel::sliderValueChanged (juce::Slider* s)
{
    if (currentPlugin == nullptr) return;
    for (auto* row : rows)
    {
        if (&row->slider == s && row->parameter != nullptr)
        {
            const float v = (float) s->getValue();
            row->parameter->setValueNotifyingHost (v);
            row->valueLabel.setText (row->parameter->getText (v, 32), juce::dontSendNotification);
            break;
        }
    }
}
