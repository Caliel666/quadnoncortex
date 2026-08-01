#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>

#if QUADNONCORTEX_HAS_NAM
namespace nam { class DSP; }
#endif

/** Internal NAM amp/pedal/cab processor — appears as "Native NAM" in the plugin list. */
class NativeNamProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName         = "Native NAM";
    static constexpr const char* kFormatName   = "Native";
    static constexpr const char* kManufacturer = "quadnoncortex";

    NativeNamProcessor();
    ~NativeNamProcessor() override;

    // AudioPluginInstance
    void fillInPluginDescription (juce::PluginDescription& d) const override;

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return cabLoaded ? 1.0 : 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    enum class Slot { Pedal, Amp, Cab };

    bool loadModel (Slot slot, const juce::File& file);
    void clearModel (Slot slot);
    juce::File getModelFile (Slot slot) const;
    juce::String getModelName (Slot slot) const;
    juce::String getLastError() const { return lastError; }

    // All of these are normal AudioProcessorParameters → MIDI-learnable via ParameterPanel
    juce::AudioParameterFloat* inputGainParam   = nullptr;
    juce::AudioParameterFloat* outputGainParam  = nullptr;
    juce::AudioParameterBool*  bypassPedalParam = nullptr;
    juce::AudioParameterBool*  bypassAmpParam   = nullptr;
    juce::AudioParameterBool*  bypassCabParam   = nullptr;
    juce::AudioParameterBool*  liteModeParam    = nullptr; // true = NAM2 A2 Lite, false = Full
    juce::AudioParameterFloat* pedalMixParam    = nullptr; // 0..1 how much pedal affects signal
    juce::AudioParameterFloat* ampGainParam     = nullptr; // dB drive into/after amp
    juce::AudioParameterFloat* ampLowParam      = nullptr; // dB low shelf
    juce::AudioParameterFloat* ampMidParam      = nullptr; // dB mid peak
    juce::AudioParameterFloat* ampHighParam     = nullptr; // dB high shelf

    std::function<void()> onModelsChanged;

    /** NAM2 A2 Lite (0.0) vs Full (1.0) via SetSlimmableSize. */
    void applySlimMode();

    static bool isNativeNam (const juce::AudioPluginInstance* inst);
    static juce::PluginDescription makeDescription();

private:
    struct NamSlot
    {
        juce::File path;
        juce::String name;
       #if QUADNONCORTEX_HAS_NAM
        std::unique_ptr<nam::DSP> dsp;
       #endif
        juce::CriticalSection lock;
    };

    NamSlot pedal, amp;
    juce::File cabPath;
    juce::String cabName;
    juce::dsp::Convolution cabConv;
    bool cabLoaded = false;
    juce::String lastError;

    double currentSr = 48000.0;
    int currentBs = 512;

    juce::AudioBuffer<float> monoScratch;
    // NAM core processes doubles
    std::vector<double> inDoubles, outDoubles;

    void processNamSlot (NamSlot& slot, float* data, int n);
    void applySlimModeToSlot (NamSlot& slot);
    void updateAmpEq();
    void reloadCabIR();

    juce::dsp::IIR::Filter<float> ampLowFilter, ampMidFilter, ampHighFilter;
    std::vector<float> dryScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeNamProcessor)
};

class NativeNamFormat : public juce::AudioPluginFormat
{
public:
    NativeNamFormat() = default;
    juce::String getName() const override { return NativeNamProcessor::kFormatName; }
    bool fileMightContainThisPluginType (const juce::String&) override { return false; }
    juce::FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool canScanForPlugins() const override { return false; }
    bool isTrivialToScan() const override { return true; }
    void findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results,
                              const juce::String&) override;
    bool doesPluginStillExist (const juce::PluginDescription&) override { return true; }
    juce::String getNameOfPluginFromIdentifier (const juce::String& id) override { return id; }
    juce::StringArray searchPathsForPlugins (const juce::FileSearchPath&, bool, bool) override { return {}; }
    bool pluginNeedsRescanning (const juce::PluginDescription&) override { return false; }
    void createPluginInstance (const juce::PluginDescription&, double, int, PluginCreationCallback) override;
    bool requiresUnblockedMessageThreadDuringCreation (const juce::PluginDescription&) const override { return false; }
};
