#pragma once
#include <JuceHeader.h>

/** Guitar-oriented soft-knee compressor (optical-ish). */
class CompressorProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Guitar Comp";
    static constexpr const char* kId   = "internal://native-comp";
    static constexpr int kUid = 0x4E434D50; // NCMP

    CompressorProcessor();
    ~CompressorProcessor() override = default;

    void fillInPluginDescription (juce::PluginDescription& d) const override;
    static juce::PluginDescription makeDescription();

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
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

    juce::AudioParameterFloat* threshold = nullptr; // dB
    juce::AudioParameterFloat* ratio = nullptr;
    juce::AudioParameterFloat* attackMs = nullptr;
    juce::AudioParameterFloat* releaseMs = nullptr;
    juce::AudioParameterFloat* makeup = nullptr; // dB
    juce::AudioParameterFloat* mix = nullptr;

private:
    double sr = 48000.0;
    float env = 0.0f;
    float gainSmoothed = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorProcessor)
};
