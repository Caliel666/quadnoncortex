#pragma once
#include <JuceHeader.h>

class TunerComponent : public juce::Component, private juce::Timer
{
public:
    TunerComponent();
    ~TunerComponent() override = default;

    void pushSamples (const float* data, int numSamples);
    void setSampleRate (double sr) { sampleRate = sr > 0.0 ? sr : 44100.0; }

    float getReferenceHz() const { return referenceHz; }
    void setReferenceHz (float hz);

    bool isOutputMuted() const { return muteOutput; }
    void setOutputMuted (bool muted);

    std::function<void(bool muted)> onMuteChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void analyse();
    void updateRefLabel();
    void updateMuteButton();

    juce::AudioBuffer<float> ring;
    int writePos = 0;
    juce::CriticalSection lock;
    double sampleRate = 44100.0;

    float detectedFreq = 0.0f;
    float centsOffset  = 0.0f;
    float smoothCents  = 0.0f;
    float smoothFreq   = 0.0f;
    bool  hasNote      = false;
    float lockedFreq   = 0.0f;   // last stable pitch (octave hysteresis)
    int   lockFrames   = 0;
    float recentFreqs[5] = {};
    int   recentCount  = 0;
    int   recentIdx    = 0;
    juce::String noteName;
    int noteOctave = 0;

    float referenceHz = 440.0f;
    bool  muteOutput  = true;

    juce::TextButton refDown { "-" };
    juce::TextButton refUp   { "+" };
    juce::Label      refLabel;
    juce::TextButton muteBtn;

    static constexpr int kRingSize = 16384;
    static constexpr float kMinFreq = 45.0f;
    static constexpr float kMaxFreq = 520.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerComponent)
};
