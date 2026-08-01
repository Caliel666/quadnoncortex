#include "EchoProcessor.h"
#include "../NativePluginHelpers.h"

EchoProcessor::EchoProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    NativePluginHelpers::addParam (*this, timeMs = new juce::AudioParameterFloat (
        { "echo_time", 1 }, "Time",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 350.0f));
    NativePluginHelpers::addParam (*this, feedback = new juce::AudioParameterFloat (
        { "echo_fb", 1 }, "Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.35f));
    NativePluginHelpers::addParam (*this, mix = new juce::AudioParameterFloat (
        { "echo_mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    NativePluginHelpers::addParam (*this, pingPong = new juce::AudioParameterBool (
        { "echo_pp", 1 }, "Ping Pong", false));
    NativePluginHelpers::addParam (*this, tone = new juce::AudioParameterFloat (
        { "echo_tone", 1 }, "Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));
}

void EchoProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription EchoProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Delay", kUid);
}

bool EchoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void EchoProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int maxSamples = (int) (sampleRate * 2.5) + 64;
    delayBuf.setSize (2, maxSamples);
    delayBuf.clear();
    writePos = 0;
    lpStateL = lpStateR = 0.0f;
}

void EchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    const int bufLen = delayBuf.getNumSamples();
    if (bufLen < 2) return;

    const float delaySamples = juce::jlimit (1.0f, (float) (bufLen - 2),
                                             (float) (timeMs->get() * 0.001 * sr));
    const float fb = feedback->get();
    const float wet = mix->get();
    const float dry = 1.0f - wet;
    const bool pp = pingPong->get();
    // tone 1 = bright, 0 = dark
    const float lp = 0.15f + tone->get() * 0.8f;

    auto* dL = delayBuf.getWritePointer (0);
    auto* dR = delayBuf.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float inL = buffer.getSample (0, i);
        const float inR = nCh > 1 ? buffer.getSample (1, i) : inL;

        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) bufLen;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % bufLen;
        const float frac = readPos - (float) i0;
        float delL = dL[i0] + frac * (dL[i1] - dL[i0]);
        float delR = dR[i0] + frac * (dR[i1] - dR[i0]);

        // feedback low-pass
        lpStateL += lp * (delL - lpStateL);
        lpStateR += lp * (delR - lpStateR);
        delL = lpStateL;
        delR = lpStateR;

        float fbL, fbR;
        if (pp)
        {
            fbL = inL + delR * fb;
            fbR = inR + delL * fb;
        }
        else
        {
            fbL = inL + delL * fb;
            fbR = inR + delR * fb;
        }

        dL[writePos] = fbL;
        dR[writePos] = fbR;
        writePos = (writePos + 1) % bufLen;

        buffer.setSample (0, i, dry * inL + wet * delL);
        if (nCh > 1)
            buffer.setSample (1, i, dry * inR + wet * delR);
    }
}

void EchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeEcho");
    xml->setAttribute ("time", (double) timeMs->get());
    xml->setAttribute ("fb", (double) feedback->get());
    xml->setAttribute ("mix", (double) mix->get());
    xml->setAttribute ("pp", pingPong->get() ? 1 : 0);
    xml->setAttribute ("tone", (double) tone->get());
    copyXmlToBinary (*xml, destData);
}

void EchoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeEcho")) return;
    *timeMs = (float) xml->getDoubleAttribute ("time", 350.0);
    *feedback = (float) xml->getDoubleAttribute ("fb", 0.35);
    *mix = (float) xml->getDoubleAttribute ("mix", 0.3);
    *pingPong = xml->getIntAttribute ("pp", 0) != 0;
    *tone = (float) xml->getDoubleAttribute ("tone", 0.7);
}

juce::AudioProcessorEditor* EchoProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
