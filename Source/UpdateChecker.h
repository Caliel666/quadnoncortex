#pragma once
#include <JuceHeader.h>

/** Check GitHub releases and install Windows builds next to the running exe. */
class UpdateChecker
{
public:
    static constexpr const char* kRepoApi =
        "https://api.github.com/repos/Caliel666/quadnoncortex/releases/latest";
    static constexpr const char* kCurrentVersion = "1.0.0";

    struct Result
    {
        bool ok = false;
        bool updateAvailable = false;
        juce::String latestTag;
        juce::String message;
        juce::String downloadUrl;
        juce::String assetName;
    };

    static juce::String normalisedVersion (juce::String v)
    {
        if (v.startsWithIgnoreCase ("v"))
            v = v.substring (1);
        return v.trim();
    }

    static int compareVersions (juce::String a, juce::String b)
    {
        a = normalisedVersion (a);
        b = normalisedVersion (b);
        auto ap = juce::StringArray::fromTokens (a, ".", "");
        auto bp = juce::StringArray::fromTokens (b, ".", "");
        const int n = juce::jmax (ap.size(), bp.size());
        for (int i = 0; i < n; ++i)
        {
            const int ai = i < ap.size() ? ap[i].getIntValue() : 0;
            const int bi = i < bp.size() ? bp[i].getIntValue() : 0;
            if (ai < bi) return -1;
            if (ai > bi) return  1;
        }
        return 0;
    }

    static Result checkForUpdate()
    {
        Result r;
        juce::URL url (kRepoApi);
        // GitHub API wants a User-Agent
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd ("GET")
                        .withExtraHeaders ("User-Agent: quadnoncortex/1.0.0\r\nAccept: application/vnd.github+json\r\n")
                        .withConnectionTimeoutMs (12000);

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (opts));
        if (stream == nullptr)
        {
            r.message = "Could not reach GitHub. Check your internet connection.";
            return r;
        }

        const auto body = stream->readEntireStreamAsString();
        if (body.isEmpty() || body.contains ("\"message\": \"Not Found\""))
        {
            r.ok = true;
            r.message = "No releases published yet.";
            return r;
        }

        auto json = juce::JSON::parse (body);
        if (auto* obj = json.getDynamicObject())
        {
            r.latestTag = obj->getProperty ("tag_name").toString();
            if (r.latestTag.isEmpty())
            {
                r.message = "No release tags found.";
                r.ok = true;
                return r;
            }

            r.ok = true;
            if (compareVersions (kCurrentVersion, r.latestTag) >= 0)
            {
                r.updateAvailable = false;
                r.message = "You're on the latest version (" + juce::String (kCurrentVersion) + ").";
                return r;
            }

            r.updateAvailable = true;
            r.message = "Update available: " + r.latestTag + " (current " + juce::String (kCurrentVersion) + ")";

           #if JUCE_WINDOWS
            const juce::String prefer = ".exe";
           #elif JUCE_MAC
            const juce::String prefer = ".dmg";
           #else
            const juce::String prefer = ".AppImage";
           #endif

            if (auto* assets = obj->getProperty ("assets").getArray())
            {
                for (auto& a : *assets)
                {
                    if (auto* ao = a.getDynamicObject())
                    {
                        const auto name = ao->getProperty ("name").toString();
                        const auto dl = ao->getProperty ("browser_download_url").toString();
                        if (name.containsIgnoreCase (prefer) && dl.isNotEmpty())
                        {
                            r.assetName = name;
                            r.downloadUrl = dl;
                            break;
                        }
                    }
                }
                // Fallback: first asset
                if (r.downloadUrl.isEmpty() && ! assets->isEmpty())
                {
                    if (auto* ao = assets->getFirst().getDynamicObject())
                    {
                        r.assetName = ao->getProperty ("name").toString();
                        r.downloadUrl = ao->getProperty ("browser_download_url").toString();
                    }
                }
            }

            if (r.downloadUrl.isEmpty())
                r.message += " — no downloadable asset for this OS yet.";
        }
        else
        {
            r.message = "Failed to parse release info.";
        }
        return r;
    }

    /** Download to temp and schedule Windows replace + restart. */
    static juce::String installWindowsUpdate (const juce::String& downloadUrl, const juce::String& assetName)
    {
       #if ! JUCE_WINDOWS
        juce::ignoreUnused (downloadUrl, assetName);
        return "Auto-update is only supported on Windows.";
       #else
        if (downloadUrl.isEmpty())
            return "No download URL.";

        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto dir = exe.getParentDirectory();
        const auto tempExe = dir.getChildFile ("_update_download.exe");
        const auto bat = dir.getChildFile ("_apply_update.bat");

        juce::URL url (downloadUrl);
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd ("GET")
                        .withExtraHeaders ("User-Agent: quadnoncortex/1.0.0\r\n")
                        .withConnectionTimeoutMs (60000);

        std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
        if (in == nullptr)
            return "Download failed.";

        tempExe.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out (tempExe.createOutputStream());
        if (out == nullptr || ! out->openedOk())
            return "Could not write temporary file.";

        out->writeFromInputStream (*in, -1);
        out.reset();

        if (tempExe.getSize() < 10000)
        {
            tempExe.deleteFile();
            return "Downloaded file looks invalid.";
        }

        // Batch: wait for this process to exit, replace exe, relaunch, clean up
        const auto exePath = exe.getFullPathName();
        const auto tempPath = tempExe.getFullPathName();
        const auto batPath = bat.getFullPathName();

        juce::String script;
        script << "@echo off\r\n"
               << "timeout /t 2 /nobreak >nul\r\n"
               << ":retry\r\n"
               << "del /f /q \"" << exePath << "\" 2>nul\r\n"
               << "if exist \"" << exePath << "\" (\r\n"
               << "  timeout /t 1 /nobreak >nul\r\n"
               << "  goto retry\r\n"
               << ")\r\n"
               << "move /y \"" << tempPath << "\" \"" << exePath << "\"\r\n"
               << "start \"\" \"" << exePath << "\"\r\n"
               << "del /f /q \"%~f0\"\r\n";

        bat.replaceWithText (script);

        juce::File batFile (batPath);
        juce::ChildProcess proc;
        // Run detached via cmd
        juce::String cmd = "cmd.exe /C start \"\" /MIN \"" + batPath + "\"";
        juce::ignoreUnused (proc);
        system (cmd.toRawUTF8());

        return {};
       #endif
    }
};
