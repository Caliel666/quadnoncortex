#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "PluginBlockComponent.h"
#include "ParameterPanel.h"
#include "PluginBrowser.h"
#include "PluginScanOverlay.h"
#include "TunerComponent.h"
#include "SettingsComponent.h"
#include "AppSettings.h"
#include "OnScreenKeyboard.h"
#include "IconButton.h"

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::DragAndDropTarget,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct BlockContentArea : public juce::Component
    {
        std::function<void(juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    void timerCallback() override;
    void rebuildBlocks();
    void selectPlugin (int index);
    void removePlugin (int index);
    void reorderPlugins (int from, int to);
    void startBootScan();
    void showPluginEditor (int index);
    void setTab (int tab);
    void loadPreset (const juce::File& f);
    void savePreset();
    void newPreset();
    void showPresetNameDialog (const juce::String& title,
                               const juce::String& initial,
                               std::function<void(const juce::String&)> onOk);
    void dismissNameOverlay (bool commit, const juce::String& text);
    void renamePreset();
    void presetNext();
    void presetPrev();
    void refreshPresetList();
    void updatePresetNameDisplay();
    void showPresetPickerOverlay();
    void handleGlobalMidi (const juce::String& action, float value);
    void openSettings();
    void cycleBlockColour (int index);
    void paintMeter (juce::Graphics& g, juce::Rectangle<int> area, float peak);
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void showColourPicker (int pluginIndex);
    void paintConnectionLines (juce::Graphics& g, const juce::Rectangle<int>& area);
    int getBlockRow (int index) const;

    AudioEngine audioEngine;

    IconButton prevPresetBtn { IconButton::Icon::ChevronUp, {}, true };
    IconButton nextPresetBtn { IconButton::Icon::ChevronDown, {}, true };
    juce::Label      presetNameLabel;   // big Quad Cortex-style name (tap opens picker)
    IconButton newPresetBtn { IconButton::Icon::None, "NEW" };
    IconButton savePresetBtn { IconButton::Icon::Save };
    IconButton renamePresetBtn { IconButton::Icon::Pencil };
    IconButton addButton { IconButton::Icon::Plus };
    IconButton settingsBtn { IconButton::Icon::Gear };
    std::unique_ptr<juce::Component> presetPickerOverlay;

    juce::Slider inputFader, outputFader;
    juce::Label  inLabel { {}, "IN" }, outLabel { {}, "OUT" };
    float smoothInPeak = 0.0f, smoothOutPeak = 0.0f;

    juce::TextButton tabPedal { "BOARD" };
    juce::TextButton tabTuner { "TUNER" };
    int currentTab = 0;

    juce::Viewport blocksViewport;
    BlockContentArea blocksContent;
    juce::OwnedArray<PluginBlockComponent> blocks;
    std::unique_ptr<ParameterPanel> parameterPanel;
    std::unique_ptr<TunerComponent> tuner;
    std::unique_ptr<PluginBrowser> pluginBrowser;
    std::unique_ptr<PluginScanOverlay> scanOverlay;
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    juce::TextButton trashZone { "X" };
    bool showTrash = false;
    int  dragSourceIndex = -1;
    bool dragOverTrash = false;
    std::unique_ptr<juce::Component> nameOverlay;
    std::unique_ptr<juce::Component> settingsOverlay;
    float paramAnim = 1.0f;
    float tabAnim = 1.0f;
    float presetAnim = 1.0f;
    bool presetAnimating = false;
    bool presetLoading = false;
    juce::uint32 lastPresetSwitchMs = 0;
    std::function<void(juce::String)> pendingNameCallback;
    juce::String pendingNameText;

    int selectedIndex = -1;
    juce::Array<juce::File> presetFiles;
    int currentPresetIndex = -1;

    static constexpr int kTopBarMin = 120;
    int topBarH = 140; // set in resized()
    static constexpr int kSideW  = 72;
    static constexpr int kTabH   = 56;
    static constexpr int kBlockW = 96; // base; layout scales dynamically
    static constexpr int kBlockH = 96;

    friend struct PresetPickerOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
