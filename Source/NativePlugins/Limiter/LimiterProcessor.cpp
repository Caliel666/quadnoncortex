#include "LimiterProcessor.h"
#include "../NativePluginHelpers.h"
#include <cmath>

LimiterProcessor::LimiterProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // LoudMax-style: Threshold raises input into the brickwall, Ceiling is absolute max out
    NativePluginHelpers::addParam (*this, threshold = new juce::AudioParameterFloat (
        { "lim_th", 1 }, "Threshold",
        juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), 0.0f));
    NativePluginHelpers::addParam (*this, ceiling = new juce::AudioParameterFloat (
        { "lim_ceil", 1 }, "Ceiling",
        juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -0.3f));
}

void LimiterProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription LimiterProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Dynamics", kUid);
}

bool LimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void LimiterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    gainSmoothed = 1.0f;
    peakHold = 0.0f;
    delayBuf.setSize (2, juce::jmax (samplesPerBlock + kLookahead + 16, kLookahead * 2));
    delayBuf.clear();
    writePos = 0;
}

void LimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    const int bufLen = delayBuf.getNumSamples();
    if (bufLen < kLookahead + 2) return;

    // Drive = inverse of threshold (0 dB th = unity, lower th = more make-up into limiter)
    const float drive = juce::Decibels::decibelsToGain (-threshold->get());
    const float ceilLin = juce::Decibels::decibelsToGain (ceiling->get());
    const float release = std::exp (-1.0f / (0.05f * (float) sr)); // ~50ms
    const float attack  = std::exp (-1.0f / (0.001f * (float) sr)); // ~1ms

    auto* dL = delayBuf.getWritePointer (0);
    auto* dR = delayBuf.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        float inL = buffer.getSample (0, i) * drive;
        float inR = (nCh > 1 ? buffer.getSample (1, i) : inL) * drive;

        dL[writePos] = inL;
        dR[writePos] = inR;

        // Peak in lookahead window
        float peak = 0.0f;
        for (int k = 0; k < kLookahead; ++k)
        {
            const int idx = (writePos - k + bufLen) % bufLen;
            peak = juce::jmax (peak, std::abs (dL[idx]), std::abs (dR[idx]));
        }

        float targetGain = 1.0f;
        if (peak > ceilLin && peak > 1.0e-8f)
            targetGain = ceilLin / peak;

        if (targetGain < gainSmoothed)
            gainSmoothed = attack * gainSmoothed + (1.0f - attack) * targetGain;
        else
            gainSmoothed = release * gainSmoothed + (1.0f - release) * targetGain;

        const int readIdx = (writePos - kLookahead + bufLen) % bufLen;
        buffer.setSample (0, i, dL[readIdx] * gainSmoothed);
        if (nCh > 1)
            buffer.setSample (1, i, dR[readIdx] * gainSmoothed);

        writePos = (writePos + 1) % bufLen;
    }
}

void LimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeLim");
    xml->setAttribute ("th", (double) threshold->get());
    xml->setAttribute ("ceil", (double) ceiling->get());
    copyXmlToBinary (*xml, destData);
}

void LimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeLim")) return;
    *threshold = (float) xml->getDoubleAttribute ("th", 0.0);
    *ceiling = (float) xml->getDoubleAttribute ("ceil", -0.3);
}

juce::AudioProcessorEditor* LimiterProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
