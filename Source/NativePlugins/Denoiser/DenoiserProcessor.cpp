#include "DenoiserProcessor.h"
#include "../NativePluginHelpers.h"
#include <cmath>

DenoiserProcessor::DenoiserProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Master amount: 0 dB = denoise OFF (band sliders do nothing)
    //                -80 dB = full application of band sliders
    NativePluginHelpers::addParam (*this, threshold = new juce::AudioParameterFloat (
        { "dn_th", 1 }, "Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), 0.0f));

    const char* bandNames[4] = {
        "Hum / Ground",
        "Mid Grounding",
        "Preamp",
        "Hiss"
    };
    const float defAmt[4] = { 0.55f, 0.85f, 0.70f, 0.75f };
    for (int b = 0; b < kBands; ++b)
        NativePluginHelpers::addParam (*this, bandAmt[b] = new juce::AudioParameterFloat (
            { "dn_b" + juce::String (b), 1 }, bandNames[b],
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), defAmt[b]));

    NativePluginHelpers::addParam (*this, hfBias = new juce::AudioParameterBool (
        { "dn_hfbias", 1 }, "HF Bias", true));

    NativePluginHelpers::addParam (*this, gateOn = new juce::AudioParameterBool (
        { "dn_gate", 1 }, "Gate", false));
    NativePluginHelpers::addParam (*this, gateThresh = new juce::AudioParameterFloat (
        { "dn_gth", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f));
    NativePluginHelpers::addParam (*this, gateHard = new juce::AudioParameterBool (
        { "dn_ghard", 1 }, "Hard Gate", true));
    NativePluginHelpers::addParam (*this, gateAttack = new juce::AudioParameterFloat (
        { "dn_gatk", 1 }, "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.1f, 0.4f), 1.0f));
    NativePluginHelpers::addParam (*this, gateRelease = new juce::AudioParameterFloat (
        { "dn_grel", 1 }, "Gate Release",
        juce::NormalisableRange<float> (5.0f, 500.0f, 1.0f, 0.4f), 80.0f));
}

void DenoiserProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription DenoiserProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Dynamics", kUid);
}

bool DenoiserProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

void DenoiserProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    blockCounter = 0;
    for (int b = 0; b < kBands; ++b)
    {
        lastCutDb[b] = 0.0f;
        smoothCut[b] = 0.0f;
        dynL[b].reset();
        dynR[b].reset();
        setBandCut (b, 0.0f);
    }
    gateEnv = 0.0f;
    gateGain = 1.0f;
}

void DenoiserProcessor::setBandCut (int b, float cutDb)
{
    cutDb = juce::jlimit (-30.0f, 0.0f, cutDb);
    const float g = juce::Decibels::decibelsToGain (cutDb);
    auto c = Coeffs::makePeakFilter (sr, kCentre[b], kQ[b], g);
    *dynL[b].coefficients = *c;
    *dynR[b].coefficients = *c;
    lastCutDb[b] = cutDb;
}

float DenoiserProcessor::processGate (float peak)
{
    if (! gateOn->get())
    {
        gateGain = 1.0f;
        return 1.0f;
    }

    const float th = juce::Decibels::decibelsToGain (gateThresh->get());
    const float atk = std::exp (-1.0f / (0.001f * juce::jmax (0.1f, gateAttack->get()) * (float) sr));
    const float rel = std::exp (-1.0f / (0.001f * juce::jmax (1.0f, gateRelease->get()) * (float) sr));

    if (peak > gateEnv) gateEnv = atk * gateEnv + (1.0f - atk) * peak;
    else                gateEnv = rel * gateEnv + (1.0f - rel) * peak;

    float target = 1.0f;
    if (gateHard->get())
    {
        const float hyst = th * 0.65f;
        if (gateEnv > th)        target = 1.0f;
        else if (gateEnv < hyst) target = 0.0f;
        else                     target = gateGain;
    }
    else
    {
        if (gateEnv <= 1.0e-9f)
            target = 0.0f;
        else if (gateEnv < th)
        {
            const float over = juce::Decibels::gainToDecibels (gateEnv) - gateThresh->get();
            target = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (over * 0.75f));
        }
        else
            target = 1.0f;
    }

    const float coeff = (target < gateGain) ? rel : atk;
    gateGain = coeff * gateGain + (1.0f - coeff) * target;
    return gateGain;
}

void DenoiserProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();

    // Threshold → master amount
    // 0 dB = 0% (band sliders inert), -80 dB = 100%
    const float thDb = threshold->get();
    const float amount = juce::jlimit (0.0f, 1.0f, -thDb / 80.0f);

    const bool bias = hfBias->get();

    // Target cuts from amount * band sliders (static — no per-note jumping)
    float targetCut[kBands];
    for (int b = 0; b < kBands; ++b)
    {
        float amt = bandAmt[b]->get();
        if (bias && b >= 1)
            amt = juce::jmin (1.0f, amt * 1.2f);
        // amount==0 → cut==0 regardless of bandAmt
        targetCut[b] = -26.0f * amount * amt;
    }

    // Block-rate smoothing (~25 ms) — coeff must use block length, not 1 sample
    const float blockSec = (float) n / (float) sr;
    const float smoothCoeff = std::exp (-blockSec / 0.025f);

    for (int b = 0; b < kBands; ++b)
    {
        smoothCut[b] = smoothCoeff * smoothCut[b] + (1.0f - smoothCoeff) * targetCut[b];
        // Apply immediately when meaningful change (or fully off)
        if (std::abs (smoothCut[b] - lastCutDb[b]) > 0.03f || amount < 0.001f)
            setBandCut (b, amount < 0.001f ? 0.0f : smoothCut[b]);
    }

    const bool denoiseActive = amount > 0.001f;
    juce::ignoreUnused (blockCounter);

    for (int i = 0; i < n; ++i)
    {
        float L = buffer.getSample (0, i);
        float R = nCh > 1 ? buffer.getSample (1, i) : L;

        if (denoiseActive)
        {
            for (int b = 0; b < kBands; ++b)
            {
                L = dynL[b].processSample (L);
                R = dynR[b].processSample (R);
            }
        }

        const float g = processGate (juce::jmax (std::abs (L), std::abs (R)));
        L *= g;
        R *= g;

        buffer.setSample (0, i, L);
        if (nCh > 1) buffer.setSample (1, i, R);
    }
}

void DenoiserProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeDenoiser");
    xml->setAttribute ("th", (double) threshold->get());
    for (int b = 0; b < kBands; ++b)
        xml->setAttribute ("b" + juce::String (b), (double) bandAmt[b]->get());
    xml->setAttribute ("hfbias", hfBias->get() ? 1 : 0);
    xml->setAttribute ("gate", gateOn->get() ? 1 : 0);
    xml->setAttribute ("gth", (double) gateThresh->get());
    xml->setAttribute ("ghard", gateHard->get() ? 1 : 0);
    xml->setAttribute ("gatk", (double) gateAttack->get());
    xml->setAttribute ("grel", (double) gateRelease->get());
    copyXmlToBinary (*xml, destData);
}

void DenoiserProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeDenoiser")) return;
    *threshold = (float) xml->getDoubleAttribute ("th", 0.0);
    for (int b = 0; b < kBands; ++b)
        *bandAmt[b] = (float) xml->getDoubleAttribute ("b" + juce::String (b), bandAmt[b]->get());
    *hfBias = xml->getIntAttribute ("hfbias", 1) != 0;
    *gateOn = xml->getIntAttribute ("gate", 0) != 0;
    *gateThresh = (float) xml->getDoubleAttribute ("gth", -40.0);
    *gateHard = xml->getIntAttribute ("ghard", 1) != 0;
    *gateAttack = (float) xml->getDoubleAttribute ("gatk", 1.0);
    *gateRelease = (float) xml->getDoubleAttribute ("grel", 80.0);
}

juce::AudioProcessorEditor* DenoiserProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
