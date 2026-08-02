#pragma once
#include <JuceHeader.h>

/**
 * Check GitHub releases, download the matching OS/arch zip, extract the
 * binary next to the running executable, and schedule a restart.
 *
 * Expected release assets (as produced by CI):
 *   quadnoncortex-windows-x64.zip
 *   quadnoncortex-linux-x64.zip
 *   quadnoncortex-linux-arm64.zip
 */
class UpdateChecker
{
public:
    static constexpr const char* kRepoApi =
        "https://api.github.com/repos/Caliel666/quadnoncortex/releases/latest";

    static juce::String currentVersion()
    {
       #if defined (QUADNONCORTEX_VERSION)
        return juce::String (QUADNONCORTEX_VERSION);
       #elif defined (JUCE_APPLICATION_VERSION_STRING)
        return juce::String (JUCE_APPLICATION_VERSION_STRING);
       #else
        return "1.1.1";
       #endif
    }

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

    /** Asset name substring for this build (order = preference). */
    static juce::StringArray preferredAssetTokens()
    {
        juce::StringArray tokens;
       #if JUCE_WINDOWS
        tokens.add ("windows-x64");
        tokens.add ("windows");
        tokens.add (".exe");
       #elif JUCE_MAC
        tokens.add ("macos");
        tokens.add ("darwin");
        tokens.add (".dmg");
        tokens.add (".pkg");
       #else
        // Linux — distinguish arm64 vs x64
        #if defined (__aarch64__) || defined (_M_ARM64)
        tokens.add ("linux-arm64");
        tokens.add ("aarch64");
        tokens.add ("arm64");
        #else
        tokens.add ("linux-x64");
        tokens.add ("linux-amd64");
        tokens.add ("x86_64");
        #endif
        tokens.add ("linux");
        tokens.add (".AppImage");
       #endif
        return tokens;
    }

    static bool assetMatches (const juce::String& name, const juce::StringArray& tokens)
    {
        for (const auto& t : tokens)
            if (name.containsIgnoreCase (t))
                return true;
        return false;
    }

    static Result checkForUpdate()
    {
        Result r;
        juce::URL url (kRepoApi);
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd ("GET")
                        .withExtraHeaders ("User-Agent: quadnoncortex/" + currentVersion()
                                           + "\r\nAccept: application/vnd.github+json\r\n")
                        .withConnectionTimeoutMs (12000);

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (opts));
        if (stream == nullptr)
        {
            r.message = "Could not reach GitHub. Check your internet connection.";
            return r;
        }

        const auto body = stream->readEntireStreamAsString();
        if (body.isEmpty() || body.containsIgnoreCase ("\"message\": \"Not Found\""))
        {
            r.message = "No releases found on GitHub yet.";
            return r;
        }

        auto parsed = juce::JSON::parse (body);
        if (auto* obj = parsed.getDynamicObject())
        {
            r.ok = true;
            r.latestTag = obj->getProperty ("tag_name").toString();
            if (r.latestTag.isEmpty())
                r.latestTag = obj->getProperty ("name").toString();

            if (r.latestTag.isEmpty())
            {
                r.message = "Could not read release tag.";
                return r;
            }

            if (compareVersions (currentVersion(), r.latestTag) >= 0)
            {
                r.updateAvailable = false;
                r.message = "You're on the latest version (" + currentVersion() + ").";
                return r;
            }

            r.updateAvailable = true;
            r.message = "Update available: " + r.latestTag + " (current " + currentVersion() + ")";

            const auto prefer = preferredAssetTokens();
            if (auto* assets = obj->getProperty ("assets").getArray())
            {
                // First pass: preferred tokens
                for (const auto& token : prefer)
                {
                    for (auto& a : *assets)
                    {
                        if (auto* ao = a.getDynamicObject())
                        {
                            const auto name = ao->getProperty ("name").toString();
                            const auto dl = ao->getProperty ("browser_download_url").toString();
                            if (name.containsIgnoreCase (token) && dl.isNotEmpty())
                            {
                                r.assetName = name;
                                r.downloadUrl = dl;
                                break;
                            }
                        }
                    }
                    if (r.downloadUrl.isNotEmpty())
                        break;
                }

                // Fallback: first zip/exe
                if (r.downloadUrl.isEmpty())
                {
                    for (auto& a : *assets)
                    {
                        if (auto* ao = a.getDynamicObject())
                        {
                            const auto name = ao->getProperty ("name").toString();
                            const auto dl = ao->getProperty ("browser_download_url").toString();
                            if (dl.isNotEmpty()
                                && (name.endsWithIgnoreCase (".zip")
                                    || name.endsWithIgnoreCase (".exe")
                                    || ! name.contains (".")))
                            {
                                r.assetName = name;
                                r.downloadUrl = dl;
                                break;
                            }
                        }
                    }
                }
            }

            if (r.downloadUrl.isEmpty())
                r.message += " — no downloadable asset for this OS/arch.";
        }
        else
        {
            r.message = "Failed to parse release info.";
        }
        return r;
    }

    /**
     * Download the release asset, extract the app binary if it's a zip,
     * replace the running executable, and relaunch.
     * Returns empty string on success (caller should quit), or an error message.
     */
    static juce::String installUpdate (const juce::String& downloadUrl, const juce::String& assetName)
    {
        if (downloadUrl.isEmpty())
            return "No download URL.";

        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto dir = exe.getParentDirectory();
        const auto tempDownload = dir.getChildFile ("_update_download.bin");
        const auto tempExtractDir = dir.getChildFile ("_update_extract");
        const auto stagedBinary = dir.getChildFile (
           #if JUCE_WINDOWS
            "_update_new.exe"
           #else
            "_update_new"
           #endif
        );

        // --- download ---
        {
            juce::URL url (downloadUrl);
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withHttpRequestCmd ("GET")
                            .withExtraHeaders ("User-Agent: quadnoncortex/" + currentVersion() + "\r\n")
                            .withConnectionTimeoutMs (120000);

            std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
            if (in == nullptr)
                return "Download failed.";

            tempDownload.deleteFile();
            std::unique_ptr<juce::FileOutputStream> out (tempDownload.createOutputStream());
            if (out == nullptr || ! out->openedOk())
                return "Could not write temporary download.";

            out->writeFromInputStream (*in, -1);
            out.reset();

            if (tempDownload.getSize() < 10000)
            {
                tempDownload.deleteFile();
                return "Downloaded file looks invalid (too small).";
            }
        }

        // --- resolve binary (zip extract or raw) ---
        stagedBinary.deleteFile();
        const bool isZip = assetName.endsWithIgnoreCase (".zip")
                        || tempDownload.getFileExtension().equalsIgnoreCase (".zip");

        if (isZip || looksLikeZip (tempDownload))
        {
            tempExtractDir.deleteRecursively();
            tempExtractDir.createDirectory();

            juce::ZipFile zip (tempDownload);
            if (zip.getNumEntries() <= 0)
            {
                tempDownload.deleteFile();
                return "Zip archive is empty or invalid.";
            }

            // Prefer a file named like the current exe, else first executable-looking entry
            const auto exeName = exe.getFileName();
            int best = -1;
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                if (auto* e = zip.getEntry (i))
                {
                    if (e->isSymbolicLink)
                        continue;
                    const auto n = e->filename.fromLastOccurrenceOf ("/", false, false)
                                              .fromLastOccurrenceOf ("\\", false, false);
                    if (n.isEmpty() || n.endsWithChar ('/'))
                        continue;
                    if (n.equalsIgnoreCase (exeName) || n.equalsIgnoreCase ("quadnoncortex")
                        || n.equalsIgnoreCase ("quadnoncortex.exe"))
                    {
                        best = i;
                        break;
                    }
                    if (best < 0)
                        best = i;
                }
            }

            if (best < 0)
            {
                tempDownload.deleteFile();
                tempExtractDir.deleteRecursively();
                return "No binary found inside the zip.";
            }

            if (zip.uncompressEntry (best, tempExtractDir, true) == nullptr)
            {
                tempDownload.deleteFile();
                tempExtractDir.deleteRecursively();
                return "Failed to extract binary from zip.";
            }

            // Find extracted file (may be nested one level)
            juce::Array<juce::File> found;
            tempExtractDir.findChildFiles (found, juce::File::findFiles, true);
            juce::File extracted;
            for (auto& f : found)
            {
                if (f.getFileName().equalsIgnoreCase (exeName)
                    || f.getFileName().equalsIgnoreCase ("quadnoncortex")
                    || f.getFileName().equalsIgnoreCase ("quadnoncortex.exe"))
                {
                    extracted = f;
                    break;
                }
            }
            if (extracted == juce::File() && ! found.isEmpty())
                extracted = found.getFirst();

            if (! extracted.existsAsFile())
            {
                tempDownload.deleteFile();
                tempExtractDir.deleteRecursively();
                return "Extracted file missing.";
            }

            if (! extracted.copyFileTo (stagedBinary))
            {
                tempDownload.deleteFile();
                tempExtractDir.deleteRecursively();
                return "Could not stage new binary.";
            }
            tempExtractDir.deleteRecursively();
        }
        else
        {
            // Raw binary (e.g. bare .exe asset)
            if (! tempDownload.moveFileTo (stagedBinary)
                && ! tempDownload.copyFileTo (stagedBinary))
            {
                tempDownload.deleteFile();
                return "Could not stage downloaded binary.";
            }
        }

        tempDownload.deleteFile();

        if (stagedBinary.getSize() < 10000)
        {
            stagedBinary.deleteFile();
            return "Staged binary looks invalid.";
        }

       #if ! JUCE_WINDOWS
        stagedBinary.setExecutePermission (true);
       #endif

        // --- schedule replace + relaunch after this process exits ---
        return scheduleReplaceAndRelaunch (exe, stagedBinary);
    }

    /** Back-compat name used by older Settings UI. */
    static juce::String installWindowsUpdate (const juce::String& downloadUrl, const juce::String& assetName)
    {
        return installUpdate (downloadUrl, assetName);
    }

private:
    static bool looksLikeZip (const juce::File& f)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk()) return false;
        char magic[4] = {};
        if (in.read (magic, 4) < 2) return false;
        return magic[0] == 'P' && magic[1] == 'K';
    }

    static juce::String scheduleReplaceAndRelaunch (const juce::File& exe, const juce::File& staged)
    {
        const auto dir = exe.getParentDirectory();
        const auto exePath = exe.getFullPathName();
        const auto stagedPath = staged.getFullPathName();

       #if JUCE_WINDOWS
        const auto bat = dir.getChildFile ("_apply_update.bat");
        juce::String script;
        script << "@echo off\r\n"
               << "timeout /t 2 /nobreak >nul\r\n"
               << ":retry\r\n"
               << "del /f /q \"" << exePath << "\" 2>nul\r\n"
               << "if exist \"" << exePath << "\" (\r\n"
               << "  timeout /t 1 /nobreak >nul\r\n"
               << "  goto retry\r\n"
               << ")\r\n"
               << "move /y \"" << stagedPath << "\" \"" << exePath << "\"\r\n"
               << "start \"\" \"" << exePath << "\"\r\n"
               << "del /f /q \"%~f0\"\r\n";
        bat.replaceWithText (script);
        juce::String cmd = "cmd.exe /C start \"\" /MIN \"" + bat.getFullPathName() + "\"";
        system (cmd.toRawUTF8());
        return {};
       #else
        // Linux / macOS: shell script
        const auto sh = dir.getChildFile ("_apply_update.sh");
        juce::String script;
        script << "#!/bin/sh\n"
               << "sleep 2\n"
               << "EXE=\"" << exePath << "\"\n"
               << "NEW=\"" << stagedPath << "\"\n"
               << "i=0\n"
               << "while [ $i -lt 30 ]; do\n"
               << "  rm -f \"$EXE\" 2>/dev/null && break\n"
               << "  sleep 1\n"
               << "  i=$((i+1))\n"
               << "done\n"
               << "mv -f \"$NEW\" \"$EXE\"\n"
               << "chmod +x \"$EXE\"\n"
               << "\"$EXE\" &\n"
               << "rm -f \"$0\"\n";
        sh.replaceWithText (script);
        sh.setExecutePermission (true);
        juce::ChildProcess proc;
        // Detach: no wait
        if (! proc.start ("/bin/sh \"" + sh.getFullPathName() + "\"",
                          juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            // Fallback
            system (("nohup /bin/sh \"" + sh.getFullPathName() + "\" >/dev/null 2>&1 &").toRawUTF8());
        }
        return {};
       #endif
    }
};
