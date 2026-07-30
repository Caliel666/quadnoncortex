#pragma once
#include <JuceHeader.h>

//==============================================================================
/** All user data lives next to the .exe (portable). */
class AppSettings
{
public:
    static AppSettings& get()
    {
        static AppSettings instance;
        return instance;
    }

    /** Folder containing the running executable. */
    static juce::File getExeDir()
    {
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                   .getParentDirectory();
    }

    static juce::File getDataDir()
    {
        auto d = getExeDir().getChildFile ("data");
        d.createDirectory();
        return d;
    }

    juce::File getSettingsFile() const { return getDataDir().getChildFile ("settings.xml"); }
    juce::File getPresetsDir() const
    {
        auto d = getDataDir().getChildFile ("Presets");
        d.createDirectory();
        return d;
    }

    void load()
    {
        auto f = getSettingsFile();
        if (! f.existsAsFile()) return;
        if (auto xml = juce::XmlDocument::parse (f))
        {
            if (auto* audio = xml->getChildByName ("DEVICESETUP"))
                audioDeviceStateXml = std::make_unique<juce::XmlElement> (*audio);
            else if (auto* audio = xml->getChildByName ("AudioDeviceState"))
                audioDeviceStateXml = std::make_unique<juce::XmlElement> (*audio);

            inputGain  = (float) xml->getDoubleAttribute ("inputGain",  1.0);
            outputGain = (float) xml->getDoubleAttribute ("outputGain", 1.0);
            lastPreset = xml->getStringAttribute ("lastPreset");

            globalMidiMaps.clear();
            if (auto* maps = xml->getChildByName ("GlobalMidiMaps"))
                for (auto* e : maps->getChildIterator())
                    if (e->hasTagName ("Map"))
                        globalMidiMaps[e->getStringAttribute ("action")]
                            = e->getStringAttribute ("binding");
        }
    }

    void save()
    {
        juce::XmlElement root ("QuadnonCortexSettings");
        if (audioDeviceStateXml != nullptr)
            root.addChildElement (new juce::XmlElement (*audioDeviceStateXml));
        root.setAttribute ("inputGain",  inputGain);
        root.setAttribute ("outputGain", outputGain);
        root.setAttribute ("lastPreset", lastPreset);

        auto* maps = root.createNewChildElement ("GlobalMidiMaps");
        for (auto& p : globalMidiMaps)
        {
            auto* e = maps->createNewChildElement ("Map");
            e->setAttribute ("action", p.first);
            e->setAttribute ("binding", p.second);
        }

        root.writeTo (getSettingsFile());
    }

    void storeAudioDeviceState (juce::AudioDeviceManager& dm)
    {
        audioDeviceStateXml = dm.createStateXml();
        save();
    }

    std::unique_ptr<juce::XmlElement> audioDeviceStateXml;
    float inputGain  = 1.0f;
    float outputGain = 1.0f;
    juce::String lastPreset;
    std::map<juce::String, juce::String> globalMidiMaps;

private:
    AppSettings() { load(); }
};
