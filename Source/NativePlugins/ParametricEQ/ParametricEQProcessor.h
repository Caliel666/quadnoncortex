#pragma once
#include <JuceHeader.h>

/** 4-band parametric EQ: low shelf, 2 peaks, high shelf. */
class ParametricEQProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Parametric EQ";
    static constexpr const char* kId   = "internal://native-eq";
    static constexpr int kUid = 0x4E455131; // NEQ1

    ParametricEQProcessor();
    ~ParametricEQProcessor() override = default;

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

    // Band 0 low shelf, 1/2 peak, 3 high shelf — freq, gain, Q each
    juce::AudioParameterFloat* freq[4] {};
    juce::AudioParameterFloat* gain[4] {};
    juce::AudioParameterFloat* q[4] {};
    juce::AudioParameterFloat* outputGain = nullptr;

private:
    void updateFilters();
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    std::array<Filter, 4> filtersL, filtersR;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQProcessor)
};
