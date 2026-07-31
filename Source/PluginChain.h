#pragma once
#include <JuceHeader.h>
#include <atomic>

class PluginChain
{
public:
    PluginChain();
    ~PluginChain();

    void prepare (double sampleRate, int maximumBlockSize);
    void releaseResources();
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    int  addPlugin (const juce::PluginDescription& desc, juce::String& error);
    void removePlugin (int index);
    /** Mark for removal; actual delete happens on message thread after audio unlock. */
    void requestRemovePlugin (int index);
    void movePlugin (int from, int to);
    bool replacePlugin (int index, const juce::PluginDescription& desc, juce::String& error);

    int getNumPlugins() const { return (int) plugins.size(); }

    juce::AudioPluginInstance* getPluginInstance (int index) const
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            return plugins[(size_t) index].instance.get();
        return nullptr;
    }

    juce::String getPluginName (int index) const
    {
        if (auto* p = getPluginInstance (index))
            return p->getName();
        return {};
    }

    bool isBypassed (int index) const
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            return plugins[(size_t) index].bypassed;
        return false;
    }

    void setBypass (int index, bool bypass)
    {
        const juce::ScopedLock sl (processLock);
        if (juce::isPositiveAndBelow (index, plugins.size()))
            plugins[(size_t) index].bypassed = bypass;
    }

    void toggleBypass (int index) { setBypass (index, ! isBypassed (index)); }

    bool isMono (int index) const
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            return plugins[(size_t) index].mono;
        return false;
    }

    void setMono (int index, bool mono)
    {
        const juce::ScopedLock sl (processLock);
        if (juce::isPositiveAndBelow (index, plugins.size()))
            plugins[(size_t) index].mono = mono;
    }

    void toggleMono (int index) { setMono (index, ! isMono (index)); }

    juce::Colour getBlockColour (int index) const
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            return plugins[(size_t) index].colour;
        return juce::Colour (0xff3a7ca5);
    }

    void setBlockColour (int index, juce::Colour c)
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            plugins[(size_t) index].colour = c;
    }

    std::function<void(int removedIndex)> onPluginRemoved;

    juce::AudioPluginFormatManager& getFormatManager()   { return formatManager; }
    juce::KnownPluginList&          getKnownPluginList() { return knownPluginList; }

    static juce::FileSearchPath getDefaultVST3Paths();
    static juce::File getAppDataDir();
    static juce::File getKnownPluginsFile();
    static juce::File getScanCacheFile();

    void loadKnownPluginsFromDisk();
    void saveKnownPluginsToDisk() const;

    void saveState (juce::XmlElement& parent) const;
    void loadState (const juce::XmlElement& parent);

    /** Must be called on the message thread before destroying plugins. */
    void closeAllEditors();

    /** While true, process() is a no-op (safe during preset load). */
    void setSuspended (bool s) { suspended.store (s); }
    bool isSuspended() const { return suspended.load(); }

private:
    struct Slot
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        bool bypassed = false;
        bool mono = false;   // true = mono output wiring, false = stereo (default)
        juce::Colour colour { 0xff3a7ca5 };
    };

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList          knownPluginList;
    std::vector<Slot>              plugins;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    bool   prepared          = false;
    juce::AudioBuffer<float> tempBuffer;
    juce::CriticalSection processLock;
    std::atomic<bool> suspended { false };
    juce::Array<int> pendingRemoves;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChain)
};
