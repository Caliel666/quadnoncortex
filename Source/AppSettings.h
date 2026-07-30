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
            if (auto* dev = xml->getChildByName ("DEVICESETUP"))
                audioDeviceStateXml = std::make_unique<juce::XmlElement> (*dev);
            else if (auto* dev = xml->getChildByName ("AudioDeviceState"))
                audioDeviceStateXml = std::make_unique<juce::XmlElement> (*dev);

            inputGain  = (float) xml->getDoubleAttribute ("inputGain",  1.0);
            outputGain = (float) xml->getDoubleAttribute ("outputGain", 1.0);
            lastPreset = xml->getStringAttribute ("lastPreset");
            themeName = xml->getStringAttribute ("theme", "Dark");

            globalMidiMaps.clear();
            if (auto* maps = xml->getChildByName ("GlobalMidiMaps"))
                for (auto* e : maps->getChildIterator())
                    if (e->hasTagName ("Map"))
                        globalMidiMaps[e->getStringAttribute ("action")]
                            = e->getStringAttribute ("binding");

            midiInputEnabled.clear();
            midiInputsLoaded = false;
            if (auto* midis = xml->getChildByName ("MidiInputs"))
            {
                midiInputsLoaded = true;
                for (auto* e : midis->getChildIterator())
                    if (e->hasTagName ("Input"))
                        midiInputEnabled[e->getStringAttribute ("id")]
                            = e->getBoolAttribute ("enabled", true);
            }
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
        root.setAttribute ("theme", themeName);

        auto* maps = root.createNewChildElement ("GlobalMidiMaps");
        for (auto& p : globalMidiMaps)
        {
            auto* e = maps->createNewChildElement ("Map");
            e->setAttribute ("action", p.first);
            e->setAttribute ("binding", p.second);
        }

        auto* midis = root.createNewChildElement ("MidiInputs");
        for (auto& p : midiInputEnabled)
        {
            auto* e = midis->createNewChildElement ("Input");
            e->setAttribute ("id", p.first);
            e->setAttribute ("enabled", p.second ? 1 : 0);
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
    juce::String themeName { "Dark" };
    std::map<juce::String, juce::String> globalMidiMaps;
    /** identifier → enabled. Missing devices kept until next explicit save. */
    std::map<juce::String, bool> midiInputEnabled;
    bool midiInputsLoaded = false;

    void setMidiInputEnabled (const juce::String& id, bool en)
    {
        midiInputEnabled[id] = en;
    }

    /** On SAVE: drop ids that are no longer present on the system. */
    void pruneMidiInputs (const juce::StringArray& currentlyPresentIds)
    {
        for (auto it = midiInputEnabled.begin(); it != midiInputEnabled.end(); )
        {
            if (! currentlyPresentIds.contains (it->first))
                it = midiInputEnabled.erase (it);
            else
                ++it;
        }
    }

private:
    AppSettings() { load(); }
};
