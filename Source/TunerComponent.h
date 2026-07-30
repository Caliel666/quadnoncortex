#pragma once
#include <JuceHeader.h>

class TunerComponent : public juce::Component, private juce::Timer
{
public:
    TunerComponent();
    ~TunerComponent() override = default;

    void pushSamples (const float* data, int numSamples);
    void setSampleRate (double sr) { sampleRate = sr > 0.0 ? sr : 44100.0; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void analyse();

    juce::AudioBuffer<float> ring;
    int writePos = 0;
    juce::CriticalSection lock;
    double sampleRate = 44100.0;

    float detectedFreq = 0.0f;
    float centsOffset  = 0.0f;
    float smoothCents  = 0.0f;
    float smoothFreq   = 0.0f;
    bool  hasNote      = false;
    juce::String noteName;
    int noteOctave = 0;

    static constexpr int kRingSize = 8192;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerComponent)
};
