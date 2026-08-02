#include "PitchShifterProcessor.h"
#include "../NativePluginHelpers.h"

// Adapted from VoLum VoLumPitchShifter.h (MIT) — see LICENSE-VoLum.txt

PitchShifterProcessor::PitchShifterProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    NativePluginHelpers::addParam (*this, pitchSemis = new juce::AudioParameterInt (
        { "pitch_semi", 1 }, "Pitch", -12, 12, 0,
        juce::AudioParameterIntAttributes().withLabel ("st")));

    NativePluginHelpers::addParam (*this, character = new juce::AudioParameterChoice (
        { "pitch_char", 1 }, "Mode",
        juce::StringArray { "Instant", "Drop", "Poly" }, 2));

    NativePluginHelpers::addParam (*this, quality = new juce::AudioParameterFloat (
        { "pitch_qual", 1 }, "Quality",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));

    NativePluginHelpers::addParam (*this, tone = new juce::AudioParameterFloat (
        { "pitch_tone", 1 }, "Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    NativePluginHelpers::addParam (*this, clarity = new juce::AudioParameterFloat (
        { "pitch_clar", 1 }, "Clarity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    NativePluginHelpers::addParam (*this, mix = new juce::AudioParameterFloat (
        { "pitch_mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
}

void PitchShifterProcessor::fillInPluginDescription (juce::PluginDescription& d) const { d = makeDescription(); }
juce::PluginDescription PitchShifterProcessor::makeDescription()
{
    return NativePluginHelpers::makeDesc (kName, kId, "Modulation", kUid);
}

bool PitchShifterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in.size() <= 2 && out.size() <= 2;
}

PitchShifterProcessor::Timing PitchShifterProcessor::computeTiming (Character c, double sampleRate, float quality)
{
    const double srate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const double pDesign = srate / kDesignFmin;
    const float q = juce::jlimit (0.0f, 1.0f, quality);
    const double qScale = 0.55 + 0.9 * (double) q;
    Timing t;

    if (c == Character::Poly)
    {
        t.wsola = true;
        t.fixedGrain = true;
        t.xfade = std::max (8, (int) std::lround (srate * 0.004));
        const double bandMs = 16.0 + 12.0 * (double) q;
        t.band = (double) std::lround (srate * bandMs / 1000.0);
        t.search = std::max (1, (int) std::lround (srate * 0.016 * qScale));
        t.corrWin = std::max (8, (int) std::lround (srate * 0.016 * qScale));
        if (t.band > 1.0)
            t.search = std::min (t.search, (int) t.band - 1);
        t.dLo = (double) t.xfade + 2.0;
        t.dHi = t.dLo + t.band;
        t.latency = (int) std::lround (t.dLo + 0.5 * t.band);
    }
    else
    {
        const double xfadeMs = (c == Character::Drop) ? 5.0 : 2.5;
        t.xfade = std::max (8, (int) std::lround (srate * xfadeMs / 1000.0));
        t.fixedGrain = false;
        if (c == Character::Drop)
        {
            t.wsola = true;
            t.search = std::max (1, (int) std::lround (srate * 0.0015 * qScale));
            t.corrWin = std::max (8, (int) std::lround (0.35 * pDesign * qScale));
        }
        else
        {
            t.wsola = false;
            t.search = 0;
            t.corrWin = 0;
        }
        t.band = pDesign;
        t.dLo = (double) t.xfade + (double) (t.search + t.corrWin) + 2.0;
        t.dHi = t.dLo + t.band;
        t.latency = (int) std::lround (t.dLo + 0.5 * t.band);
    }
    return t;
}

void PitchShifterProcessor::configureVoice (Character c, float quality)
{
    auto& v = voice;
    const Timing t = computeTiming (c, sr, quality);
    const Timing worst = computeTiming (Character::Poly, sr, 1.0f);
    const int tmax = (int) std::ceil (sr / kPminFreq);
    const int maxReadDelay = (int) std::ceil (worst.dHi) + worst.search + worst.corrWin + 2;
    const int historyNeed = std::max (maxReadDelay, 2 * tmax + 2);
    const size_t need = (size_t) (historyNeed + maxBlock + 32);
    if (v.bufL.size() < need)
    {
        v.bufL.assign (need, 0.0);
        v.bufR.assign (need, 0.0);
    }
    v.periodScratch.assign ((size_t) (2 * tmax + 4), 0.0);
    v.refWin.assign ((size_t) std::max (worst.corrWin, 1), 0.0);
    v.periodUpdate = std::max (1, (int) std::lround (sr * 0.01));

    v.char_ = c;
    v.quality = quality;
    v.xfade = t.xfade;
    v.search = t.search;
    v.corrWin = t.corrWin;
    v.wsola = t.wsola;
    v.fixedGrain = t.fixedGrain;
    v.dLo = t.dLo;
    v.dHi = t.dHi;
    v.band = t.band;
    v.latency = t.latency;
    v.delay = (double) t.latency;
    v.delayNew = v.delay;
    v.fading = false;
    v.fadePos = 0;
}

void PitchShifterProcessor::resetVoice()
{
    auto& v = voice;
    std::fill (v.bufL.begin(), v.bufL.end(), 0.0);
    std::fill (v.bufR.begin(), v.bufR.end(), 0.0);
    v.write = 0;
    v.writeCount = 0;
    v.period = sr / 110.0;
    v.periodCountdown = v.periodUpdate;
    v.delay = (double) v.latency;
    v.delayNew = v.delay;
    v.fading = false;
    v.fadePos = 0;
}

void PitchShifterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max (samplesPerBlock, 64);

    dryL.assign ((size_t) maxBlock, 0.0f);
    dryR.assign ((size_t) maxBlock, 0.0f);
    wetL.assign ((size_t) maxBlock, 0.0f);
    wetR.assign ((size_t) maxBlock, 0.0f);

    const int idx = character != nullptr ? character->getIndex() : 2;
    const float q = quality != nullptr ? quality->get() : 0.55f;
    configureVoice ((Character) juce::jlimit (0, 2, idx), q);
    resetVoice();

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
    shelfL.prepare (spec); shelfR.prepare (spec);
    hpL.prepare (spec); hpR.prepare (spec);
    shelfL.reset(); shelfR.reset(); hpL.reset(); hpR.reset();
    lastTone = lastClar = 1.0e9f;
    updateFilters (0.0f, 0.25f);
}

void PitchShifterProcessor::updateFilters (float toneAmt, float clarityAmt)
{
    // Tone: low-shelf / high-shelf around 1.2 kHz, ±12 dB
    const float shelfDb = toneAmt * 12.0f;
    const float shelfHz = 1200.0f;
    *shelfL.coefficients = *Coefs::makeLowShelf (sr, shelfHz, 0.707f, juce::Decibels::decibelsToGain (-shelfDb));
    // Brighten/darken: use high shelf in the opposite direction for more audible tilt
    // Actually for guitar tone: positive tone = high shelf up, negative = high shelf down
    *shelfL.coefficients = *Coefs::makeHighShelf (sr, 2500.0f, 0.7f, juce::Decibels::decibelsToGain (shelfDb));
    *shelfR.coefficients = *shelfL.coefficients;

    // Clarity: high-pass 40 Hz .. 350 Hz
    const float hpHz = 40.0f + clarityAmt * 310.0f;
    *hpL.coefficients = *Coefs::makeHighPass (sr, hpHz, 0.707f);
    *hpR.coefficients = *hpL.coefficients;

    lastTone = toneAmt;
    lastClar = clarityAmt;
}

double PitchShifterProcessor::readAtDelay (const std::vector<double>& buf, double delay) const
{
    if (buf.empty()) return 0.0;
    const double sz = (double) buf.size();
    double rp = (double) voice.write - delay;
    while (rp < 0.0) rp += sz;
    while (rp >= sz) rp -= sz;
    const double fl = std::floor (rp);
    const size_t i0 = (size_t) fl % buf.size();
    const size_t i1 = (i0 + 1) % buf.size();
    const double frac = rp - fl;
    return buf[i0] * (1.0 - frac) + buf[i1] * frac;
}

double PitchShifterProcessor::wsolaRefine (double cand)
{
    auto& v = voice;
    const int win = v.corrWin;
    if (win <= 0 || v.writeCount < (long long) (v.dHi + win + v.search + 4))
        return cand;

    // Correlate on mono (L+R)/2
    double rn = 0.0;
    for (int j = 0; j < win; ++j)
    {
        const double val = 0.5 * (readAtDelay (v.bufL, v.delay + j) + readAtDelay (v.bufR, v.delay + j));
        v.refWin[(size_t) j] = val;
        rn += val * val;
    }
    rn = std::sqrt (rn) + 1e-9;

    double bestC = -2.0;
    int bestLag = 0;
    for (int lag = -v.search; lag <= v.search; ++lag)
    {
        const double dc = cand + lag;
        if (dc < (double) v.xfade + 1.0)
            continue;
        double dot = 0.0, sn = 0.0;
        for (int j = 0; j < win; ++j)
        {
            const double val = 0.5 * (readAtDelay (v.bufL, dc + j) + readAtDelay (v.bufR, dc + j));
            dot += v.refWin[(size_t) j] * val;
            sn += val * val;
        }
        const double cc = dot / (rn * (std::sqrt (sn) + 1e-9));
        if (cc > bestC) { bestC = cc; bestLag = lag; }
    }
    return cand + bestLag;
}

void PitchShifterProcessor::updatePeriod()
{
    auto& v = voice;
    const int tmin = std::max (2, (int) (sr / kPmaxFreq));
    const int tmax = (int) (sr / kPminFreq);
    const int len = tmax;
    const int span = tmax + len;
    if (v.writeCount < span + 2 || (int) v.periodScratch.size() < span)
        return;

    for (int k = 0; k < span; ++k)
        v.periodScratch[(size_t) k] = 0.5 * (readAtDelay (v.bufL, (double) k) + readAtDelay (v.bufR, (double) k));

    double e = 0.0;
    for (int k = 0; k < len; ++k)
        e += v.periodScratch[(size_t) k] * v.periodScratch[(size_t) k];
    if (e < 1e-7)
        return;

    double best = -1.0;
    int bestTau = tmin;
    for (int tau = tmin; tau <= tmax; ++tau)
    {
        double r = 0.0;
        for (int k = 0; k < len; ++k)
            r += v.periodScratch[(size_t) k] * v.periodScratch[(size_t) (k + tau)];
        if (r > best) { best = r; bestTau = tau; }
    }
    v.period = 0.7 * v.period + 0.3 * (double) bestTau;
}

void PitchShifterProcessor::processStereo (const float* inL, const float* inR,
                                           float* outL, float* outR, int n, bool stereo)
{
    auto& v = voice;
    if (v.bufL.empty())
    {
        std::copy (inL, inL + n, outL);
        if (stereo) std::copy (inR, inR + n, outR);
        return;
    }

    const size_t sz = v.bufL.size();
    const double f = v.ratio;
    const double grow = 1.0 - f;

    for (int i = 0; i < n; ++i)
    {
        v.bufL[v.write] = (double) inL[i];
        v.bufR[v.write] = (double) (stereo ? inR[i] : inL[i]);
        ++v.writeCount;

        if (! v.fixedGrain)
        {
            if (--v.periodCountdown <= 0)
            {
                v.periodCountdown = v.periodUpdate;
                updatePeriod();
            }
        }

        double sL = readAtDelay (v.bufL, v.delay);
        double sR = readAtDelay (v.bufR, v.delay);
        if (v.fading)
        {
            const double w = (double) v.fadePos / (double) v.xfade;
            sL = sL * (1.0 - w) + readAtDelay (v.bufL, v.delayNew) * w;
            sR = sR * (1.0 - w) + readAtDelay (v.bufR, v.delayNew) * w;
        }
        outL[i] = (float) sL;
        if (stereo) outR[i] = (float) sR;

        v.delay += grow;
        if (v.fading)
        {
            v.delayNew += grow;
            if (++v.fadePos >= v.xfade)
            {
                v.delay = v.delayNew;
                v.fading = false;
            }
        }

        if (! v.fading)
        {
            const double P = v.fixedGrain ? v.band : v.period;
            if (f < 1.0 && v.delay > v.dHi)
            {
                const int k = std::max (1, (int) std::lround ((v.delay - v.dLo) / P));
                double cand = v.delay - k * P;
                if (v.wsola) cand = wsolaRefine (cand);
                if (cand >= (double) v.xfade + 1.0)
                {
                    v.delayNew = cand;
                    v.fading = true;
                    v.fadePos = 0;
                }
            }
            else if (f > 1.0 && v.delay < v.dLo)
            {
                const int k = std::max (1, (int) std::lround ((v.dHi - v.delay) / P));
                const double base = v.delay + k * P;
                double cand = base;
                if (v.wsola) cand = wsolaRefine (cand);
                if (cand < base) cand = base;
                v.delayNew = cand;
                v.fading = true;
                v.fadePos = 0;
            }
        }

        v.write = (v.write + 1) % sz;
    }
}

void PitchShifterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0) return;

    // Grow scratch if host exceeds prepare size (rare)
    if ((int) dryL.size() < n)
    {
        dryL.resize ((size_t) n);
        dryR.resize ((size_t) n);
        wetL.resize ((size_t) n);
        wetR.resize ((size_t) n);
    }

    const int semis = pitchSemis != nullptr ? pitchSemis->get() : 0;
    const float wetAmt = mix != nullptr ? mix->get() : 1.0f;
    const int cIdx = character != nullptr ? character->getIndex() : 2;
    const Character c = (Character) juce::jlimit (0, 2, cIdx);
    const float q = quality != nullptr ? quality->get() : 0.55f;
    const float toneAmt = tone != nullptr ? tone->get() : 0.0f;
    const float clarAmt = clarity != nullptr ? clarity->get() : 0.25f;

    if (semis == 0 || wetAmt < 0.001f)
        return;

    if (voice.char_ != c || std::abs (voice.quality - q) > 0.04f)
    {
        configureVoice (c, q);
        voice.delay = (double) voice.latency;
        voice.fading = false;
    }

    voice.ratio = std::pow (2.0, (double) semis / 12.0);

    for (int i = 0; i < n; ++i)
    {
        dryL[(size_t) i] = buffer.getSample (0, i);
        dryR[(size_t) i] = nCh > 1 ? buffer.getSample (1, i) : dryL[(size_t) i];
    }

    processStereo (dryL.data(), dryR.data(), wetL.data(), wetR.data(), n, nCh > 1);

    // Tone + clarity on WET only (audible: ±12 dB shelf, HP up to 350 Hz)
    if (std::abs (toneAmt - lastTone) > 0.01f || std::abs (clarAmt - lastClar) > 0.01f)
        updateFilters (toneAmt, clarAmt);

    const bool doTone = std::abs (toneAmt) > 0.001f;
    const bool doClar = clarAmt > 0.001f;

    for (int i = 0; i < n; ++i)
    {
        float wL = wetL[(size_t) i];
        float wR = wetR[(size_t) i];

        if (doClar)
        {
            wL = hpL.processSample (wL);
            wR = hpR.processSample (wR);
        }
        if (doTone)
        {
            wL = shelfL.processSample (wL);
            wR = shelfR.processSample (wR);
        }

        const float oL = (1.0f - wetAmt) * dryL[(size_t) i] + wetAmt * wL;
        const float oR = (1.0f - wetAmt) * dryR[(size_t) i] + wetAmt * wR;
        buffer.setSample (0, i, oL);
        if (nCh > 1) buffer.setSample (1, i, oR);
    }
}

void PitchShifterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativePitch");
    xml->setAttribute ("semi", pitchSemis ? pitchSemis->get() : 0);
    xml->setAttribute ("char", character ? character->getIndex() : 2);
    xml->setAttribute ("qual", (double) (quality ? quality->get() : 0.55f));
    xml->setAttribute ("tone", (double) (tone ? tone->get() : 0.0f));
    xml->setAttribute ("clar", (double) (clarity ? clarity->get() : 0.25f));
    xml->setAttribute ("mix", (double) (mix ? mix->get() : 1.0f));
    copyXmlToBinary (*xml, destData);
}

void PitchShifterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativePitch")) return;
    if (pitchSemis) *pitchSemis = xml->getIntAttribute ("semi", 0);
    if (character)  character->setValueNotifyingHost (character->convertTo0to1 (xml->getIntAttribute ("char", 2)));
    if (quality)    *quality = (float) xml->getDoubleAttribute ("qual", 0.55);
    if (tone)       *tone = (float) xml->getDoubleAttribute ("tone", 0.0);
    if (clarity)    *clarity = (float) xml->getDoubleAttribute ("clar", 0.25);
    if (mix)        *mix = (float) xml->getDoubleAttribute ("mix", 1.0);
}

juce::AudioProcessorEditor* PitchShifterProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
