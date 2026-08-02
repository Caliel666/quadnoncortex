#include "SplitterProcessor.h"
#include "../NativePluginHelpers.h"

juce::StringArray SplitterProcessor::sourceChoices()
{
    return { "Mute", "Left", "Right", "Mono L+R", "Stereo" };
}

SplitterProcessor::SplitterProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    NativePluginHelpers::addParam (*this, modeParam = new juce::AudioParameterChoice (
        { "split_mode", 1 }, "Mode", juce::StringArray { "Split", "Join" }, 0));

    NativePluginHelpers::addParam (*this, numLanes = new juce::AudioParameterInt (
        { "split_lanes", 1 }, "Lanes", 2, kMaxLanes, 2));

    NativePluginHelpers::addParam (*this, mix = new juce::AudioParameterFloat (
        { "split_mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    // A / B / Both — MIDI learn this for lane A/B switching
    NativePluginHelpers::addParam (*this, abSelect = new juce::AudioParameterChoice (
        { "split_ab", 1 }, "A/B", juce::StringArray { "A", "B", "Both" }, 2));

    NativePluginHelpers::addParam (*this, lane0Src = new juce::AudioParameterChoice (
        { "split_s0", 1 }, "Lane A In", sourceChoices(), 1)); // Left
    NativePluginHelpers::addParam (*this, lane1Src = new juce::AudioParameterChoice (
        { "split_s1", 1 }, "Lane B In", sourceChoices(), 2)); // Right
    NativePluginHelpers::addParam (*this, lane2Src = new juce::AudioParameterChoice (
        { "split_s2", 1 }, "Lane C In", sourceChoices(), 3)); // Mono
    NativePluginHelpers::addParam (*this, lane3Src = new juce::AudioParameterChoice (
        { "split_s3", 1 }, "Lane D In", sourceChoices(), 3));
}

void SplitterProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription SplitterProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Routing", kUid);
}

bool SplitterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void SplitterProcessor::prepareToPlay (double, int) {}

SplitterProcessor::Mode SplitterProcessor::getMode() const
{
    return modeParam != nullptr && modeParam->getIndex() == 1 ? Mode::Join : Mode::Split;
}

int SplitterProcessor::getNumLanesActive() const
{
    return numLanes != nullptr ? juce::jlimit (2, kMaxLanes, numLanes->get()) : 2;
}

SplitterProcessor::Source SplitterProcessor::getLaneSource (int lane) const
{
    auto* p = srcParam (lane);
    const int idx = p != nullptr ? p->getIndex() : 0;
    return (Source) juce::jlimit (0, 4, idx);
}

int SplitterProcessor::getAbMode() const
{
    return abSelect != nullptr ? abSelect->getIndex() : 2;
}

juce::AudioParameterChoice* SplitterProcessor::srcParam (int lane) const
{
    switch (lane)
    {
        case 0: return lane0Src;
        case 1: return lane1Src;
        case 2: return lane2Src;
        case 3: return lane3Src;
        default: return nullptr;
    }
}

void SplitterProcessor::splitToLanes (const juce::AudioBuffer<float>& stereoIn,
                                      std::vector<juce::AudioBuffer<float>>& laneBufs,
                                      int numSamples) const
{
    const int nLanes = getNumLanesActive();
    const int ab = getAbMode();
    const int inCh = stereoIn.getNumChannels();
    const float* L = stereoIn.getReadPointer (0);
    const float* R = inCh > 1 ? stereoIn.getReadPointer (1) : L;

    for (int lane = 0; lane < nLanes; ++lane)
    {
        if ((int) laneBufs.size() <= lane)
            break;
        auto& buf = laneBufs[(size_t) lane];
        if (buf.getNumSamples() < numSamples)
            buf.setSize (1, numSamples, false, false, true);

        // A/B mute: ab=0 only lane0, ab=1 only lane1, ab=2 all
        bool muted = false;
        if (ab == 0 && lane != 0) muted = true;
        if (ab == 1 && lane != 1) muted = true;

        const Source src = muted ? Source::Mute : getLaneSource (lane);
        float* d = buf.getWritePointer (0);
        switch (src)
        {
            case Source::Mute:
                juce::FloatVectorOperations::clear (d, numSamples);
                break;
            case Source::Left:
                juce::FloatVectorOperations::copy (d, L, numSamples);
                break;
            case Source::Right:
                juce::FloatVectorOperations::copy (d, R, numSamples);
                break;
            case Source::MonoSum:
                for (int i = 0; i < numSamples; ++i)
                    d[i] = 0.5f * (L[i] + R[i]);
                break;
            case Source::Stereo:
                // Average for mono lane bus
                for (int i = 0; i < numSamples; ++i)
                    d[i] = 0.5f * (L[i] + R[i]);
                break;
        }
    }
}

void SplitterProcessor::joinFromLanes (const std::vector<juce::AudioBuffer<float>>& laneBufs,
                                       juce::AudioBuffer<float>& stereoOut,
                                       int numSamples) const
{
    const int nLanes = getNumLanesActive();
    const int ab = getAbMode();
    const float wet = mix != nullptr ? mix->get() : 1.0f;
    stereoOut.clear();

    for (int lane = 0; lane < nLanes; ++lane)
    {
        if (ab == 0 && lane != 0) continue;
        if (ab == 1 && lane != 1) continue;
        if ((int) laneBufs.size() <= lane) break;
        const auto& buf = laneBufs[(size_t) lane];
        if (buf.getNumSamples() < numSamples) continue;
        const float* s = buf.getReadPointer (0);
        // Lane 0 → L, Lane 1 → R, extra lanes → both (centered)
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = s[i] * wet;
            if (lane == 0)
                stereoOut.addSample (0, i, v);
            else if (lane == 1 && stereoOut.getNumChannels() > 1)
                stereoOut.addSample (1, i, v);
            else
            {
                stereoOut.addSample (0, i, v * 0.5f);
                if (stereoOut.getNumChannels() > 1)
                    stereoOut.addSample (1, i, v * 0.5f);
            }
        }
    }
}

// Fallback: as a normal insert, Splitter is transparent (chain drives split/join)
void SplitterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ignoreUnused (buffer);
    // Identity — PluginChain handles actual routing around this node
}

void SplitterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeSplitter");
    if (modeParam) xml->setAttribute ("mode", modeParam->getIndex());
    if (numLanes)  xml->setAttribute ("lanes", numLanes->get());
    if (mix)       xml->setAttribute ("mix", (double) mix->get());
    if (abSelect)  xml->setAttribute ("ab", abSelect->getIndex());
    if (lane0Src)  xml->setAttribute ("s0", lane0Src->getIndex());
    if (lane1Src)  xml->setAttribute ("s1", lane1Src->getIndex());
    if (lane2Src)  xml->setAttribute ("s2", lane2Src->getIndex());
    if (lane3Src)  xml->setAttribute ("s3", lane3Src->getIndex());
    copyXmlToBinary (*xml, destData);
}

void SplitterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeSplitter")) return;
    if (modeParam) modeParam->setValueNotifyingHost (modeParam->convertTo0to1 (xml->getIntAttribute ("mode", 0)));
    if (numLanes)  *numLanes = xml->getIntAttribute ("lanes", 2);
    if (mix)       *mix = (float) xml->getDoubleAttribute ("mix", 1.0);
    if (abSelect)  abSelect->setValueNotifyingHost (abSelect->convertTo0to1 (xml->getIntAttribute ("ab", 2)));
    if (lane0Src)  lane0Src->setValueNotifyingHost (lane0Src->convertTo0to1 (xml->getIntAttribute ("s0", 1)));
    if (lane1Src)  lane1Src->setValueNotifyingHost (lane1Src->convertTo0to1 (xml->getIntAttribute ("s1", 2)));
    if (lane2Src)  lane2Src->setValueNotifyingHost (lane2Src->convertTo0to1 (xml->getIntAttribute ("s2", 3)));
    if (lane3Src)  lane3Src->setValueNotifyingHost (lane3Src->convertTo0to1 (xml->getIntAttribute ("s3", 3)));
}

juce::AudioProcessorEditor* SplitterProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
