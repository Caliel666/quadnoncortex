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

    AudioEngine audioEngine;

    juce::TextButton prevPresetBtn { "<" };
    juce::TextButton nextPresetBtn { ">" };
    juce::ComboBox   presetBox;
    juce::TextButton newPresetBtn { "NEW" };
    juce::TextButton savePresetBtn { "SAVE" };
    juce::TextButton renamePresetBtn { "REN" };
    juce::TextButton addButton     { "+" };
    juce::TextButton settingsBtn   { "SET" };
    juce::Label      titleLabel;

    juce::Slider inputFader, outputFader;
    juce::Label  inLabel { {}, "IN" }, outLabel { {}, "OUT" };
    float smoothInPeak = 0.0f, smoothOutPeak = 0.0f;

    juce::TextButton tabPedal { "BOARD" };
    juce::TextButton tabTuner { "TUNER" };
    int currentTab = 0;

    juce::Viewport blocksViewport;
    juce::Component blocksContent;
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
    std::function<void(juce::String)> pendingNameCallback;
    juce::String pendingNameText;

    int selectedIndex = -1;
    juce::Array<juce::File> presetFiles;
    int currentPresetIndex = -1;

    static constexpr int kTopBar = 52;
    static constexpr int kSideW  = 64;
    static constexpr int kTabH   = 48;
    static constexpr int kBlockW = 96; // base; layout scales dynamically
    static constexpr int kBlockH = 96;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
