#include <JuceHeader.h>
#include "MainComponent.h"
#include "AppSettings.h"
#include "DevLog.h"

/** Apply saved window mode to any top-level DocumentWindow named quadnoncortex. */
void applyAppWindowSettings()
{
    auto& s = AppSettings::get();
    auto& desktop = juce::Desktop::getInstance();
    juce::DocumentWindow* win = nullptr;
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* dw = dynamic_cast<juce::DocumentWindow*> (desktop.getComponent (i)))
            if (dw->getName() == "quadnoncortex")
            {
                win = dw;
                break;
            }
    if (win == nullptr) return;

    auto* display = desktop.getDisplays().getPrimaryDisplay();
    const auto area = display != nullptr ? display->totalArea.toNearestInt()
                                         : juce::Rectangle<int> (0, 0, 1920, 1080);

    win->setUsingNativeTitleBar (false);
    win->setTitleBarHeight (0);
    win->setResizable (false, false);

    if (s.windowFullscreen)
    {
        int w = s.fullscreenWidth  > 0 ? s.fullscreenWidth  : area.getWidth();
        int h = s.fullscreenHeight > 0 ? s.fullscreenHeight : area.getHeight();
        w = juce::jmin (w, area.getWidth());
        h = juce::jmin (h, area.getHeight());
        if (w >= area.getWidth() - 2 && h >= area.getHeight() - 2)
        {
            win->setBounds (area);
            win->setFullScreen (true);
        }
        else
        {
            win->setFullScreen (false);
            win->setBounds (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h);
        }
    }
    else
    {
        win->setFullScreen (false);
        const int w = juce::jlimit (640, area.getWidth(),  s.windowedWidth);
        const int h = juce::jlimit (360, area.getHeight(), s.windowedHeight);
        win->setBounds (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h);
    }
    win->setVisible (true);
    win->toFront (true);
}

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
            setVisible (true);
            // Apply fullscreen / windowed from settings (default fullscreen)
            applyAppWindowSettings();
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
