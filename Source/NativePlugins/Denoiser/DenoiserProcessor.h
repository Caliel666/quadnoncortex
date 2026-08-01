#pragma once
#include <JuceHeader.h>

/**
 * Guitar 4-band denoiser (0 latency) + gate.
 *
 * Threshold = master amount (0 dB = off, -80 dB = full).
 * Band sliders only apply when Threshold is engaged; they scale max cut per band.
 *
 * Bands: Hum ~80 Hz | Mid grounding ~1.5 kHz (wide) | Preamp ~4 kHz | Hiss ~9 kHz
 */
class DenoiserProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Denoiser";
    static constexpr const char* kId   = "internal://native-denoiser";
    static constexpr int kUid = 0x4E444E53;

    DenoiserProcessor();
    ~DenoiserProcessor() override = default;

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

    juce::AudioParameterFloat* threshold = nullptr; // master amount: 0 = off, -80 = full
    juce::AudioParameterFloat* bandAmt[4] {};
    juce::AudioParameterBool*  hfBias = nullptr;

    juce::AudioParameterBool*  gateOn = nullptr;
    juce::AudioParameterFloat* gateThresh = nullptr;
    juce::AudioParameterBool*  gateHard = nullptr;
    juce::AudioParameterFloat* gateAttack = nullptr;
    juce::AudioParameterFloat* gateRelease = nullptr;

private:
    static constexpr int kBands = 4;
    static constexpr float kCentre[kBands] = { 80.0f, 1500.0f, 4000.0f, 9000.0f };
    static constexpr float kQ[kBands]      = { 1.4f,  0.45f,   0.85f,   0.75f };

    double sr = 48000.0;

    float lastCutDb[kBands] {};
    float smoothCut[kBands] {}; // heavily smoothed target cuts

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    Filter dynL[kBands], dynR[kBands];

    float gateEnv = 0.0f;
    float gateGain = 1.0f;
    int blockCounter = 0;

    void setBandCut (int b, float cutDb);
    float processGate (float peak);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DenoiserProcessor)
};
