#pragma once
#include <JuceHeader.h>

/** Simple room / hall reverb (JUCE Reverb + mix/width). */
class ReverbProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Reverb";
    static constexpr const char* kId   = "internal://native-reverb";
    static constexpr int kUid = 0x4E525642; // NRVB

    ReverbProcessor();
    ~ReverbProcessor() override = default;

    void fillInPluginDescription (juce::PluginDescription& d) const override;
    static juce::PluginDescription makeDescription();

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioParameterFloat* size = nullptr;
    juce::AudioParameterFloat* damping = nullptr;
    juce::AudioParameterFloat* width = nullptr;
    juce::AudioParameterFloat* mix = nullptr;
    juce::AudioParameterFloat* freeze = nullptr; // 0..1 freeze mode

private:
    juce::Reverb reverb;
    double sr = 48000.0;
    void applyParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbProcessor)
};
