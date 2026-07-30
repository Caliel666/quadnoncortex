#pragma once
#include <JuceHeader.h>
#include "MidiLearnManager.h"

class ParameterPanel : public juce::Component,
                       public juce::Slider::Listener,
                       public juce::AudioProcessorParameter::Listener
{
public:
    ParameterPanel (MidiLearnManager& learnMgr);
    ~ParameterPanel() override;

    void setPlugin (juce::AudioPluginInstance* instance, int pluginIndex,
                    bool bypassed, std::function<void()> onBypass,
                    std::function<void()> onColour = nullptr);
    void clear();
    void updateParamValue (int paramIndex, float value);
    void updateBypass (bool bypassed);

    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged (juce::Slider*) override;

    // AudioProcessorParameter::Listener – plugin UI → our controls
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

private:
    struct ParamRow : public juce::Component
    {
        ParamRow (int paramIdx, juce::AudioProcessorParameter* param,
                  MidiLearnManager& mgr, int pluginIdx);
        ~ParamRow() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void updateLearnButton();
        void setFromNormalised (float v);

        int paramIndex, pluginIndex;
        juce::AudioProcessorParameter* parameter = nullptr;
        juce::Label nameLabel;
        juce::Label valueLabel;
        juce::Slider slider;
        juce::ToggleButton toggle;
        juce::TextButton learnButton { "LEARN" };
        juce::TextButton clearMidiButton { "x" };
        MidiLearnManager& learnManager;
        bool isToggle = false;
    };

    MidiLearnManager& midiLearn;
    juce::AudioPluginInstance* currentPlugin = nullptr;
    int currentPluginIndex = -1;

    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<ParamRow> rows;
    juce::Label titleLabel;
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton bypassLearnButton { "LEARN" };
    juce::TextButton bypassClearButton { "x" };
    juce::TextButton colourButton { "COLOR" };
    std::function<void()> bypassCallback;
    std::function<void()> colourCallback;

    static constexpr int kRowHeight = 96;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterPanel)
};
