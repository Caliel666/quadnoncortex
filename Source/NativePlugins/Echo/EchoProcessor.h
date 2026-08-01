#pragma once
#include <JuceHeader.h>

/** Stereo echo / delay — Airwindows-simple: time, feedback, mix, ping-pong. */
class EchoProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Echo";
    static constexpr const char* kId   = "internal://native-echo";
    static constexpr int kUid = 0x4E45434F; // NECO

    EchoProcessor();
    ~EchoProcessor() override = default;

    void fillInPluginDescription (juce::PluginDescription& d) const override;
    static juce::PluginDescription makeDescription();

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
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

    juce::AudioParameterFloat* timeMs = nullptr;
    juce::AudioParameterFloat* feedback = nullptr;
    juce::AudioParameterFloat* mix = nullptr;
    juce::AudioParameterBool*  pingPong = nullptr;
    juce::AudioParameterFloat* tone = nullptr; // high-cut on feedback path

private:
    juce::AudioBuffer<float> delayBuf;
    int writePos = 0;
    double sr = 48000.0;
    float lpStateL = 0.0f, lpStateR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoProcessor)
};
