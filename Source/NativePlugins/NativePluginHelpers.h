#pragma once
#include <JuceHeader.h>

/** Shared helpers for built-in Native format plugins. */
namespace NativePluginHelpers
{
inline void addParam (juce::AudioProcessor& p, juce::AudioProcessorParameter* param)
{
    p.addParameter (param);
}

inline juce::PluginDescription makeDesc (const juce::String& name,
                                         const juce::String& id,
                                         const juce::String& category,
                                         int uniqueId)
{
    juce::PluginDescription d;
    d.name             = name;
    d.descriptiveName  = name + " (built-in)";
    d.pluginFormatName = "Native";
    d.category         = category;
    d.manufacturerName = "quadnoncortex";
    d.version          = "1.0";
    d.fileOrIdentifier = id;
    d.uniqueId         = uniqueId;
    d.isInstrument     = false;
    d.numInputChannels = 2;
    d.numOutputChannels = 2;
    return d;
}

inline bool isNativeId (const juce::PluginDescription& d, const juce::String& id, int uid, const juce::String& name)
{
    return d.fileOrIdentifier == id
        || d.uniqueId == uid
        || d.name == name;
}
}
