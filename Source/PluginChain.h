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

    int getLane (int index) const
    {
        if (juce::isPositiveAndBelow (index, plugins.size()))
            return plugins[(size_t) index].lane;
        return 0;
    }

    void setLane (int index, int lane)
    {
        const juce::ScopedLock sl (processLock);
        if (juce::isPositiveAndBelow (index, plugins.size()))
            plugins[(size_t) index].lane = juce::jmax (0, lane);
    }

    /** 0 = not in parallel region; else split index that owns this region. */
    int getSplitOwner (int index) const;

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
    void ensureNativePlugins();

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
        int lane = 0; // 0 = trunk; 1..N = parallel lane inside a split region
    };

    struct SpareInstance
    {
        juce::PluginDescription desc;
        std::unique_ptr<juce::AudioPluginInstance> instance;
    };

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList          knownPluginList;
    std::vector<Slot>              plugins;

    /** Plugin instances that were swapped out during a preset switch.
        Kept alive so they can be reused when the same plugin type is
        needed again.  Avoids destroying plugins (e.g. NAM Rig) whose
        internal background threads crash after the C++ object is deleted. */
    std::vector<SpareInstance> spareInstances;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    bool   prepared          = false;
    juce::AudioBuffer<float> tempBuffer;
    std::vector<juce::AudioBuffer<float>> laneBuffers;
    juce::CriticalSection processLock;
    std::atomic<bool> suspended { false };
    juce::Array<int> pendingRemoves;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChain)
};
