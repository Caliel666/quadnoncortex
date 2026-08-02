#include "PluginChain.h"
#include "DevLog.h"
#include "NativeNam/NativeNamProcessor.h"
#include "NativePlugins/ParametricEQ/ParametricEQProcessor.h"
#include "NativePlugins/Echo/EchoProcessor.h"
#include "NativePlugins/Reverb/ReverbProcessor.h"
#include "NativePlugins/Compressor/CompressorProcessor.h"
#include "NativePlugins/Limiter/LimiterProcessor.h"
#include "NativePlugins/Denoiser/DenoiserProcessor.h"

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
    formatManager.addFormat (new NativeNamFormat());
}

void PluginChain::ensureNativePlugins()
{
    const juce::PluginDescription natives[] = {
        NativeNamProcessor::makeDescription(),
        ParametricEQProcessor::makeDescription(),
        EchoProcessor::makeDescription(),
        ReverbProcessor::makeDescription(),
        CompressorProcessor::makeDescription(),
        LimiterProcessor::makeDescription(),
        DenoiserProcessor::makeDescription(),
    };

    // Drop previous native entries so we always re-register current set
    juce::OwnedArray<juce::PluginDescription> keep;
    for (auto& t : knownPluginList.getTypes())
    {
        bool isNative = (t.pluginFormatName == "Native" || t.pluginFormatName == "Internal"
                         || t.fileOrIdentifier.startsWith ("internal://"));
        if (! isNative)
            keep.add (new juce::PluginDescription (t));
    }
    knownPluginList.clear();
    for (auto* t : keep)
        knownPluginList.addType (*t);

    for (auto& d : natives)
        knownPluginList.addType (d);

    saveKnownPluginsToDisk();
    DevLog::log ("Registered " + juce::String ((int) (sizeof (natives) / sizeof (natives[0])))
                 + " native plugins");
}

PluginChain::~PluginChain()
{
    releaseResources();
    plugins.clear();
    // Release all spare instances — let the OS reclaim on exit.
    // We do NOT call delete because plugins like NAM Rig leave
    // background threads running that crash after the C++ object is gone.
    for (auto& sp : spareInstances)
        if (sp.instance != nullptr)
            sp.instance.release();
    spareInstances.clear();
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
    // Linux / generic Unix — multi-arch + user dirs (PipeWire/desktop installs)
    paths.add (juce::File ("/usr/lib/vst3"));
    paths.add (juce::File ("/usr/lib/x86_64-linux-gnu/vst3"));
    paths.add (juce::File ("/usr/lib/aarch64-linux-gnu/vst3"));
    paths.add (juce::File ("/usr/lib/arm-linux-gnueabihf/vst3"));
    paths.add (juce::File ("/usr/local/lib/vst3"));
    paths.add (juce::File ("/usr/local/lib/x86_64-linux-gnu/vst3"));
    paths.add (juce::File ("/usr/local/lib/aarch64-linux-gnu/vst3"));
    paths.add (juce::File ("~/.vst3"));
    paths.add (juce::File ("~/.local/lib/vst3"));
    {
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        paths.add (home.getChildFile (".vst3"));
        paths.add (home.getChildFile (".local/lib/vst3"));
    }
   #endif
    return paths;
}

void PluginChain::loadKnownPluginsFromDisk()
{
    auto file = getKnownPluginsFile();
    if (file.existsAsFile())
        if (auto xml = juce::XmlDocument::parse (file))
            knownPluginList.recreateFromXml (*xml);
    ensureNativePlugins();
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
            if (s.mono && buffer.getNumChannels() >= 2)
            {
                // Mono mode: sum L+R into ch0, feed identical L+R to plugin,
                // then take ch0 output and copy to both channels.
                tempBuffer.setSize (2, buffer.getNumSamples(), false, false, true);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const float sum = (buffer.getSample (0, i) + buffer.getSample (1, i)) * 0.5f;
                    tempBuffer.setSample (0, i, sum);
                    tempBuffer.setSample (1, i, sum);
                }
                s.instance->processBlock (tempBuffer, midi);
                // Copy mono output (ch0) to both channels
                buffer.copyFrom (0, 0, tempBuffer, 0, 0, buffer.getNumSamples());
                buffer.copyFrom (1, 0, tempBuffer, 0, 0, buffer.getNumSamples());
            }
            else
            {
                const int outCh = s.instance->getTotalNumOutputChannels();
                s.instance->processBlock (buffer, midi);

                if (outCh == 1 && buffer.getNumChannels() >= 2)
                    buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
            }
        }
        catch (...)
        {
            s.bypassed = true;
            DevLog::log ("PluginChain::process EXCEPTION — plugin auto-bypassed");
        }
    }
}


static std::unique_ptr<juce::AudioPluginInstance> createNativeInstance (
    const juce::PluginDescription& desc, double sr, int bs)
{
    auto prepare = [sr, bs] (auto proc) -> std::unique_ptr<juce::AudioPluginInstance>
    {
        if (sr > 0.0 && bs > 0)
            proc->prepareToPlay (sr, bs);
        return proc;
    };

    if (desc.fileOrIdentifier == "internal://native-nam"
        || desc.uniqueId == 0x4E414D32
        || desc.name == NativeNamProcessor::kName)
        return prepare (std::make_unique<NativeNamProcessor>());

    if (desc.fileOrIdentifier == ParametricEQProcessor::kId
        || desc.uniqueId == ParametricEQProcessor::kUid
        || desc.name == ParametricEQProcessor::kName)
        return prepare (std::make_unique<ParametricEQProcessor>());

    if (desc.fileOrIdentifier == EchoProcessor::kId
        || desc.uniqueId == EchoProcessor::kUid
        || desc.name == EchoProcessor::kName)
        return prepare (std::make_unique<EchoProcessor>());

    if (desc.fileOrIdentifier == ReverbProcessor::kId
        || desc.uniqueId == ReverbProcessor::kUid
        || desc.name == ReverbProcessor::kName)
        return prepare (std::make_unique<ReverbProcessor>());

    if (desc.fileOrIdentifier == CompressorProcessor::kId
        || desc.uniqueId == CompressorProcessor::kUid
        || desc.name == CompressorProcessor::kName)
        return prepare (std::make_unique<CompressorProcessor>());

    if (desc.fileOrIdentifier == LimiterProcessor::kId
        || desc.uniqueId == LimiterProcessor::kUid
        || desc.name == LimiterProcessor::kName)
        return prepare (std::make_unique<LimiterProcessor>());

    if (desc.fileOrIdentifier == DenoiserProcessor::kId
        || desc.uniqueId == DenoiserProcessor::kUid
        || desc.name == DenoiserProcessor::kName)
        return prepare (std::make_unique<DenoiserProcessor>());

    return nullptr;
}

static std::unique_ptr<juce::AudioPluginInstance> createInstanceForDesc (
    juce::AudioPluginFormatManager& formatManager,
    const juce::PluginDescription& desc,
    double sr, int bs, juce::String& error)
{
    // Built-in Native plugins — never require a file / external format match
    if (desc.pluginFormatName == "Native" || desc.pluginFormatName == "Internal"
        || desc.fileOrIdentifier.startsWith ("internal://"))
    {
        if (auto proc = createNativeInstance (desc, sr, bs))
        {
            error.clear();
            return proc;
        }
    }

    auto inst = formatManager.createPluginInstance (desc, sr, bs, error);
    if (inst == nullptr && error.isEmpty())
        error = "No compatible plug-in format exists for this plug-in";
    return inst;
}

int PluginChain::addPlugin (const juce::PluginDescription& desc, juce::String& error)
{
    auto instance = createInstanceForDesc (formatManager, desc, currentSampleRate, currentBlockSize, error);
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

    // Move to spare cache instead of destroying immediately.
    // Some plugins (NAM Rig) have internal async ops that crash if the
    // instance is deleted.  The spare cache allows reuse without destruction.
    if (doomed != nullptr)
    {
        try
        {
            doomed->releaseResources();
            doomed->setPlayHead (nullptr);
        }
        catch (...) {}
        SpareInstance sp;
        doomed->fillInPluginDescription (sp.desc);
        sp.instance = std::move (doomed);
        spareInstances.push_back (std::move (sp));
        DevLog::log ("removePlugin: instance cached as spare");
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
    auto instance = createInstanceForDesc (formatManager, desc, currentSampleRate, currentBlockSize, error);
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
        pluginXml->setAttribute ("mono", plugins[i].mono ? 1 : 0);
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
        // Snapshot pointer — getActiveEditor may change during delete
        juce::AudioProcessorEditor* ed = s.instance->getActiveEditor();
        if (ed == nullptr) continue;
        DevLog::log ("closeAllEditors: deleting editor for " + s.instance->getName());
        try
        {
            delete ed; // AudioProcessorEditor dtor notifies the processor
        }
        catch (...)
        {
            DevLog::log ("closeAllEditors: editor destructor threw — continuing");
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
            slot.mono = pluginXml->getBoolAttribute ("mono");
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

    // Move doomed instances to the spare cache instead of destroying.
    // The cache allows reuse when the same plugin type is needed again,
    // avoiding destruction of plugins (e.g. NAM Rig) whose internal
    // background threads crash after the C++ object is deleted.
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
        SpareInstance sp;
        s.instance->fillInPluginDescription (sp.desc);
        sp.instance = std::move (s.instance);
        spareInstances.push_back (std::move (sp));
        DevLog::log ("  spare: " + name + " cached");
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
                    // Try to reuse a cached spare instance
                    int spareIdx = -1;
                    for (size_t si = 0; si < spareInstances.size(); ++si)
                    {
                        if (spareInstances[si].instance != nullptr
                            && desc.isDuplicateOf (spareInstances[si].desc))
                        {
                            spareIdx = (int) si;
                            break;
                        }
                    }

                    int idx = -1;
                    if (spareIdx >= 0)
                    {
                        // Reuse cached instance
                        auto spare = std::move (spareInstances[(size_t) spareIdx].instance);
                        spareInstances.erase (spareInstances.begin() + spareIdx);
                        configureBuses (*spare, currentSampleRate, currentBlockSize, false);
                        Slot slot;
                        slot.instance = std::move (spare);
                        {
                            const juce::ScopedLock sl (processLock);
                            idx = (int) plugins.size();
                            plugins.push_back (std::move (slot));
                        }
                        DevLog::log ("  loaded idx=" + juce::String (idx) + " " + desc.name + " (from spare cache)");
                    }
                    else
                    {
                        juce::String error;
                        DevLog::log ("  loading plugin: " + desc.name);
                        idx = addPlugin (desc, error);
                        if (idx < 0)
                        {
                            DevLog::log ("  FAILED to load " + desc.name + ": " + error);
                            continue;
                        }
                        DevLog::log ("  loaded idx=" + juce::String (idx) + " " + desc.name);
                    }
                    {
                        const juce::ScopedLock sl (processLock);
                        if (juce::isPositiveAndBelow (idx, (int) plugins.size()))
                        {
                            plugins[(size_t) idx].bypassed = pluginXml->getBoolAttribute ("bypassed");
                            plugins[(size_t) idx].mono = pluginXml->getBoolAttribute ("mono");
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
