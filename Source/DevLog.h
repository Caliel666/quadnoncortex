#pragma once
#include <JuceHeader.h>
#include <cstdarg>
#include <cstdio>

/**
 * File logger for Dev builds. Writes to data/debug.log next to the exe.
 * Always available; only verbose when QUADNONCORTEX_DEV is defined.
 */
class DevLog
{
public:
    static DevLog& get()
    {
        static DevLog instance;
        return instance;
    }

    static void log (const juce::String& msg)
    {
        get().write (msg);
    }

    static void logf (const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start (args, fmt);
       #if JUCE_WINDOWS
        vsnprintf_s (buf, sizeof (buf), _TRUNCATE, fmt, args);
       #else
        vsnprintf (buf, sizeof (buf), fmt, args);
       #endif
        va_end (args);
        get().write (juce::String (buf));
    }

    juce::File getLogFile() const { return logFile; }

private:
    DevLog()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                       .getParentDirectory()
                       .getChildFile ("data");
        dir.createDirectory();
        logFile = dir.getChildFile ("debug.log");

        // Rotate if huge
        if (logFile.getSize() > 2 * 1024 * 1024)
            logFile.deleteFile();

        write ("===== quadnoncortex log start =====");
       #if defined (QUADNONCORTEX_DEV)
        write ("Build: DEV (extra logging enabled)");
       #else
        write ("Build: Release");
       #endif
        write ("Version: " + juce::String (
           #if defined (QUADNONCORTEX_VERSION)
            QUADNONCORTEX_VERSION
           #else
            "?"
           #endif
        ));
    }

    void write (const juce::String& msg)
    {
        const juce::ScopedLock sl (lock);
        const auto line = juce::Time::getCurrentTime().toString (true, true, true, true)
                          + "  " + msg + "\n";
        logFile.appendText (line, false, false);
       #if defined (QUADNONCORTEX_DEV)
        juce::Logger::writeToLog (msg);
       #endif
    }

    juce::File logFile;
    juce::CriticalSection lock;
};

#if defined (QUADNONCORTEX_DEV)
 #define DEV_LOG(msg)       DevLog::log (msg)
 #define DEV_LOGF(...)      DevLog::logf (__VA_ARGS__)
#else
 #define DEV_LOG(msg)       ((void) 0)
 #define DEV_LOGF(...)      ((void) 0)
#endif
