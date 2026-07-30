#pragma once

#include <JuceHeader.h>
#include "PluginChain.h"

//==============================================================================
/** Full-screen overlay shown while VST3 plugins are scanned on a background
    thread. Uses dead-man's-pedal to skip previously crashing plugins.
*/
class PluginScanOverlay : public juce::Component,
                          private juce::Timer
{
public:
    explicit PluginScanOverlay (PluginChain& chain);
    ~PluginScanOverlay() override;

    /** Start a background scan of the default VST3 paths.
        onFinished is called on the message thread when done. */
    void startScan (std::function<void()> onFinished);

    bool isScanning() const { return scanning; }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called by the background thread (public so the thread helper can reach them)
    void runScanOnBackgroundThread();
    void finishOnMessageThread();

private:
    void timerCallback() override;

    PluginChain& pluginChain;
    std::function<void()> finishedCallback;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::ProgressBar progressBar;
    double progress = 0.0;

    std::atomic<bool> scanning { false };
    std::atomic<bool> shouldStop { false };
    juce::String currentFileName;
    juce::CriticalSection nameLock;

    std::unique_ptr<juce::Thread> scanThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanOverlay)
};
