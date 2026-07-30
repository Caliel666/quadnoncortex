#include "MidiLearnManager.h"
#include "PluginChain.h"

void MidiLearnManager::applyBinding (const Binding& b, float normalisedValue, const juce::MidiMessage& msg)
{
    // -------------------------------------------------------------------------
    // GLOBAL ACTIONS
    // -------------------------------------------------------------------------
    if (b.globalAction.isNotEmpty())
    {
        const bool isCC = b.ccNumber >= 0;
        const int  num  = isCC ? b.ccNumber : b.noteNumber;
        const int  key  = makeKey (b.midiChannel, num, isCC);
        const auto now  = juce::Time::getMillisecondCounter();

        // ---- TUNER: absolute like a parameter (ON = tuner, OFF = board) ----
        if (b.globalAction == "tuner")
        {
            bool on = false;
            if (msg.isController()
                || msg.isNoteOff()
                || (msg.isNoteOn() && msg.getVelocity() == 0))
                on = normalisedValue >= 0.5f;
            else if (msg.isNoteOn())
                on = true;
            else
                return;

            // Debounce only identical rapid packets
            auto it = lastToggleState.find (key);
            const int want = on ? 1 : 0;
            if (it != lastToggleState.end()
                && it->second.value == want
                && (now - it->second.ms) < 80)
                return;

            lastToggleState[key] = { want, now };

            juce::MessageManager::callAsync ([this, on]
            {
                if (onGlobalAction)
                    onGlobalAction ("tuner", on ? 1.0f : 0.0f);
            });
            return;
        }

        // ---- PRESET NEXT / PREV: fire on EVERY press (ON and OFF edges) ----
        // so a latching footswitch advances on both stomp directions
        {
            bool edge = false;
            if (msg.isController())
                edge = true; // any CC change (0 or 127)
            else if (msg.isNoteOn() && msg.getVelocity() > 0)
                edge = true;
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                edge = true;

            if (! edge)
                return;

            auto it = lastToggleState.find (key);
            if (it != lastToggleState.end() && (now - it->second.ms) < 120)
                return;

            lastToggleState[key] = { 1, now };

            juce::MessageManager::callAsync ([this, action = b.globalAction]
            {
                if (onGlobalAction)
                    onGlobalAction (action, 1.0f);
            });
            return;
        }
    }

    if (pluginChain == nullptr) return;

    if (! juce::isPositiveAndBelow (b.pluginIndex, pluginChain->getNumPlugins()))
        return;

    // -------------------------------------------------------------------------
    // BYPASS — absolute
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // PARAMETERS — every message applies the value
    // -------------------------------------------------------------------------
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
