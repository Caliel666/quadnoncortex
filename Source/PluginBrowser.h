#pragma once

#include <JuceHeader.h>
#include "PluginChain.h"

//==============================================================================
/** Full-screen modal list of available VST3 plugins.
    Large touch-friendly rows. Tap a plugin to add/replace it.
*/
class PluginBrowser : public juce::Component
{
public:
    PluginBrowser (PluginChain& chain);
    ~PluginBrowser() override = default;

    /** Show browser. If replaceIndex >= 0, the chosen plugin replaces that slot. */
    void show (int replaceIndex = -1);

    /** Refresh the list from the known plugin list (call after a scan). */
    void rebuildList();

    std::function<void()> onClosed;
    std::function<void(int /*newOrReplacedIndex*/)> onPluginChosen;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginChain& pluginChain;
    int replaceIndex = -1;

    juce::TextButton closeButton { "CLOSE" };
    juce::Label      titleLabel;

    juce::Viewport viewport;
    juce::Component content;

    struct PluginRow : public juce::Component
    {
        PluginRow (const juce::PluginDescription& d, PluginBrowser& owner);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        juce::PluginDescription desc;
        PluginBrowser& browser;
    };

    juce::OwnedArray<PluginRow> rows;

    void choosePlugin (const juce::PluginDescription& desc);

    static constexpr int kRowHeight = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBrowser)
};
