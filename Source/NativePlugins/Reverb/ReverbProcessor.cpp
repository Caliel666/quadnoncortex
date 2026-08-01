#include "ReverbProcessor.h"
#include "../NativePluginHelpers.h"

ReverbProcessor::ReverbProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    NativePluginHelpers::addParam (*this, size = new juce::AudioParameterFloat (
        { "rv_size", 1 }, "Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    NativePluginHelpers::addParam (*this, damping = new juce::AudioParameterFloat (
        { "rv_damp", 1 }, "Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));
    NativePluginHelpers::addParam (*this, width = new juce::AudioParameterFloat (
        { "rv_width", 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    NativePluginHelpers::addParam (*this, mix = new juce::AudioParameterFloat (
        { "rv_mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));
    NativePluginHelpers::addParam (*this, freeze = new juce::AudioParameterFloat (
        { "rv_freeze", 1 }, "Freeze",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
}

void ReverbProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription ReverbProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Reverb", kUid);
}

bool ReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void ReverbProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    reverb.reset();
    reverb.setSampleRate (sampleRate);
    applyParams();
}

void ReverbProcessor::applyParams()
{
    juce::Reverb::Parameters p;
    p.roomSize   = size->get();
    p.damping    = damping->get();
    p.width      = width->get();
    p.freezeMode = freeze->get();
    const float m = mix->get();
    p.wetLevel = m;
    p.dryLevel = 1.0f - m * 0.85f; // keep some dry
    reverb.setParameters (p);
}

void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    applyParams();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    else
        reverb.processMono (buffer.getWritePointer (0), n);
}

void ReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeReverb");
    xml->setAttribute ("size", (double) size->get());
    xml->setAttribute ("damp", (double) damping->get());
    xml->setAttribute ("width", (double) width->get());
    xml->setAttribute ("mix", (double) mix->get());
    xml->setAttribute ("freeze", (double) freeze->get());
    copyXmlToBinary (*xml, destData);
}

void ReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeReverb")) return;
    *size = (float) xml->getDoubleAttribute ("size", 0.5);
    *damping = (float) xml->getDoubleAttribute ("damp", 0.4);
    *width = (float) xml->getDoubleAttribute ("width", 1.0);
    *mix = (float) xml->getDoubleAttribute ("mix", 0.25);
    *freeze = (float) xml->getDoubleAttribute ("freeze", 0.0);
}

juce::AudioProcessorEditor* ReverbProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
