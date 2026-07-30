#include "SettingsComponent.h"
#include "AppSettings.h"

SettingsComponent::MapRow::MapRow (const juce::String& actionName, const juce::String& label,
                                   MidiLearnManager& mgr)
    : action (actionName), learn (mgr)
{
    nameLabel.setText (label, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont (juce::FontOptions (15.0f));
    addAndMakeVisible (nameLabel);
    bindingLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    bindingLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (bindingLabel);
    learnBtn.onClick = [this]
    {
        if (learn.isLearning()) learn.cancelLearn();
        else learn.startLearnGlobal (action);
        refresh();
    };
    clearBtn.onClick = [this] { learn.clearGlobal (action); refresh(); };
    addAndMakeVisible (learnBtn);
    addAndMakeVisible (clearBtn);
    refresh();
}

void SettingsComponent::MapRow::refresh()
{
    if (auto* b = learn.findGlobal (action))
    {
        juce::String s = b->ccNumber >= 0
            ? ("Ch" + juce::String (b->midiChannel) + " CC" + juce::String (b->ccNumber))
            : ("Ch" + juce::String (b->midiChannel) + " Note " + juce::String (b->noteNumber));
        bindingLabel.setText (s, juce::dontSendNotification);
        learnBtn.setButtonText ("MIDI");
        learnBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
    }
    else if (learn.isLearning())
    {
        bindingLabel.setText ("Move a control...", juce::dontSendNotification);
        learnBtn.setButtonText ("...");
        learnBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffe67e22));
    }
    else
    {
        bindingLabel.setText ("Not mapped", juce::dontSendNotification);
        learnBtn.setButtonText ("LEARN");
        learnBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff455a64));
    }
}

void SettingsComponent::MapRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1e1e1e));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2), 6.0f);
}

void SettingsComponent::MapRow::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    clearBtn.setBounds (r.removeFromRight (36));
    r.removeFromRight (4);
    learnBtn.setBounds (r.removeFromRight (80));
    r.removeFromRight (8);
    nameLabel.setBounds (r.removeFromLeft (140));
    bindingLabel.setBounds (r);
}

//==============================================================================
SettingsComponent::SettingsComponent (AudioEngine& engine, MidiLearnManager& learn)
    : audioEngine (engine), midiLearn (learn)
{
    tabAudio.setClickingTogglesState (true);
    tabMidi.setClickingTogglesState (true);
    tabAudio.setRadioGroupId (42);
    tabMidi.setRadioGroupId (42);
    tabAudio.setToggleState (true, juce::dontSendNotification);
    tabAudio.onClick = [this] { setTab (0); };
    tabMidi.onClick  = [this] { setTab (1); };
    addAndMakeVisible (tabAudio);
    addAndMakeVisible (tabMidi);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        audioEngine.getDeviceManager(),
        0, 2, 0, 2,
        true,   // MIDI inputs shown on audio tab still useful, but primarily audio
        false,  // no MIDI outputs
        false,  // individual channels
        false);
    addAndMakeVisible (*deviceSelector);

    saveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27ae60));
    saveButton.onClick = [this]
    {
        audioEngine.saveDeviceState();
        AppSettings::get().save();
        saveButton.setButtonText ("SAVED");
        juce::Timer::callAfterDelay (1200, [this] { saveButton.setButtonText ("SAVE SETTINGS"); });
        if (onSaved) onSaved();
    };
    addAndMakeVisible (saveButton);

    exitButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
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

    mapsTitle.setText ("Global MIDI Maps", juce::dontSendNotification);
    mapsTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    mapsTitle.setColour (juce::Label::textColourId, juce::Colours::white);
    midiPage.addAndMakeVisible (mapsTitle);

    auto addMap = [this] (const juce::String& a, const juce::String& l)
    {
        auto* row = mapRows.add (new MapRow (a, l, midiLearn));
        midiPage.addAndMakeVisible (row);
    };
    addMap ("tuner", "Tuner toggle");
    addMap ("presetNext", "Preset next");
    addMap ("presetPrev", "Preset previous");
    addAndMakeVisible (midiPage);
    midiPage.setVisible (false);

    {
        auto prev = midiLearn.onBindingsChanged;
        midiLearn.onBindingsChanged = [this, prev]
        {
            if (prev) prev();
            for (auto* r : mapRows) r->refresh();
        };
    }

    setSize (520, 560);
}

void SettingsComponent::setTab (int t)
{
    currentTab = t;
    if (deviceSelector) deviceSelector->setVisible (t == 0);
    midiPage.setVisible (t == 1);
    tabAudio.setToggleState (t == 0, juce::dontSendNotification);
    tabMidi.setToggleState (t == 1, juce::dontSendNotification);
    resized();
}

void SettingsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void SettingsComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto bottom = r.removeFromBottom (44);
    exitButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).reduced (2));
    saveButton.setBounds (bottom.reduced (2));
    r.removeFromBottom (8);

    auto tabs = r.removeFromTop (36);
    tabAudio.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (2));
    tabMidi.setBounds (tabs.reduced (2));
    r.removeFromTop (8);

    if (currentTab == 0)
    {
        if (deviceSelector) deviceSelector->setBounds (r);
    }
    else
    {
        midiPage.setBounds (r);
        auto m = midiPage.getLocalBounds();
        mapsTitle.setBounds (m.removeFromTop (28));
        for (auto* row : mapRows)
            row->setBounds (m.removeFromTop (48));
    }
}
