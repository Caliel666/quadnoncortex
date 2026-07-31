#include <JuceHeader.h>
#include "MainComponent.h"
#include "DevLog.h"

class QuadnonCortexApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "quadnoncortex"; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, 0)
        {
            setUsingNativeTitleBar (false);
            setTitleBarHeight (0);
            setContentOwned (new MainComponent(), true);
            setResizable (false, false);

            // Borderless fullscreen on primary display (NOT always-on-top,
            // so plugin editor windows can appear above)
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                setBounds (display->totalArea.toNearestInt());

            setFullScreen (true);
            setVisible (true);
            toFront (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (QuadnonCortexApplication)
