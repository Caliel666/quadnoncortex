#pragma once
#include <JuceHeader.h>

class PluginBlockComponent : public juce::Component
{
public:
    PluginBlockComponent (int index, const juce::String& name);
    ~PluginBlockComponent() override = default;

    void setPluginIndex (int idx) { pluginIndex = idx; }
    int  getPluginIndex() const   { return pluginIndex; }
    void setPluginName  (const juce::String& n);
    void setSelected    (bool s);
    void setBypassed    (bool b);
    void setBlockColour (juce::Colour c);
    void setDeleteHover (bool h);
    void setDragging (bool d);

    std::function<void(int)> onSelected;
    std::function<void(int)> onBypassToggled;
    std::function<void(int)> onDoubleTap;
    std::function<void(int)> onColourRequested;
    std::function<void(int)> onDragStarted;
    std::function<void()>    onDragEnded;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int  pluginIndex = 0;
    juce::String pluginName;
    bool selected  = false;
    bool bypassed  = false;
    bool dragOver  = false;
    bool deleteHover = false;
    bool draggingBypass = false;
    bool didStartDrag = false;
    bool dragging = false;
    juce::Colour blockColour { 0xff3a7ca5 };
    juce::Rectangle<int> bypassBounds;
    juce::Point<int> dragStart;
    juce::Time lastTapTime;
    int tapCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBlockComponent)
};
