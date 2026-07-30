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

private:
    void setTab (int t);

    AudioEngine& audioEngine;
    MidiLearnManager& midiLearn;

    juce::TextButton tabAudio { "AUDIO" };
    juce::TextButton tabMidi  { "MIDI" };
    int currentTab = 0;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::TextButton saveButton { "SAVE SETTINGS" };
    juce::TextButton exitButton { "EXIT APP" };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
};
