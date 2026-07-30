#pragma once

#include <JuceHeader.h>
#include "PluginChain.h"
#include "MidiLearnManager.h"

//==============================================================================
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();
    void saveDeviceState();

    PluginChain&              getPluginChain()       { return chain; }
    MidiLearnManager&         getMidiLearnManager()  { return midiLearn; }
    juce::AudioDeviceManager& getDeviceManager()     { return deviceManager; }

    void setInputGain  (float g) { inputGain.store  (g); }
    void setOutputGain (float g) { outputGain.store (g); }
    float getInputGain()  const  { return inputGain.load(); }
    float getOutputGain() const  { return outputGain.load(); }

    /** When true, audio output is silenced (tuner view). */
    void setMuted (bool m) { muted.store (m); }
    bool isMuted() const   { return muted.load(); }
    float getInputPeak()  const { return inputPeak.load(); }
    float getOutputPeak() const { return outputPeak.load(); }
    double getSampleRate() const { return sampleRate; }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // MidiInputCallback
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    std::function<void(const juce::MidiMessage&)> onMidiForGlobals;
    std::function<void(const float*, int)> onAudioForTuner;

private:
    juce::AudioDeviceManager deviceManager;
    PluginChain              chain;
    MidiLearnManager         midiLearn;

    juce::AudioBuffer<float> processBuffer;
    juce::MidiBuffer         incomingMidi;
    juce::CriticalSection    midiLock;

    std::atomic<float> inputGain  { 1.0f };
    std::atomic<float> outputGain { 1.0f };
    std::atomic<bool>  muted      { false };
    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    double sampleRate = 44100.0;
    int    blockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
