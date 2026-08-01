#include "CompressorProcessor.h"
#include "../NativePluginHelpers.h"
#include <cmath>

CompressorProcessor::CompressorProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    NativePluginHelpers::addParam (*this, threshold = new juce::AudioParameterFloat (
        { "cmp_th", 1 }, "Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f));
    NativePluginHelpers::addParam (*this, ratio = new juce::AudioParameterFloat (
        { "cmp_ratio", 1 }, "Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.4f), 4.0f));
    NativePluginHelpers::addParam (*this, attackMs = new juce::AudioParameterFloat (
        { "cmp_atk", 1 }, "Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.4f), 10.0f));
    NativePluginHelpers::addParam (*this, releaseMs = new juce::AudioParameterFloat (
        { "cmp_rel", 1 }, "Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 120.0f));
    NativePluginHelpers::addParam (*this, makeup = new juce::AudioParameterFloat (
        { "cmp_mu", 1 }, "Makeup",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 6.0f));
    NativePluginHelpers::addParam (*this, mix = new juce::AudioParameterFloat (
        { "cmp_mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
}

void CompressorProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription CompressorProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Dynamics", kUid);
}

bool CompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void CompressorProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    env = 0.0f;
    gainSmoothed = 1.0f;
}

void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();

    const float thDb = threshold->get();
    const float thLin = juce::Decibels::decibelsToGain (thDb);
    const float rat = juce::jmax (1.0f, ratio->get());
    const float atk = std::exp (-1.0f / (0.001f * attackMs->get() * (float) sr));
    const float rel = std::exp (-1.0f / (0.001f * releaseMs->get() * (float) sr));
    const float mu = juce::Decibels::decibelsToGain (makeup->get());
    const float wet = mix->get();
    const float dry = 1.0f - wet;

    for (int i = 0; i < n; ++i)
    {
        float L = buffer.getSample (0, i);
        float R = nCh > 1 ? buffer.getSample (1, i) : L;
        const float peak = juce::jmax (std::abs (L), std::abs (R));

        // Soft envelope
        if (peak > env) env = atk * env + (1.0f - atk) * peak;
        else            env = rel * env + (1.0f - rel) * peak;

        // Soft-knee gain computer
        float g = 1.0f;
        if (env > thLin * 0.5f)
        {
            const float over = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f)) - thDb;
            const float knee = 6.0f; // dB soft knee
            float gr = 0.0f;
            if (over <= -knee * 0.5f)
                gr = 0.0f;
            else if (over >= knee * 0.5f)
                gr = over * (1.0f - 1.0f / rat);
            else
            {
                const float x = over + knee * 0.5f;
                gr = (x * x) / (2.0f * knee) * (1.0f - 1.0f / rat);
            }
            g = juce::Decibels::decibelsToGain (-gr);
        }

        gainSmoothed = 0.9f * gainSmoothed + 0.1f * g;
        const float apply = gainSmoothed * mu;

        buffer.setSample (0, i, dry * L + wet * L * apply);
        if (nCh > 1)
            buffer.setSample (1, i, dry * R + wet * R * apply);
    }
}

void CompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeComp");
    xml->setAttribute ("th", (double) threshold->get());
    xml->setAttribute ("ratio", (double) ratio->get());
    xml->setAttribute ("atk", (double) attackMs->get());
    xml->setAttribute ("rel", (double) releaseMs->get());
    xml->setAttribute ("mu", (double) makeup->get());
    xml->setAttribute ("mix", (double) mix->get());
    copyXmlToBinary (*xml, destData);
}

void CompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeComp")) return;
    *threshold = (float) xml->getDoubleAttribute ("th", -24.0);
    *ratio = (float) xml->getDoubleAttribute ("ratio", 4.0);
    *attackMs = (float) xml->getDoubleAttribute ("atk", 10.0);
    *releaseMs = (float) xml->getDoubleAttribute ("rel", 120.0);
    *makeup = (float) xml->getDoubleAttribute ("mu", 6.0);
    *mix = (float) xml->getDoubleAttribute ("mix", 1.0);
}

juce::AudioProcessorEditor* CompressorProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
