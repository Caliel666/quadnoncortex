#pragma once
#include <JuceHeader.h>

/**
 * Split / Join node for parallel lanes.
 *
 * Mode Split:  1 trunk → N mono lanes (default 2: L and R)
 * Mode Join:   N lanes → 1 stereo trunk (sum + mix)
 *
 * Routing (per lane): source = Mute | Left | Right | Mono(L+R) | Stereo
 * A/B: which lane is "active" for monitoring or MIDI mute of the other (abLearn)
 */
class SplitterProcessor : public juce::AudioPluginInstance
{
public:
    static constexpr const char* kName = "Splitter";
    static constexpr const char* kId   = "internal://native-splitter";
    static constexpr int kUid = 0x4E53504C; // NSPL
    static constexpr int kMaxLanes = 4;

    enum class Mode  { Split = 0, Join = 1 };
    enum class Source { Mute = 0, Left = 1, Right = 2, MonoSum = 3, Stereo = 4 };

    SplitterProcessor();
    ~SplitterProcessor() override = default;

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

    // Exposed for chain / custom UI
    juce::AudioParameterChoice* modeParam = nullptr;   // Split / Join
    juce::AudioParameterInt*    numLanes  = nullptr;   // 2..4
    juce::AudioParameterFloat*  mix       = nullptr;
    juce::AudioParameterChoice* abSelect  = nullptr;   // A / B / Both (MIDI-learnable)
    juce::AudioParameterChoice* lane0Src  = nullptr;
    juce::AudioParameterChoice* lane1Src  = nullptr;
    juce::AudioParameterChoice* lane2Src  = nullptr;
    juce::AudioParameterChoice* lane3Src  = nullptr;

    Mode getMode() const;
    int  getNumLanesActive() const;
    Source getLaneSource (int lane) const;
    int  getAbMode() const; // 0=A, 1=B, 2=Both

    /** Fill dest[lane] mono buffers from stereo input per routing matrix. */
    void splitToLanes (const juce::AudioBuffer<float>& stereoIn,
                       std::vector<juce::AudioBuffer<float>>& laneBufs,
                       int numSamples) const;

    /** Mix mono lane buffers back into stereoOut (Join). */
    void joinFromLanes (const std::vector<juce::AudioBuffer<float>>& laneBufs,
                        juce::AudioBuffer<float>& stereoOut,
                        int numSamples) const;

private:
    static juce::StringArray sourceChoices();
    juce::AudioParameterChoice* srcParam (int lane) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitterProcessor)
};
