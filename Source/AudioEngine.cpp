#include "AudioEngine.h"
#include "AppSettings.h"

AudioEngine::AudioEngine()
{
    midiLearn.setChain (&chain);
    chain.onPluginRemoved = [this] (int index) { midiLearn.clearPlugin (index); };
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    auto& settings = AppSettings::get();

    // Allow mono (1) through stereo (2) inputs and outputs
    juce::String error;
    if (settings.audioDeviceStateXml != nullptr)
        error = deviceManager.initialise (0, 2, settings.audioDeviceStateXml.get(), true);
    else
        error = deviceManager.initialise (0, 2, nullptr, true);

    if (error.isNotEmpty())
        deviceManager.initialise (0, 2, nullptr, true);

    inputGain.store  (settings.inputGain);
    outputGain.store (settings.outputGain);

    deviceManager.addAudioCallback (this);

    // MIDI inputs only – never open outputs
    for (auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (! deviceManager.isMidiInputDeviceEnabled (device.identifier))
            deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        deviceManager.addMidiInputDeviceCallback (device.identifier, this);
    }
}

void AudioEngine::saveDeviceState()
{
    AppSettings::get().inputGain  = inputGain.load();
    AppSettings::get().outputGain = outputGain.load();
    AppSettings::get().storeAudioDeviceState (deviceManager);
}

void AudioEngine::shutdown()
{
    saveDeviceState();
    deviceManager.removeAudioCallback (this);
    for (auto& device : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (device.identifier, this);
    chain.releaseResources();
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    blockSize  = device->getCurrentBufferSizeSamples();
    processBuffer.setSize (2, blockSize, false, false, true);
    chain.prepare (sampleRate, blockSize);
}

void AudioEngine::audioDeviceStopped()
{
    chain.releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    processBuffer.setSize (2, numSamples, false, false, true);
    processBuffer.clear();

    const float inG = inputGain.load();

    if (numInputChannels >= 1 && inputChannelData[0] != nullptr)
    {
        processBuffer.copyFrom (0, 0, inputChannelData[0], numSamples, inG);
        if (numInputChannels >= 2 && inputChannelData[1] != nullptr)
            processBuffer.copyFrom (1, 0, inputChannelData[1], numSamples, inG);
        else
            processBuffer.copyFrom (1, 0, inputChannelData[0], numSamples, inG);
    }

    // Peak meters (atomic)
    float inPeak = 0.0f, outPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        inPeak = juce::jmax (inPeak, std::abs (processBuffer.getSample (0, i)));
        if (processBuffer.getNumChannels() > 1)
            inPeak = juce::jmax (inPeak, std::abs (processBuffer.getSample (1, i)));
    }

    if (onAudioForTuner && processBuffer.getNumChannels() > 0)
        onAudioForTuner (processBuffer.getReadPointer (0), numSamples);

    juce::MidiBuffer midi;
    {
        const juce::ScopedLock sl (midiLock);
        midi.swapWith (incomingMidi);
    }

    chain.process (processBuffer, midi);

    const float outG = muted.load() ? 0.0f : outputGain.load();

    for (int ch = 0; ch < juce::jmin (numOutputChannels, processBuffer.getNumChannels()); ++ch)
    {
        if (outputChannelData[ch] == nullptr) continue;
        juce::FloatVectorOperations::copyWithMultiply (outputChannelData[ch],
                                                       processBuffer.getReadPointer (ch),
                                                       outG, numSamples);
        for (int i = 0; i < numSamples; ++i)
            outPeak = juce::jmax (outPeak, std::abs (outputChannelData[ch][i]));
    }

    for (int ch = processBuffer.getNumChannels(); ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    inputPeak.store (inPeak);
    outputPeak.store (outPeak);
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* source,
                                             const juce::MidiMessage& message)
{
    midiLearn.handleIncomingMidiMessage (source, message);
    if (onMidiForGlobals)
        onMidiForGlobals (message);

    if (message.isNoteOnOrOff() || message.isController() || message.isPitchWheel())
    {
        const juce::ScopedLock sl (midiLock);
        incomingMidi.addEvent (message, 0);
    }
}
