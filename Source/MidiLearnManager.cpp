#include "MidiLearnManager.h"
#include "PluginChain.h"

void MidiLearnManager::applyBinding (const Binding& b, float normalisedValue, const juce::MidiMessage& msg)
{
    if (b.globalAction.isNotEmpty())
    {
        // Globals (tuner / preset): one physical toggle = one action.
        // Must process CC-low to clear edge state, otherwise the next high is ignored.
        if (msg.isController() && normalisedValue < 0.5f)
        {
            const int key = makeKey (b.midiChannel, b.ccNumber, true);
            lastToggleState[key] = { 0, juce::Time::getMillisecondCounter() };
            return;
        }
        if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            const int key = makeKey (b.midiChannel, b.noteNumber, false);
            lastToggleState[key] = { 0, juce::Time::getMillisecondCounter() };
            return;
        }

        if (msg.isNoteOn() || (msg.isController() && normalisedValue >= 0.5f))
        {
            if (! acceptToggleEdge (b, true))
                return;

            juce::MessageManager::callAsync ([this, action = b.globalAction, normalisedValue]
            {
                if (onGlobalAction) onGlobalAction (action, normalisedValue);
            });
        }
        return;
    }

    if (pluginChain == nullptr) return;

    if (! juce::isPositiveAndBelow (b.pluginIndex, pluginChain->getNumPlugins()))
        return;

    // Bypass — absolute mapping (unchanged; works with toggle switches)
    if (b.paramIndex == -2)
    {
        const int idx = b.pluginIndex;
        bool bypassOn = false;

        if (msg.isController() || msg.isNoteOff()
            || (msg.isNoteOn() && msg.getVelocity() == 0))
            bypassOn = normalisedValue >= 0.5f;
        else if (msg.isNoteOn())
            bypassOn = true;
        else
            return;

        juce::MessageManager::callAsync ([this, idx, bypassOn]
        {
            if (pluginChain == nullptr) return;
            if (! juce::isPositiveAndBelow (idx, pluginChain->getNumPlugins()))
                return;
            pluginChain->setBypass (idx, bypassOn);
            if (onParamChangedByMidi)
                onParamChangedByMidi (idx, -2, bypassOn ? 1.0f : 0.0f);
        });
        return;
    }

    if (auto* instance = pluginChain->getPluginInstance (b.pluginIndex))
    {
        const auto& params = instance->getParameters();
        if (! juce::isPositiveAndBelow (b.paramIndex, params.size()))
            return;

        if (auto* param = params[b.paramIndex])
        {
            float v = normalisedValue;
            if (param->isBoolean() || param->getNumSteps() == 2)
                v = normalisedValue >= 0.5f ? 1.0f : 0.0f;

            try
            {
                param->setValueNotifyingHost (v);
            }
            catch (...)
            {
                return;
            }

            const int pIdx  = b.pluginIndex;
            const int prIdx = b.paramIndex;
            juce::MessageManager::callAsync ([this, pIdx, prIdx, v]
            {
                if (onParamChangedByMidi)
                    onParamChangedByMidi (pIdx, prIdx, v);
            });
        }
    }
}
