#include "MidiLearnManager.h"
#include "PluginChain.h"
#include "AppSettings.h"

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

        // ---- TUNER ----
        if (b.globalAction == "tuner")
        {
            float v = normalisedValue;
            if (b.mode == MidiMode::Toggle)
            {
                bool fire = false;
                if (msg.isController())
                {
                    if (normalisedValue >= 0.5f) fire = acceptToggleEdge (b, true);
                    else acceptToggleEdge (b, false);
                }
                else if (msg.isNoteOn() && msg.getVelocity() > 0)
                    fire = acceptToggleEdge (b, true);
                else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                {
                    acceptToggleEdge (b, false);
                    return;
                }
                else return;
                if (! fire) return;
                // Flip: we don't know current tab here — host handles as toggle pulse
                v = 1.0f; // pulse; MainComponent toggles tab on any >=0.5 in Toggle mode
            }
            else
            {
                if (msg.isController() || msg.isNoteOff()
                    || (msg.isNoteOn() && msg.getVelocity() == 0))
                    v = normalisedValue >= 0.5f ? 1.0f : 0.0f;
                else if (msg.isNoteOn())
                    v = 1.0f;
                else return;

                auto it = lastToggleState.find (key);
                const int want = v >= 0.5f ? 1 : 0;
                if (it != lastToggleState.end() && it->second.value == want && (now - it->second.ms) < 80)
                    return;
                lastToggleState[key] = { want, now };
            }

            juce::MessageManager::callAsync ([this, v, mode = b.mode]
            {
                if (onGlobalAction)
                    onGlobalAction (mode == MidiMode::Toggle ? "tunerToggle" : "tuner", v);
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

        if (b.mode == MidiMode::Toggle)
        {
            bool fire = false;
            if (msg.isController())
            {
                if (normalisedValue >= 0.5f) fire = acceptToggleEdge (b, true);
                else acceptToggleEdge (b, false);
            }
            else if (msg.isNoteOn() && msg.getVelocity() > 0)
                fire = acceptToggleEdge (b, true);
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                acceptToggleEdge (b, false);
                return;
            }
            else return;
            if (! fire) return;

            juce::MessageManager::callAsync ([this, idx]
            {
                if (pluginChain == nullptr) return;
                if (! juce::isPositiveAndBelow (idx, pluginChain->getNumPlugins())) return;
                const bool next = ! pluginChain->isBypassed (idx);
                pluginChain->setBypass (idx, next);
                if (onParamChangedByMidi)
                    onParamChangedByMidi (idx, -2, next ? 1.0f : 0.0f);
            });
            return;
        }

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
    // PARAMETERS — Instant follows value; Toggle flips once per press
    // -------------------------------------------------------------------------
    if (auto* instance = pluginChain->getPluginInstance (b.pluginIndex))
    {
        const auto& params = instance->getParameters();
        if (! juce::isPositiveAndBelow (b.paramIndex, params.size()))
            return;

        if (auto* param = params[b.paramIndex])
        {
            float v = normalisedValue;

            if (b.mode == MidiMode::Toggle)
            {
                // Rising edge only — works with hold/momentary hardware.
                // Use Toggle mode in the UI when you want one flip per press.
                bool fire = false;
                if (msg.isController())
                {
                    if (normalisedValue >= 0.5f)
                        fire = acceptToggleEdge (b, true);
                    else
                        acceptToggleEdge (b, false);
                }
                else if (msg.isNoteOn() && msg.getVelocity() > 0)
                    fire = acceptToggleEdge (b, true);
                else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                {
                    acceptToggleEdge (b, false);
                    return;
                }
                else
                    return;

                if (! fire)
                    return;

                const float cur = param->getValue();
                v = cur >= 0.5f ? 0.0f : 1.0f;
            }
            else
            {
                // Instant / Hold: follow the control value
                if (param->isBoolean() || param->getNumSteps() == 2)
                    v = normalisedValue >= 0.5f ? 1.0f : 0.0f;
            }

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


void MidiLearnManager::saveGlobalsToSettings() const
{
    auto xml = std::make_unique<juce::XmlElement> ("GlobalMidiBindings");
    for (const auto& pair : bindings)
    {
        if (pair.second.globalAction.isEmpty()) continue;
        auto* e = xml->createNewChildElement ("Binding");
        e->setAttribute ("global",  pair.second.globalAction);
        e->setAttribute ("channel", pair.second.midiChannel);
        e->setAttribute ("cc",      pair.second.ccNumber);
        e->setAttribute ("note",    pair.second.noteNumber);
        e->setAttribute ("mode",    pair.second.mode == MidiMode::Toggle ? "toggle" : "instant");
    }
    AppSettings::get().globalMidiXml = std::move (xml);
    AppSettings::get().save();
}

void MidiLearnManager::loadGlobalsFromSettings()
{
    auto* src = AppSettings::get().globalMidiXml.get();
    if (src == nullptr) return;
    for (auto* e : src->getChildIterator())
    {
        if (! e->hasTagName ("Binding")) continue;
        Binding b;
        b.globalAction = e->getStringAttribute ("global");
        b.midiChannel  = e->getIntAttribute ("channel");
        b.ccNumber     = e->getIntAttribute ("cc", -1);
        b.noteNumber   = e->getIntAttribute ("note", -1);
        b.mode = e->getStringAttribute ("mode") == "toggle" ? MidiMode::Toggle
                                                            : MidiMode::Instant;
        b.pluginIndex  = -1;
        b.paramIndex   = -1;
        if (b.globalAction.isEmpty()) continue;
        const bool isCC = b.ccNumber >= 0;
        const int  num  = isCC ? b.ccNumber : b.noteNumber;
        bindings[makeKey (b.midiChannel, num, isCC)] = b;
    }
}
