#include "PluginScanOverlay.h"

struct PluginScanThread : public juce::Thread
{
    PluginScanOverlay& owner;
    explicit PluginScanThread (PluginScanOverlay& o) : juce::Thread ("VST3Scanner"), owner (o) {}
    void run() override { owner.runScanOnBackgroundThread(); }
};

static bool shouldSkipPluginFile (const juce::String& path)
{
    const auto lower = path.toLowerCase();
    if (lower.contains ("ara") || lower.contains ("-ara") || lower.contains ("_ara")) return true;
    if (lower.contains ("waveshell") || lower.contains ("waveslib") || lower.contains ("waverack")) return true;
    return false;
}

PluginScanOverlay::PluginScanOverlay (PluginChain& chain)
    : pluginChain (chain), progressBar (progress)
{
    titleLabel.setText ("Scanning VST3 plugins...", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("Preparing...", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (16.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    progressBar.setPercentageDisplay (true);
    addAndMakeVisible (progressBar);
    setVisible (false);
}

PluginScanOverlay::~PluginScanOverlay()
{
    shouldStop = true;
    if (scanThread != nullptr) { scanThread->stopThread (8000); scanThread = nullptr; }
    stopTimer();
}

void PluginScanOverlay::startScan (std::function<void()> onFinished)
{
    if (scanning) return;
    finishedCallback = std::move (onFinished);
    scanning = true;
    shouldStop = false;
    progress = 0.0;
    currentFileName = "Checking...";
    setVisible (true);
    toFront (true);
    startTimerHz (15);
    scanThread = std::make_unique<PluginScanThread> (*this);
    scanThread->startThread (juce::Thread::Priority::background);
}

void PluginScanOverlay::runScanOnBackgroundThread()
{
    auto& list    = pluginChain.getKnownPluginList();
    auto& formats = pluginChain.getFormatManager();
    auto paths    = PluginChain::getDefaultVST3Paths();
    auto deadMansPedal = PluginChain::getAppDataDir().getChildFile ("DeadMansPedal");
    auto blacklistFile = PluginChain::getAppDataDir().getChildFile ("PluginBlacklist.txt");
    auto cacheFile     = PluginChain::getScanCacheFile();

    juce::StringArray permanentSkip;
    if (blacklistFile.existsAsFile()) permanentSkip.addLines (blacklistFile.loadFileAsString());
    if (deadMansPedal.existsAsFile()) permanentSkip.addLines (deadMansPedal.loadFileAsString());

    // Load previous scan cache: path -> modificationTime
    std::map<juce::String, juce::int64> cachedTimes;
    if (cacheFile.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse (cacheFile))
            for (auto* e : xml->getChildIterator())
                if (e->hasTagName ("File"))
                    cachedTimes[e->getStringAttribute ("path")] = e->getStringAttribute ("mod").getLargeIntValue();
    }

    juce::StringArray filesToScan;
    juce::StringArray allFiles;
    for (int i = 0; i < formats.getNumFormats(); ++i)
    {
        auto* format = formats.getFormat (i);
        if (format == nullptr || format->getName() != "VST3") continue;
        auto files = format->searchPathsForPlugins (paths, true, true);
        for (auto& f : files)
        {
            if (shouldSkipPluginFile (f) || permanentSkip.contains (f)) continue;
            allFiles.addIfNotAlreadyThere (f);

            juce::File file (f);
            const auto mod = file.getLastModificationTime().toMilliseconds();
            auto it = cachedTimes.find (f);
            // Only scan if never seen or file changed
            if (it == cachedTimes.end() || it->second != mod)
                filesToScan.addIfNotAlreadyThere (f);
        }
    }

    // Remove from known list any files that no longer exist
    // (left for future; known list is additive)

    const int totalFiles = filesToScan.size();
    int doneFiles = 0;

    if (totalFiles == 0)
    {
        progress = 1.0;
        {
            const juce::ScopedLock sl (nameLock);
            currentFileName = allFiles.isEmpty() ? "No plugins found" : "Up to date";
        }
        juce::MessageManager::callAsync ([this] { finishOnMessageThread(); });
        return;
    }

    std::map<juce::String, juce::int64> newCache = cachedTimes;

    for (int fi = 0; fi < filesToScan.size() && ! shouldStop; ++fi)
    {
        const auto filePath = filesToScan[fi];
        {
            const juce::ScopedLock sl (nameLock);
            currentFileName = filePath;
        }

        deadMansPedal.replaceWithText (filePath);

        try
        {
            for (int i = 0; i < formats.getNumFormats(); ++i)
            {
                auto* format = formats.getFormat (i);
                if (format == nullptr || format->getName() != "VST3") continue;

                juce::OwnedArray<juce::PluginDescription> typesFound;
                format->findAllTypesForFile (typesFound, filePath);
                for (auto* desc : typesFound)
                {
                    if (desc == nullptr) continue;
                    if (shouldSkipPluginFile (desc->name) || shouldSkipPluginFile (desc->descriptiveName))
                        continue;
                    list.addType (*desc);
                }
            }
            juce::File file (filePath);
            newCache[filePath] = file.getLastModificationTime().toMilliseconds();
        }
        catch (...)
        {
            permanentSkip.addIfNotAlreadyThere (filePath);
        }

        deadMansPedal.replaceWithText ({});
        ++doneFiles;
        progress = juce::jlimit (0.0, 1.0, (double) doneFiles / (double) juce::jmax (1, totalFiles));
    }

    // Write scan cache
    {
        juce::XmlElement root ("ScanCache");
        for (auto& p : newCache)
        {
            auto* e = root.createNewChildElement ("File");
            e->setAttribute ("path", p.first);
            e->setAttribute ("mod", juce::String (p.second));
        }
        root.writeTo (cacheFile);
    }

    if (permanentSkip.size() > 0)
        blacklistFile.replaceWithText (permanentSkip.joinIntoString ("\n"));

    if (! shouldStop)
        pluginChain.saveKnownPluginsToDisk();

    progress = 1.0;
    juce::MessageManager::callAsync ([this] { finishOnMessageThread(); });
}

void PluginScanOverlay::finishOnMessageThread()
{
    stopTimer();
    scanning = false;
    if (scanThread != nullptr) { scanThread->stopThread (1000); scanThread = nullptr; }
    setVisible (false);
    if (finishedCallback) finishedCallback();
}

void PluginScanOverlay::timerCallback()
{
    juce::String name;
    { const juce::ScopedLock sl (nameLock); name = currentFileName; }
    if (name.containsChar ('\\') || name.containsChar ('/'))
        name = juce::File (name).getFileName();
    if (name.length() > 60) name = "..." + name.substring (name.length() - 57);
    statusLabel.setText (name.isEmpty() ? "Working..." : name, juce::dontSendNotification);
    repaint();
}

void PluginScanOverlay::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xee0a0a0a)); }

void PluginScanOverlay::resized()
{
    auto r = getLocalBounds().reduced (40);
    auto centre = r.withSizeKeepingCentre (juce::jmin (500, r.getWidth()), 160);
    titleLabel.setBounds (centre.removeFromTop (40));
    centre.removeFromTop (12);
    progressBar.setBounds (centre.removeFromTop (28));
    centre.removeFromTop (12);
    statusLabel.setBounds (centre.removeFromTop (28));
}
