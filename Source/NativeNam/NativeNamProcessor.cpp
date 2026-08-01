#include "NativeNamProcessor.h"
#include "NativeNamPanel.h"
#include "MidiLearnManager.h"
#include "DevLog.h"

#if QUADNONCORTEX_HAS_NAM
 #include "get_dsp.h"
 #include "slimmable.h"
 #include <filesystem>
#endif


namespace {
// AudioPluginInstance hides addParameter; route through AudioProcessor& (public).
void addParam (juce::AudioProcessor& p, juce::AudioProcessorParameter* param)
{
    p.addParameter (param);
}
}
//==============================================================================
NativeNamProcessor::NativeNamProcessor()
    : AudioPluginInstance (BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // AudioPluginInstance hides addParameter; free function uses public AudioProcessor::addParameter
    addParam (*this, inputGainParam  = new juce::AudioParameterFloat (
        { "nn_input", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    addParam (*this, outputGainParam = new juce::AudioParameterFloat (
        { "nn_output", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    addParam (*this, bypassPedalParam = new juce::AudioParameterBool (
        { "nn_bypass_pedal", 1 }, "Bypass Pedal", false));

    addParam (*this, bypassAmpParam = new juce::AudioParameterBool (
        { "nn_bypass_amp", 1 }, "Bypass Amp", false));

    addParam (*this, bypassCabParam = new juce::AudioParameterBool (
        { "nn_bypass_cab", 1 }, "Bypass Cab", false));

    addParam (*this, liteModeParam = new juce::AudioParameterBool (
        { "nn_lite_mode", 1 }, "Lite Mode", false));

    addParam (*this, pedalMixParam = new juce::AudioParameterFloat (
        { "nn_pedal_mix", 1 }, "Pedal Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    addParam (*this, ampGainParam = new juce::AudioParameterFloat (
        { "nn_amp_gain", 1 }, "Amp Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // Official NAM tone stack: 0..10 knobs, centre = 5 (flat). Maps to shelf/peak dB.
    addParam (*this, ampLowParam = new juce::AudioParameterFloat (
        { "nn_amp_bass", 1 }, "Bass",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    addParam (*this, ampMidParam = new juce::AudioParameterFloat (
        { "nn_amp_mid", 1 }, "Middle",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));

    addParam (*this, ampHighParam = new juce::AudioParameterFloat (
        { "nn_amp_treble", 1 }, "Treble",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f));
}

NativeNamProcessor::~NativeNamProcessor() = default;

void NativeNamProcessor::fillInPluginDescription (juce::PluginDescription& d) const
{
    d = makeDescription();
}

juce::PluginDescription NativeNamProcessor::makeDescription()
{
    juce::PluginDescription d;
    d.name              = kName;
    d.descriptiveName   = "Built-in NAM2 pedal / amp / cabinet";
    d.pluginFormatName  = kFormatName;
    d.category          = "Amp Sim";
    d.manufacturerName  = kManufacturer;
    d.version           = "1.0";
    d.fileOrIdentifier  = "internal://native-nam";
    d.uniqueId          = 0x4E414D32; // 'NAM2'
    d.isInstrument      = false;
    d.numInputChannels  = 2;
    d.numOutputChannels = 2;
    return d;
}

bool NativeNamProcessor::isNativeNam (const juce::AudioPluginInstance* inst)
{
    return inst != nullptr
        && (dynamic_cast<const NativeNamProcessor*> (inst) != nullptr
            || inst->getName() == kName);
}

bool NativeNamProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in.size() > 2 || out.size() > 2) return false;
    return true;
}

//==============================================================================
void NativeNamProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    currentBs = juce::jmax (samplesPerBlock, 32);

    monoScratch.setSize (1, currentBs);
    inDoubles.assign ((size_t) currentBs, 0.0);
    outDoubles.assign ((size_t) currentBs, 0.0);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) currentBs;
    spec.numChannels      = 1;
    cabConv.prepare (spec);
    cabConv.reset();

    dryScratch.assign ((size_t) currentBs, 0.0f);
    ampLowFilter.reset();
    ampMidFilter.reset();
    ampHighFilter.reset();
    updateAmpEq();

   #if QUADNONCORTEX_HAS_NAM
    auto resetSlot = [this] (NamSlot& s)
    {
        const juce::ScopedLock sl (s.lock);
        if (s.dsp != nullptr)
            s.dsp->Reset (currentSr, currentBs);
    };
    resetSlot (pedal);
    resetSlot (amp);
   #endif

    if (cabPath.existsAsFile())
        reloadCabIR();
}

void NativeNamProcessor::releaseResources()
{
    cabConv.reset();
}


void NativeNamProcessor::updateAmpEq()
{
    // Match NeuralAmpModelerPlugin BasicNamToneStack (ToneStack.cpp):
    // Bass 150 Hz low-shelf  ±20 dB  (gain = 4*(v-5))
    // Mid  425 Hz peak       ±15 dB  (gain = 3*(v-5)), Q=1.5 cut / 0.7 boost
    // Treble 1800 Hz high-shelf ±10 dB (gain = 2*(v-5))
    const double sr = currentSr > 0.0 ? currentSr : 48000.0;
    const float bassKnob   = ampLowParam  ? ampLowParam->get()  : 5.0f;
    const float midKnob    = ampMidParam  ? ampMidParam->get()  : 5.0f;
    const float trebleKnob = ampHighParam ? ampHighParam->get() : 5.0f;

    const float bassDb   = 4.0f * (bassKnob   - 5.0f);
    const float midDb    = 3.0f * (midKnob    - 5.0f);
    const float trebleDb = 2.0f * (trebleKnob - 5.0f);
    const float midQ     = midDb < 0.0f ? 1.5f : 0.7f;

    *ampLowFilter.coefficients  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sr, 150.0, 0.707f, juce::Decibels::decibelsToGain (bassDb));
    *ampMidFilter.coefficients  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sr, 425.0, midQ, juce::Decibels::decibelsToGain (midDb));
    *ampHighFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sr, 1800.0, 0.707f, juce::Decibels::decibelsToGain (trebleDb));
}

void NativeNamProcessor::applySlimMode()
{
    applySlimModeToSlot (pedal);
    applySlimModeToSlot (amp);
}

void NativeNamProcessor::applySlimModeToSlot (NamSlot& slot)
{
   #if QUADNONCORTEX_HAS_NAM
    const juce::ScopedLock sl (slot.lock);
    if (slot.dsp == nullptr) return;
    if (auto* slim = dynamic_cast<nam::SlimmableModel*> (slot.dsp.get()))
    {
        // NAM2 A2: 0.0..0.5 = Lite (3-ch), 0.6..1.0 = Full (8-ch)
        const double v = liteModeParam->get() ? 0.0 : 1.0;
        slim->SetSlimmableSize (v);
        DevLog::log ("NativeNam SetSlimmableSize " + juce::String (v));
    }
   #else
    juce::ignoreUnused (slot);
   #endif
}

void NativeNamProcessor::processNamSlot (NamSlot& slot, float* data, int n)
{
   #if QUADNONCORTEX_HAS_NAM
    const juce::ScopedLock sl (slot.lock);
    if (slot.dsp == nullptr) return;

    if ((int) inDoubles.size() < n)
    {
        inDoubles.resize ((size_t) n);
        outDoubles.resize ((size_t) n);
    }

    for (int i = 0; i < n; ++i)
        inDoubles[(size_t) i] = (double) data[i];

    double* inP  = inDoubles.data();
    double* outP = outDoubles.data();

    try
    {
        slot.dsp->process (&inP, &outP, n);
        for (int i = 0; i < n; ++i)
            data[i] = (float) outDoubles[(size_t) i];
    }
    catch (...)
    {
        DevLog::log ("NativeNam: process exception");
    }
   #else
    juce::ignoreUnused (slot, data, n);
   #endif
}

void NativeNamProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n   = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh <= 0) return;

    const float inG  = juce::Decibels::decibelsToGain (inputGainParam->get());
    const float outG = juce::Decibels::decibelsToGain (outputGainParam->get());
    monoScratch.setSize (1, n, false, false, true);
    monoScratch.clear();

    // Mono sum with input gain
    for (int i = 0; i < n; ++i)
    {
        float s = buffer.getSample (0, i) * inG;
        if (nCh > 1)
            s = 0.5f * (s + buffer.getSample (1, i) * inG);
        monoScratch.setSample (0, i, s);
    }

    float* m = monoScratch.getWritePointer (0);

    // Pedal with dry/wet mix (Pedal Mix 0 = dry, 1 = full pedal)
    if (! bypassPedalParam->get() && pedal.name.isNotEmpty())
    {
        const float mix = pedalMixParam->get();
        if (mix <= 0.001f)
        {
            // fully dry — skip NAM
        }
        else
        {
            if ((int) dryScratch.size() < n)
                dryScratch.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
                dryScratch[(size_t) i] = m[i];

            processNamSlot (pedal, m, n);

            if (mix < 0.999f)
            {
                const float dry = 1.0f - mix;
                for (int i = 0; i < n; ++i)
                    m[i] = dry * dryScratch[(size_t) i] + mix * m[i];
            }
        }
    }

    // Amp + tone stack
    if (! bypassAmpParam->get() && amp.name.isNotEmpty())
    {
        processNamSlot (amp, m, n);
        const float ag = juce::Decibels::decibelsToGain (ampGainParam->get());
        if (std::abs (ag - 1.0f) > 0.001f)
            for (int i = 0; i < n; ++i)
                m[i] *= ag;

        // 3-band EQ (mono)
        updateAmpEq();
        for (int i = 0; i < n; ++i)
        {
            float s = m[i];
            s = ampLowFilter.processSample (s);
            s = ampMidFilter.processSample (s);
            s = ampHighFilter.processSample (s);
            m[i] = s;
        }
    }

    if (! bypassCabParam->get() && cabLoaded)
    {
        juce::dsp::AudioBlock<float> block (monoScratch);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        cabConv.process (ctx);
        m = monoScratch.getWritePointer (0);
    }

    for (int ch = 0; ch < nCh; ++ch)
    {
        buffer.copyFrom (ch, 0, m, n);
        buffer.applyGain (ch, 0, n, outG);
    }
}

//==============================================================================
bool NativeNamProcessor::loadModel (Slot slot, const juce::File& file)
{
    lastError.clear();
    if (! file.existsAsFile())
    {
        lastError = "File not found";
        return false;
    }

    if (slot == Slot::Cab)
    {
        cabPath = file;
        cabName = file.getFileNameWithoutExtension();
        reloadCabIR();
        if (onModelsChanged) onModelsChanged();
        return cabLoaded;
    }

   #if QUADNONCORTEX_HAS_NAM
    NamSlot& s = (slot == Slot::Pedal) ? pedal : amp;
    try
    {
        nam::DspLoadOptions opts;
        opts.prewarm = false;
        auto dsp = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()), opts);
        if (dsp == nullptr)
        {
            lastError = "NAM loader returned null";
            return false;
        }
        dsp->Reset (currentSr, currentBs);
        {
            const juce::ScopedLock sl (s.lock);
            s.dsp = std::move (dsp);
            s.path = file;
            s.name = file.getFileNameWithoutExtension();
        }
        DevLog::log ("NativeNam loaded " + s.name);
        applySlimModeToSlot (s);
        if (onModelsChanged) onModelsChanged();
        return true;
    }
    catch (const std::exception& ex)
    {
        lastError = juce::String ("Load failed: ") + ex.what();
        DevLog::log ("NativeNam " + lastError);
        return false;
    }
    catch (...)
    {
        lastError = "Load failed (unknown)";
        return false;
    }
   #else
    NamSlot& s = (slot == Slot::Pedal) ? pedal : amp;
    s.path = file;
    s.name = file.getFileNameWithoutExtension();
    lastError = "Built without NeuralAmpModelerCore — model path stored only";
    if (onModelsChanged) onModelsChanged();
    return true;
   #endif
}

void NativeNamProcessor::clearModel (Slot slot)
{
    if (slot == Slot::Cab)
    {
        cabPath = {};
        cabName = {};
        cabLoaded = false;
        cabConv.reset();
    }
    else
    {
        NamSlot& s = (slot == Slot::Pedal) ? pedal : amp;
        const juce::ScopedLock sl (s.lock);
       #if QUADNONCORTEX_HAS_NAM
        s.dsp.reset();
       #endif
        s.path = {};
        s.name = {};
    }
    if (onModelsChanged) onModelsChanged();
}

juce::File NativeNamProcessor::getModelFile (Slot slot) const
{
    switch (slot)
    {
        case Slot::Pedal: return pedal.path;
        case Slot::Amp:   return amp.path;
        case Slot::Cab:   return cabPath;
    }
    return {};
}

juce::String NativeNamProcessor::getModelName (Slot slot) const
{
    switch (slot)
    {
        case Slot::Pedal: return pedal.name;
        case Slot::Amp:   return amp.name;
        case Slot::Cab:   return cabName;
    }
    return {};
}

void NativeNamProcessor::reloadCabIR()
{
    cabLoaded = false;
    cabConv.reset();
    if (! cabPath.existsAsFile()) return;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (cabPath));
    if (reader == nullptr)
    {
        lastError = "Cannot read IR";
        return;
    }

    juce::AudioBuffer<float> ir ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&ir, 0, (int) reader->lengthInSamples, 0, true, true);

    // Use mono IR (sum if stereo)
    juce::AudioBuffer<float> mono (1, ir.getNumSamples());
    mono.copyFrom (0, 0, ir, 0, 0, ir.getNumSamples());
    if (ir.getNumChannels() > 1)
        mono.addFrom (0, 0, ir, 1, 0, ir.getNumSamples(), 0.5f);

    cabConv.loadImpulseResponse (std::move (mono),
                                 reader->sampleRate,
                                 juce::dsp::Convolution::Stereo::no,
                                 juce::dsp::Convolution::Trim::yes,
                                 juce::dsp::Convolution::Normalise::yes);
    cabLoaded = true;
    DevLog::log ("NativeNam cab IR loaded: " + cabName);
}

//==============================================================================
void NativeNamProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("NativeNAM");
    xml->setAttribute ("inputGain",  (double) inputGainParam->get());
    xml->setAttribute ("outputGain", (double) outputGainParam->get());
    xml->setAttribute ("bypassPedal", bypassPedalParam->get() ? 1 : 0);
    xml->setAttribute ("bypassAmp",   bypassAmpParam->get()   ? 1 : 0);
    xml->setAttribute ("bypassCab",   bypassCabParam->get()   ? 1 : 0);
    xml->setAttribute ("liteMode",    liteModeParam->get()    ? 1 : 0);
    xml->setAttribute ("pedalMix",    (double) pedalMixParam->get());
    xml->setAttribute ("ampGain",     (double) ampGainParam->get());
    xml->setAttribute ("bass",        (double) ampLowParam->get());
    xml->setAttribute ("middle",      (double) ampMidParam->get());
    xml->setAttribute ("treble",      (double) ampHighParam->get());
    xml->setAttribute ("pedal", pedal.path.getFullPathName());
    xml->setAttribute ("amp",   amp.path.getFullPathName());
    xml->setAttribute ("cab",   cabPath.getFullPathName());
    copyXmlToBinary (*xml, destData);
}

void NativeNamProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NativeNAM")) return;

    try
    {
        *inputGainParam   = (float) xml->getDoubleAttribute ("inputGain", 0.0);
        *outputGainParam  = (float) xml->getDoubleAttribute ("outputGain", 0.0);
        *bypassPedalParam = xml->getIntAttribute ("bypassPedal", 0) != 0;
        *bypassAmpParam   = xml->getIntAttribute ("bypassAmp", 0) != 0;
        *bypassCabParam   = xml->getIntAttribute ("bypassCab", 0) != 0;
        *liteModeParam    = xml->getIntAttribute ("liteMode", 0) != 0;
        *pedalMixParam    = (float) xml->getDoubleAttribute ("pedalMix", 1.0);
        *ampGainParam     = (float) xml->getDoubleAttribute ("ampGain", 0.0);
        *ampLowParam      = (float) xml->getDoubleAttribute ("bass", 5.0);
        *ampMidParam      = (float) xml->getDoubleAttribute ("middle", 5.0);
        *ampHighParam     = (float) xml->getDoubleAttribute ("treble", 5.0);

        updateAmpEq();
        applySlimMode();

        auto loadPath = [this] (Slot slot, const juce::String& p)
        {
            if (p.isEmpty()) return;
            juce::File f (p);
            if (! f.existsAsFile())
            {
                DevLog::log ("NativeNam setState missing file: " + p);
                return;
            }
            try { loadModel (slot, f); }
            catch (...) { DevLog::log ("NativeNam setState loadModel threw: " + p); }
        };
        loadPath (Slot::Pedal, xml->getStringAttribute ("pedal"));
        loadPath (Slot::Amp,   xml->getStringAttribute ("amp"));
        loadPath (Slot::Cab,   xml->getStringAttribute ("cab"));

        if (onModelsChanged)
            onModelsChanged();
    }
    catch (...)
    {
        DevLog::log ("NativeNam setStateInformation EXCEPTION");
    }
}

juce::AudioProcessorEditor* NativeNamProcessor::createEditor()
{
    // Fullscreen editor: MIDI learn still works via ParameterPanel when block is selected.
    // Provide a no-op manager if opened standalone.
    static MidiLearnManager editorFallbackLearn;
    return new NativeNamPanel (*this, editorFallbackLearn, -1);
}

//==============================================================================
void NativeNamFormat::findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results,
                                           const juce::String&)
{
    results.add (new juce::PluginDescription (NativeNamProcessor::makeDescription()));
}

void NativeNamFormat::createPluginInstance (const juce::PluginDescription& desc,
                                            double initialSampleRate,
                                            int initialBufferSize,
                                            PluginCreationCallback callback)
{
    if (desc.fileOrIdentifier != "internal://native-nam"
        && desc.uniqueId != 0x4E414D32
        && desc.name != NativeNamProcessor::kName)
    {
        callback (nullptr, "Not a Native NAM plugin");
        return;
    }

    auto proc = std::make_unique<NativeNamProcessor>();
    proc->prepareToPlay (initialSampleRate, initialBufferSize);
    // unique_ptr<NativeNamProcessor> → unique_ptr<AudioPluginInstance> (base)
    callback (std::unique_ptr<juce::AudioPluginInstance> (proc.release()), {});
}
