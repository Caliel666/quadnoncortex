#pragma once
#include <JuceHeader.h>
#include "MidiLearnManager.h"
#include "NativeNam/NativeNamPanel.h"

/** Horizontal drag adjusts value; vertical drag scrolls the parameter list (touch-friendly). */
class TouchSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent& e) override
    {
        start = e.position;
        scrolling = false;
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto d = e.position - start;
        if (! scrolling && std::abs (d.y) > std::abs (d.x) && std::abs (d.y) > 14.0f)
            scrolling = true;
        if (scrolling)
        {
            if (auto* v = findParentComponentOfClass<juce::Viewport>())
            {
                auto pos = v->getViewPosition();
                v->setViewPosition (pos.x, pos.y - (int) (e.position.y - start.y));
                start = e.position;
            }
            return;
        }
        juce::Slider::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! scrolling)
            juce::Slider::mouseUp (e);
        scrolling = false;
    }
private:
    juce::Point<float> start;
    bool scrolling = false;
};


class ParameterPanel : public juce::Component,
                       public juce::Slider::Listener,
                       public juce::AudioProcessorParameter::Listener
{
public:
    ParameterPanel (MidiLearnManager& learnMgr);
    ~ParameterPanel() override;

    void setPlugin (juce::AudioPluginInstance* instance, int pluginIndex,
                    bool bypassed, bool mono, std::function<void()> onBypass,
                    std::function<void()> onMonoToggle,
                    std::function<void()> onColour = nullptr,
                    std::function<void()> onOpenEditor = nullptr);
    void clear();
    void updateParamValue (int paramIndex, float value);
    void updateBypass (bool bypassed);
    void updateMono (bool mono);
    void refreshMidiButtons();

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
        TouchSlider slider;
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
    juce::TextButton monoButton { "STEREO" };
    juce::TextButton bypassLearnButton { "LEARN" };
    juce::TextButton bypassClearButton { "x" };
    juce::TextButton colourButton { "COLOR" };
    juce::TextButton openEditorButton { "UI" };
    std::function<void()> bypassCallback;
    std::function<void()> monoToggleCallback;
    std::function<void()> colourCallback;
    std::function<void()> openEditorCallback;
    std::unique_ptr<NativeNamPanel> nativeNamPanel;

    static constexpr int kRowHeight = 96;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterPanel)
};
