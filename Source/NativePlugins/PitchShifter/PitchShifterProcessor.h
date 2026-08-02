#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * Guitar pitch shifter — adapted from VoLum (MIT). See LICENSE-VoLum.txt.
 *
 * Mono time-domain engine (shared delay/splices for L+R) so stereo stays coherent.
 * Tone = high/low shelf. Clarity = gentle high-pass on wet.
 */
class PitchShifterProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Pitch Shifter";
    static constexpr const char* kId   = "internal://native-pitch";
    static constexpr int kUid = 0x4E505443;

    PitchShifterProcessor();
    ~PitchShifterProcessor() override = default;

    void fillInPluginDescription (juce::PluginDescription& d) const override;
    static juce::PluginDescription makeDescription();

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
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

    juce::AudioParameterInt*    pitchSemis = nullptr;
    juce::AudioParameterChoice* character  = nullptr;
    juce::AudioParameterFloat*  quality    = nullptr;
    juce::AudioParameterFloat*  tone       = nullptr;
    juce::AudioParameterFloat*  clarity    = nullptr;
    juce::AudioParameterFloat*  mix        = nullptr;

private:
    enum class Character { Instant = 0, Drop = 1, Poly = 2 };

    struct Timing
    {
        int xfade = 0, search = 0, corrWin = 0;
        bool wsola = false, fixedGrain = false;
        double dLo = 0, dHi = 0, band = 0;
        int latency = 0;
    };

    static constexpr double kDesignFmin = 82.41;
    static constexpr double kPmaxFreq = 600.0;
    static constexpr double kPminFreq = 40.0;

    double sr = 48000.0;
    int maxBlock = 512;

    // Single mono voice drives both channels (same delay/splices)
    struct Voice
    {
        std::vector<double> bufL, bufR; // stereo ring, shared write/delay
        std::vector<double> periodScratch;
        std::vector<double> refWin;
        size_t write = 0;
        long long writeCount = 0;
        double period = 48000.0 / 110.0;
        int periodCountdown = 1;
        int periodUpdate = 480;
        double delay = 0, delayNew = 0;
        bool fading = false;
        int fadePos = 0;
        double ratio = 1.0;

        int xfade = 0, search = 0, corrWin = 0;
        bool wsola = false, fixedGrain = false;
        double dLo = 0, dHi = 0, band = 0;
        int latency = 0;
        Character char_ = Character::Poly;
        float quality = 0.5f;
    };

    Voice voice;

    // Preallocated process scratch (never allocate on audio thread)
    std::vector<float> dryL, dryR, wetL, wetR;

    // Tone / clarity filters (juce IIR)
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    Filter shelfL, shelfR, hpL, hpR;
    float lastTone = 1.0e9f, lastClar = 1.0e9f;

    static Timing computeTiming (Character c, double sampleRate, float quality);
    void configureVoice (Character c, float quality);
    void resetVoice();
    void processStereo (const float* inL, const float* inR, float* outL, float* outR, int n, bool stereo);

    double readAtDelay (const std::vector<double>& buf, double delay) const;
    double wsolaRefine (double cand);
    void updatePeriod();
    void updateFilters (float toneAmt, float clarityAmt);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShifterProcessor)
};
