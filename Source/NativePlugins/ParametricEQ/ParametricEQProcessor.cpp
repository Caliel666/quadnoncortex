#include "ParametricEQProcessor.h"
#include "../NativePluginHelpers.h"

ParametricEQProcessor::ParametricEQProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    const float defF[4] = { 100.0f, 500.0f, 2000.0f, 8000.0f };
    const char* names[4] = { "Low Shelf", "Low Mid", "High Mid", "High Shelf" };
    for (int b = 0; b < 4; ++b)
    {
        NativePluginHelpers::addParam (*this, freq[b] = new juce::AudioParameterFloat (
            { "eq_f" + juce::String (b), 1 }, juce::String (names[b]) + " Freq",
            juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.35f), defF[b]));
        NativePluginHelpers::addParam (*this, gain[b] = new juce::AudioParameterFloat (
            { "eq_g" + juce::String (b), 1 }, juce::String (names[b]) + " Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
        NativePluginHelpers::addParam (*this, q[b] = new juce::AudioParameterFloat (
            { "eq_q" + juce::String (b), 1 }, juce::String (names[b]) + " Q",
            juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.4f), 0.7f));
    }
    NativePluginHelpers::addParam (*this, outputGain = new juce::AudioParameterFloat (
        { "eq_out", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
}

void ParametricEQProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription ParametricEQProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "EQ", kUid);
}

bool ParametricEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void ParametricEQProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
    updateFilters();
}

void ParametricEQProcessor::updateFilters()
{
    auto setBand = [this] (int b, Filter& L, Filter& R)
    {
        const float f = freq[b]->get();
        const float g = juce::Decibels::decibelsToGain (gain[b]->get());
        const float qq = q[b]->get();
        juce::ReferenceCountedObjectPtr<Coeffs> c;
        if (b == 0)
            c = Coeffs::makeLowShelf (sr, f, qq, g);
        else if (b == 3)
            c = Coeffs::makeHighShelf (sr, f, qq, g);
        else
            c = Coeffs::makePeakFilter (sr, f, qq, g);
        *L.coefficients = *c;
        *R.coefficients = *c;
    };
    for (int b = 0; b < 4; ++b)
        setBand (b, filtersL[(size_t) b], filtersR[(size_t) b]);
}

void ParametricEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    updateFilters();
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    const float outG = juce::Decibels::decibelsToGain (outputGain->get());

    for (int i = 0; i < n; ++i)
    {
        float L = buffer.getSample (0, i);
        float R = nCh > 1 ? buffer.getSample (1, i) : L;
        for (int b = 0; b < 4; ++b)
        {
            L = filtersL[(size_t) b].processSample (L);
            R = filtersR[(size_t) b].processSample (R);
        }
        buffer.setSample (0, i, L * outG);
        if (nCh > 1) buffer.setSample (1, i, R * outG);
    }
}

void ParametricEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeEQ");
    for (int b = 0; b < 4; ++b)
    {
        xml->setAttribute ("f" + juce::String (b), (double) freq[b]->get());
        xml->setAttribute ("g" + juce::String (b), (double) gain[b]->get());
        xml->setAttribute ("q" + juce::String (b), (double) q[b]->get());
    }
    xml->setAttribute ("out", (double) outputGain->get());
    copyXmlToBinary (*xml, destData);
}

void ParametricEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeEQ")) return;
    for (int b = 0; b < 4; ++b)
    {
        *freq[b] = (float) xml->getDoubleAttribute ("f" + juce::String (b), freq[b]->get());
        *gain[b] = (float) xml->getDoubleAttribute ("g" + juce::String (b), 0.0);
        *q[b]    = (float) xml->getDoubleAttribute ("q" + juce::String (b), 0.7);
    }
    *outputGain = (float) xml->getDoubleAttribute ("out", 0.0);
}

juce::AudioProcessorEditor* ParametricEQProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
