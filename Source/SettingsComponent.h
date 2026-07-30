#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "MidiLearnManager.h"

class SettingsComponent : public juce::Component
{
public:
    SettingsComponent (AudioEngine& engine, MidiLearnManager& learn);
    ~SettingsComponent() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onSaved;
    std::function<void()> onThemeChanged;
    std::function<void()> onCloseRequested;

private:
    void setTab (int t);
    void refreshThemeList();
    void applyThemeColours();
    void rebuildMidiInputList();
    void refreshMapRows();

    AudioEngine& audioEngine;
    MidiLearnManager& midiLearn;

    juce::TextButton tabAudio  { "AUDIO" };
    juce::TextButton tabMidi   { "MIDI" };
    juce::TextButton tabTheme  { "GENERAL" };
    juce::Label headerLabel { {}, "Settings" };
    juce::TextButton closeBtn { "Close" };
    int currentTab = 0;

    // Stock JUCE device selector (full functionality) — styled via LookAndFeel
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::Viewport audioViewport;
    juce::Component audioPage;
    juce::Label audioTitle;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton exitButton { "EXIT" };

    // ---- MIDI page ----
    juce::Label midiInputsTitle;
    juce::OwnedArray<juce::ToggleButton> midiInputToggles;
    struct MapRow : public juce::Component
    {
        MapRow (const juce::String& actionName, const juce::String& label, MidiLearnManager& mgr);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh();
        juce::String action;
        juce::Label nameLabel, bindingLabel;
        juce::TextButton learnBtn { "LEARN" }, clearBtn { "X" };
        MidiLearnManager& learn;
    };
    juce::Label mapsTitle;
    juce::OwnedArray<MapRow> mapRows;
    juce::Component midiPage;

    // ---- Theme page ----
    juce::Component themePage;
    juce::Label themeTitle, themeHint;
    juce::ComboBox themeBox;
    juce::TextButton applyThemeBtn { "APPLY" };
    juce::TextButton updateBtn { "CHECK FOR UPDATES" };
    juce::Label versionLabel;
    juce::Label updateStatus;
    void checkForUpdates();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
};
