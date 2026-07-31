#include "PluginChain.h"
#include "DevLog.h"

static void configureBuses (juce::AudioPluginInstance& inst, double sr, int bs, bool prepared)
{
    // Prefer stereo, fall back to mono for plugins that only support mono
    auto out = inst.getBusCount (false);
    auto in  = inst.getBusCount (true);

    if (out > 0)
    {
        auto* bus = inst.getBus (false, 0);
        if (bus != nullptr)
        {
            if (bus->isNumberOfChannelsSupported (2))
                bus->setCurrentLayout (juce::AudioChannelSet::stereo());
            else if (bus->isNumberOfChannelsSupported (1))
                bus->setCurrentLayout (juce::AudioChannelSet::mono());
        }
    }
    if (in > 0)
    {
        auto* bus = inst.getBus (true, 0);
        if (bus != nullptr)
        {
            if (bus->isNumberOfChannelsSupported (2))
                bus->setCurrentLayout (juce::AudioChannelSet::stereo());
            else if (bus->isNumberOfChannelsSupported (1))
                bus->setCurrentLayout (juce::AudioChannelSet::mono());
        }
    }

    inst.enableAllBuses();

    const int nIn  = inst.getTotalNumInputChannels();
    const int nOut = juce::jmax (1, inst.getTotalNumOutputChannels());
    // Always report stereo host side; process() will upmix mono → stereo
    inst.setPlayConfigDetails (juce::jmax (1, nIn), 2, sr, bs);
    if (prepared)
        inst.prepareToPlay (sr, bs);
}


PluginChain::PluginChain()
{
    juce::addDefaultFormatsToManager (formatManager);
}

PluginChain::~PluginChain()
{
    releaseResources();
    plugins.clear();
}

juce::File PluginChain::getAppDataDir()
{
    // Portable: data/ next to the executable
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                   .getParentDirectory()
                   .getChildFile ("data");
    dir.createDirectory();
    return dir;
}

juce::File PluginChain::getKnownPluginsFile()
{
    return getAppDataDir().getChildFile ("knownPlugins.xml");
}

juce::File PluginChain::getScanCacheFile()
{
    return getAppDataDir().getChildFile ("scanCache.xml");
}

juce::FileSearchPath PluginChain::getDefaultVST3Paths()
{
    juce::FileSearchPath paths;
   #if JUCE_WINDOWS
    paths.add (juce::File ("C:/Program Files/Common Files/VST3"));
    paths.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("VST3"));
    paths.add (juce::File ("C:/Program Files (x86)/Common Files/VST3"));
   #elif JUCE_MAC
    paths.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
    paths.add (juce::File ("~/Library/Audio/Plug-Ins/VST3"));
   #else
    paths.add (juce::File ("/usr/lib/vst3"));
    paths.add (juce::File ("/usr/local/lib/vst3"));
    paths.add (juce::File ("~/.vst3"));
   #endif
    return paths;
}

void PluginChain::loadKnownPluginsFromDisk()
{
    auto file = getKnownPluginsFile();
    if (! file.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (file))
        knownPluginList.recreateFromXml (*xml);
}

void PluginChain::saveKnownPluginsToDisk() const
{
    if (auto xml = knownPluginList.createXml())
        xml->writeTo (getKnownPluginsFile());
}

void PluginChain::prepare (double sampleRate, int maximumBlockSize)
{
    const juce::ScopedLock sl (processLock);
    currentSampleRate = sampleRate;
    currentBlockSize  = maximumBlockSize;
    prepared = true;
    tempBuffer.setSize (2, maximumBlockSize, false, false, true);

    for (auto& s : plugins)
    {
        if (s.instance != nullptr)
        {
            try
            {
                s.instance->setPlayConfigDetails (2, 2, sampleRate, maximumBlockSize);
                s.instance->prepareToPlay (sampleRate, maximumBlockSize);
            }
            catch (...) { s.bypassed = true; }
        }
    }
}

void PluginChain::releaseResources()
{
    const juce::ScopedLock sl (processLock);
    for (auto& s : plugins)
        if (s.instance != nullptr)
        {
            try { s.instance->releaseResources(); }
            catch (...) {}
        }
    prepared = false;
}

void PluginChain::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (suspended.load())
        return;

    const juce::ScopedLock sl (processLock);

    // Double-check after taking lock (loadState may have swapped under us)
    if (suspended.load())
        return;

    for (auto& s : plugins)
    {
        if (s.instance == nullptr || s.bypassed)
            continue;

        try
        {
            const int outCh = s.instance->getTotalNumOutputChannels();
            s.instance->processBlock (buffer, midi);

            if (outCh == 1 && buffer.getNumChannels() >= 2)
                buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
        }
        catch (...)
        {
            s.bypassed = true;
            DevLog::log ("PluginChain::process EXCEPTION — plugin auto-bypassed");
        }
    }
}

int PluginChain::addPlugin (const juce::PluginDescription& desc, juce::String& error)
{
    auto instance = formatManager.createPluginInstance (desc, currentSampleRate, currentBlockSize, error);
    if (instance == nullptr) return -1;

    configureBuses (*instance, currentSampleRate, currentBlockSize, prepared);

    Slot slot;
    slot.instance = std::move (instance);
    // Pick a colour from a palette based on index
    static const juce::uint32 palette[] = {
        0xffe67e22, 0xffe74c3c, 0xff9b59b6, 0xff3498db,
        0xff1abc9c, 0xff2ecc71, 0xfff1c40f, 0xffe91e63
    };
    slot.colour = juce::Colour (palette[plugins.size() % 8]);

    int index = 0;
    {
        const juce::ScopedLock sl (processLock);
        index = (int) plugins.size();
        plugins.push_back (std::move (slot));
    }
    return index;
}

void PluginChain::requestRemovePlugin (int index)
{
    if (! juce::isPositiveAndBelow (index, plugins.size()))
        return;

    // Immediately bypass so audio thread stops touching it
    {
        const juce::ScopedLock sl (processLock);
        if (juce::isPositiveAndBelow (index, plugins.size()))
            plugins[(size_t) index].bypassed = true;
    }

    // Defer actual destruction to the message thread so we are outside the audio callback
    juce::MessageManager::callAsync ([this, index]
    {
        removePlugin (index);
    });
}

void PluginChain::removePlugin (int index)
{
    std::unique_ptr<juce::AudioPluginInstance> doomed;

    {
        const juce::ScopedLock sl (processLock);

        if (! juce::isPositiveAndBelow (index, plugins.size()))
            return;

        // Pull instance out under lock, leave a null slot briefly then erase
        doomed = std::move (plugins[(size_t) index].instance);
        plugins.erase (plugins.begin() + index);
    }

    // Destroy outside the lock (plugins can take time / crash in their dtors)
    if (doomed != nullptr)
    {
        try
        {
            doomed->releaseResources();
            doomed->setPlayHead (nullptr);
        }
        catch (...) {}

        try
        {
            doomed.reset();
        }
        catch (...)
        {
            // Swallow destructor crashes from badly behaved plugins
            doomed.release(); // leak rather than crash – last resort
        }
    }

    if (onPluginRemoved)
        onPluginRemoved (index);
}



void PluginChain::movePlugin (int from, int to)
{
    const juce::ScopedLock sl (processLock);
    if (from == to) return;
    if (! juce::isPositiveAndBelow (from, plugins.size())) return;
    if (! juce::isPositiveAndBelow (to,   plugins.size())) return;
    auto item = std::move (plugins[(size_t) from]);
    plugins.erase (plugins.begin() + from);
    plugins.insert (plugins.begin() + to, std::move (item));
}

bool PluginChain::replacePlugin (int index, const juce::PluginDescription& desc, juce::String& error)
{
    if (! juce::isPositiveAndBelow (index, plugins.size())) return false;
    auto instance = formatManager.createPluginInstance (desc, currentSampleRate, currentBlockSize, error);
    if (instance == nullptr) return false;
    configureBuses (*instance, currentSampleRate, currentBlockSize, prepared);
    if (plugins[(size_t) index].instance != nullptr)
        plugins[(size_t) index].instance->releaseResources();
    plugins[(size_t) index].instance = std::move (instance);
    plugins[(size_t) index].bypassed = false;
    return true;
}

void PluginChain::saveState (juce::XmlElement& parent) const
{
    auto* chainXml = parent.createNewChildElement ("PluginChain");
    for (size_t i = 0; i < plugins.size(); ++i)
    {
        auto* p = plugins[i].instance.get();
        if (p == nullptr) continue;
        auto* pluginXml = chainXml->createNewChildElement ("Plugin");
        pluginXml->setAttribute ("name", p->getName());
        pluginXml->setAttribute ("bypassed", plugins[i].bypassed ? 1 : 0);
        pluginXml->setAttribute ("colour", (int) plugins[i].colour.getARGB());
        juce::PluginDescription desc;
        p->fillInPluginDescription (desc);
        pluginXml->addChildElement (desc.createXml().release());
        juce::MemoryBlock state;
        p->getStateInformation (state);
        pluginXml->createNewChildElement ("State")->addTextElement (state.toBase64Encoding());
    }
}

void PluginChain::closeAllEditors()
{
    // Delete every active editor BEFORE touching plugin instances.
    // Destroying NAM (and many VST3s) while an editor is open crashes hard.
    for (auto& s : plugins)
    {
        if (s.instance == nullptr) continue;
        if (auto* ed = s.instance->getActiveEditor())
        {
            DevLog::log ("closeAllEditors: deleting editor for " + s.instance->getName());
            delete ed; // AudioProcessorEditor dtor notifies the processor
        }
    }
}

void PluginChain::loadState (const juce::XmlElement& parent)
{
    DevLog::log ("PluginChain::loadState BEGIN (suspend on)");
    suspended.store (true);

    closeAllEditors();

    // Switching presets usually changes only a plug-in's state. Reusing an
    // identical chain avoids unloading/reloading VST3 instances; some plug-ins
    // (including NAM Rig) are not reliable when repeatedly destroyed after an
    // editor has been created.
    std::vector<juce::XmlElement*> presetPlugins;
    std::vector<juce::PluginDescription> presetDescriptions;
    if (auto* chainXml = parent.getChildByName ("PluginChain"))
    {
        for (auto* pluginXml : chainXml->getChildIterator())
        {
            if (! pluginXml->hasTagName ("Plugin")) continue;
            if (auto* descXml = pluginXml->getChildByName ("PLUGIN"))
            {
                juce::PluginDescription desc;
                if (desc.loadFromXml (*descXml))
                {
                    presetPlugins.push_back (pluginXml);
                    presetDescriptions.push_back (std::move (desc));
                }
            }
        }
    }

    bool canReuseExistingChain = presetPlugins.size() == plugins.size();
    if (canReuseExistingChain)
    {
        for (size_t i = 0; i < plugins.size(); ++i)
        {
            if (plugins[i].instance == nullptr)
            {
                canReuseExistingChain = false;
                break;
            }

            juce::PluginDescription current;
            plugins[i].instance->fillInPluginDescription (current);
            if (! current.isDuplicateOf (presetDescriptions[i]))
            {
                canReuseExistingChain = false;
                break;
            }
        }
    }

    if (canReuseExistingChain)
    {
        DevLog::log ("PluginChain::loadState reusing "
                     + juce::String ((int) plugins.size()) + " existing plugin(s)");

        for (size_t i = 0; i < plugins.size(); ++i)
        {
            auto& slot = plugins[i];
            auto* pluginXml = presetPlugins[i];
            slot.bypassed = pluginXml->getBoolAttribute ("bypassed");
            if (pluginXml->hasAttribute ("colour"))
                slot.colour = juce::Colour ((juce::uint32) pluginXml->getIntAttribute ("colour"));

            if (auto* stateXml = pluginXml->getChildByName ("State"))
            {
                juce::MemoryBlock state;
                state.fromBase64Encoding (stateXml->getAllSubText());
                try
                {
                    slot.instance->setStateInformation (state.getData(), (int) state.getSize());
                    DevLog::log ("  state restored (" + juce::String ((int) state.getSize()) + " bytes)");
                }
                catch (...)
                {
                    DevLog::log ("  setStateInformation EXCEPTION");
                }
            }

            if (prepared)
            {
                try
                {
                    configureBuses (*slot.instance, currentSampleRate, currentBlockSize, true);
                    DevLog::log ("  re-prepared after state");
                }
                catch (...)
                {
                    DevLog::log ("  re-prepare EXCEPTION");
                }
            }
        }

        DevLog::log ("PluginChain::loadState END — reused existing chain");
        return;
    }

    // Let any in-flight process() exit the critical section
    {
        const juce::ScopedLock sl (processLock);
    }

    std::vector<Slot> doomed;
    {
        const juce::ScopedLock sl (processLock);
        doomed.swap (plugins);
        plugins.clear();
        DevLog::log ("PluginChain::loadState emptied chain, destroying "
                     + juce::String ((int) doomed.size()) + " old plugin(s)");
    }

    for (size_t i = 0; i < doomed.size(); ++i)
    {
        auto& s = doomed[i];
        if (s.instance == nullptr) continue;
        const auto name = s.instance->getName();
        DevLog::log ("  releasing [" + juce::String ((int) i) + "] " + name);
        try
        {
            s.instance->releaseResources();
            s.instance->setPlayHead (nullptr);
        }
        catch (...)
        {
            DevLog::log ("  releaseResources EXCEPTION on " + name);
        }
        try
        {
            s.instance.reset();
            DevLog::log ("  destroyed " + name);
        }
        catch (...)
        {
            DevLog::log ("  destructor EXCEPTION on " + name + " — leaking instance");
            s.instance.release();
        }
    }
    doomed.clear();

    if (auto* chainXml = parent.getChildByName ("PluginChain"))
    {
        for (auto* pluginXml : chainXml->getChildIterator())
        {
            if (! pluginXml->hasTagName ("Plugin")) continue;
            if (auto* descXml = pluginXml->getChildByName ("PLUGIN"))
            {
                juce::PluginDescription desc;
                if (desc.loadFromXml (*descXml))
                {
                    juce::String error;
                    DevLog::log ("  loading plugin: " + desc.name);
                    const int idx = addPlugin (desc, error);
                    if (idx < 0)
                    {
                        DevLog::log ("  FAILED to load " + desc.name + ": " + error);
                        continue;
                    }
                    DevLog::log ("  loaded idx=" + juce::String (idx) + " " + desc.name);
                    {
                        const juce::ScopedLock sl (processLock);
                        if (juce::isPositiveAndBelow (idx, (int) plugins.size()))
                        {
                            plugins[(size_t) idx].bypassed = pluginXml->getBoolAttribute ("bypassed");
                            // Keep suspended slots silent until fully configured
                            if (pluginXml->hasAttribute ("colour"))
                                plugins[(size_t) idx].colour =
                                    juce::Colour ((juce::uint32) pluginXml->getIntAttribute ("colour"));
                        }
                    }
                    if (auto* stateXml = pluginXml->getChildByName ("State"))
                    {
                        juce::MemoryBlock state;
                        state.fromBase64Encoding (stateXml->getAllSubText());
                        if (auto* inst = getPluginInstance (idx))
                        {
                            try
                            {
                                inst->setStateInformation (state.getData(), (int) state.getSize());
                                DevLog::log ("  state restored (" + juce::String ((int) state.getSize()) + " bytes)");
                            }
                            catch (...)
                            {
                                DevLog::log ("  setStateInformation EXCEPTION");
                            }
                            if (prepared)
                            {
                                try
                                {
                                    configureBuses (*inst, currentSampleRate, currentBlockSize, true);
                                    DevLog::log ("  re-prepared after state");
                                }
                                catch (...)
                                {
                                    DevLog::log ("  re-prepare EXCEPTION");
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Leave suspended=true — caller (loadPreset) resumes after prepare()
    DevLog::log ("PluginChain::loadState END — " + juce::String (getNumPlugins())
                 + " plugin(s), still suspended until prepare");
}
