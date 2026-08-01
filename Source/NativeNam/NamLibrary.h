#pragma once
#include <JuceHeader.h>

/** Local NAM / IR library under <exe>/data/NAM/{Pedals,Amps,Cabs}. */
class NamLibrary
{
public:
    enum class Kind { Pedal, Amp, Cab };

    struct Entry
    {
        juce::String name;
        juce::File   file;
        Kind         kind = Kind::Amp;
        juce::String source;
    };

    static juce::File rootDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                     .getParentDirectory()
                     .getChildFile ("data")
                     .getChildFile ("NAM");
        d.createDirectory();
        d.getChildFile ("Pedals").createDirectory();
        d.getChildFile ("Amps").createDirectory();
        d.getChildFile ("Cabs").createDirectory();
        return d;
    }

    static juce::File folderFor (Kind k)
    {
        auto r = rootDir();
        switch (k)
        {
            case Kind::Pedal: return r.getChildFile ("Pedals");
            case Kind::Amp:   return r.getChildFile ("Amps");
            case Kind::Cab:   return r.getChildFile ("Cabs");
        }
        return r.getChildFile ("Amps");
    }

    static const char* folderLabel (Kind k)
    {
        switch (k)
        {
            case Kind::Pedal: return "Pedals";
            case Kind::Amp:   return "Amps";
            case Kind::Cab:   return "Cabs";
        }
        return "Amps";
    }

    /** Map TONE3000 gear + format to a library folder.
        cab / ir -> Cabs; pedal -> Pedals; amp / amp-cab / full-rig -> Amps. */
    static Kind kindFromGear (const juce::String& gear, const juce::String& format = {})
    {
        const auto g = gear.trim().toLowerCase();
        const auto f = format.trim().toLowerCase();

        if (f == "ir" || f.contains ("wav") || f.contains ("impulse"))
            return Kind::Cab;

        if (g == "cab" || g == "ir" || g == "space")
            return Kind::Cab;

        // "amp-cab" and "full-rig" contain "cab" but are amp folders
        if (g == "pedal" || g == "outboard")
            return Kind::Pedal;

        if (g == "amp" || g == "amp-cab" || g == "full-rig" || g == "experimental")
            return Kind::Amp;

        if (g.contains ("pedal"))
            return Kind::Pedal;
        if (g.contains ("cab") && ! g.contains ("amp"))
            return Kind::Cab;

        return Kind::Amp;
    }

    /** Only scans the matching folder — never mixes Pedals/Amps/Cabs. */
    static juce::Array<Entry> scan (Kind kind)
    {
        juce::Array<Entry> out;
        auto dir = folderFor (kind);
        dir.createDirectory();

        const juce::String patterns = (kind == Kind::Cab)
            ? "*.wav;*.WAV;*.aif;*.aiff;*.flac;*.nam;*.NAM"
            : "*.nam;*.NAM";

        // Non-recursive: only files directly in this folder
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, patterns))
        {
            Entry e;
            e.name = f.getFileNameWithoutExtension();
            e.file = f;
            e.kind = kind;
            e.source = "local";
            out.add (e);
        }

        struct Cmp {
            static int compareElements (const Entry& a, const Entry& b)
            {
                return a.name.compareIgnoreCase (b.name);
            }
        };
        Cmp cmp;
        out.sort (cmp);
        return out;
    }

    static juce::File importFile (const juce::File& src, Kind kind)
    {
        if (! src.existsAsFile()) return {};
        auto destDir = folderFor (kind);
        destDir.createDirectory();
        auto dest = destDir.getChildFile (src.getFileName());
        if (src.getFullPathName() == dest.getFullPathName())
            return dest;
        src.copyFileTo (dest);
        return dest;
    }
};
