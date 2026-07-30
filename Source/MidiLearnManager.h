#pragma once

#include <JuceHeader.h>
#include <map>

class PluginChain;

//==============================================================================
class MidiLearnManager : public juce::MidiInputCallback
{
public:
    struct Binding
    {
        int pluginIndex = -1;  // -1 = global action
        int paramIndex  = -1;  // -1 = bypass or global
        juce::String globalAction; // e.g. "tuner", "presetNext", "presetPrev", "bypass:N"
        int midiChannel = 0;
        int ccNumber    = -1;
        int noteNumber  = -1;
    };

    MidiLearnManager() = default;

    void setChain (PluginChain* chain) { pluginChain = chain; }

    void startLearn (int pluginIndex, int paramIndex)
    {
        cancelLearn();
        learning = true;
        learnPlugin = pluginIndex;
        learnParam  = paramIndex;
        learnGlobalAction.clear();
        if (onBindingsChanged) onBindingsChanged();
    }

    void startLearnGlobal (const juce::String& action)
    {
        cancelLearn();
        learning = true;
        learnPlugin = -1;
        learnParam  = -1;
        learnGlobalAction = action;
        if (onBindingsChanged) onBindingsChanged();
    }

    void cancelLearn()
    {
        learning = false;
        learnPlugin = -1;
        learnParam  = -1;
        learnGlobalAction.clear();
    }

    bool isLearning() const { return learning; }
    int getLearnPlugin() const { return learnPlugin; }
    int getLearnParam()  const { return learnParam; }
    juce::String getLearnGlobalAction() const { return learnGlobalAction; }

    void clearBinding (int pluginIndex, int paramIndex)
    {
        for (auto it = bindings.begin(); it != bindings.end(); )
        {
            if (it->second.pluginIndex == pluginIndex && it->second.paramIndex == paramIndex
                && it->second.globalAction.isEmpty())
            {
                lastToggleState.erase (it->first);
                it = bindings.erase (it);
            }
            else
                ++it;
        }
        if (onBindingsChanged) onBindingsChanged();
    }

    void clearGlobal (const juce::String& action)
    {
        for (auto it = bindings.begin(); it != bindings.end(); )
        {
            if (it->second.globalAction == action)
                it = bindings.erase (it);
            else
                ++it;
        }
        if (onBindingsChanged) onBindingsChanged();
    }

    void clearPlugin (int pluginIndex)
    {
        cancelLearn();
        for (auto it = bindings.begin(); it != bindings.end(); )
        {
            if (it->second.pluginIndex == pluginIndex)
                it = bindings.erase (it);
            else
                ++it;
        }
        for (auto& pair : bindings)
            if (pair.second.pluginIndex > pluginIndex)
                --pair.second.pluginIndex;
        if (onBindingsChanged) onBindingsChanged();
    }

    /** Call when plugins are reordered so MIDI maps stay on the same plugin. */
    void remapAfterMove (int from, int to)
    {
        if (from == to) return;
        cancelLearn();

        for (auto& pair : bindings)
        {
            int& idx = pair.second.pluginIndex;
            if (idx < 0) continue;

            if (from < to)
            {
                // [from] moved right to [to]
                if (idx == from)              idx = to;
                else if (idx > from && idx <= to) --idx;
            }
            else
            {
                // [from] moved left to [to]
                if (idx == from)              idx = to;
                else if (idx >= to && idx < from) ++idx;
            }
        }
        if (onBindingsChanged) onBindingsChanged();
    }

    /** Call when a plugin is inserted at index (existing at and after shift right). */
    void remapAfterInsert (int atIndex)
    {
        cancelLearn();
        for (auto& pair : bindings)
            if (pair.second.pluginIndex >= atIndex)
                ++pair.second.pluginIndex;
        if (onBindingsChanged) onBindingsChanged();
    }

    Binding* findBinding (int pluginIndex, int paramIndex)
    {
        for (auto& pair : bindings)
            if (pair.second.pluginIndex == pluginIndex && pair.second.paramIndex == paramIndex
                && pair.second.globalAction.isEmpty())
                return &pair.second;
        return nullptr;
    }

    Binding* findGlobal (const juce::String& action)
    {
        for (auto& pair : bindings)
            if (pair.second.globalAction == action)
                return &pair.second;
        return nullptr;
    }

    const std::map<int, Binding>& getAllBindings() const { return bindings; }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (message.isController())
        {
            const int key = makeKey (message.getChannel(), message.getControllerNumber(), true);
            const float v = message.getControllerValue() / 127.0f;
            if (learning)
            {
                commitLearn (key, message.getChannel(), message.getControllerNumber(), -1, v);
                return;
            }
            auto it = bindings.find (key);
            if (it != bindings.end())
                applyBinding (it->second, v, message);
        }
        else if (message.isNoteOn() && message.getVelocity() > 0)
        {
            const int key = makeKey (message.getChannel(), message.getNoteNumber(), false);
            const float v = message.getFloatVelocity();
            if (learning)
            {
                commitLearn (key, message.getChannel(), -1, message.getNoteNumber(), v);
                return;
            }
            auto it = bindings.find (key);
            if (it != bindings.end())
                applyBinding (it->second, v, message);
        }
        else if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0))
        {
            // Absolute note mappings: note-off → 0 (for bypass / toggles)
            const int key = makeKey (message.getChannel(), message.getNoteNumber(), false);
            auto it = bindings.find (key);
            if (it != bindings.end() && it->second.paramIndex == -2)
                applyBinding (it->second, 0.0f, message);
        }
    }

    void saveToXml (juce::XmlElement& parent) const
    {
        auto* root = parent.createNewChildElement ("MidiBindings");
        for (const auto& pair : bindings)
        {
            auto* e = root->createNewChildElement ("Binding");
            e->setAttribute ("plugin",  pair.second.pluginIndex);
            e->setAttribute ("param",   pair.second.paramIndex);
            e->setAttribute ("global",  pair.second.globalAction);
            e->setAttribute ("channel", pair.second.midiChannel);
            e->setAttribute ("cc",      pair.second.ccNumber);
            e->setAttribute ("note",    pair.second.noteNumber);
        }
    }

    void loadFromXml (const juce::XmlElement& parent)
    {
        bindings.clear();
        if (auto* root = parent.getChildByName ("MidiBindings"))
        {
            for (auto* e : root->getChildIterator())
            {
                if (! e->hasTagName ("Binding")) continue;
                Binding b;
                b.pluginIndex  = e->getIntAttribute ("plugin", -1);
                b.paramIndex   = e->getIntAttribute ("param", -1);
                b.globalAction = e->getStringAttribute ("global");
                b.midiChannel  = e->getIntAttribute ("channel");
                b.ccNumber     = e->getIntAttribute ("cc", -1);
                b.noteNumber   = e->getIntAttribute ("note", -1);
                const bool isCC = b.ccNumber >= 0;
                const int  num  = isCC ? b.ccNumber : b.noteNumber;
                bindings[makeKey (b.midiChannel, num, isCC)] = b;
            }
        }
    }

    /** Fired on message thread after a param value is changed by MIDI. */
    std::function<void(int pluginIndex, int paramIndex, float value)> onParamChangedByMidi;
    std::function<void(const juce::String& action, float value)> onGlobalAction;
    std::function<void()> onBindingsChanged;

private:
    /** Rising-edge + debounce so a single footswitch press toggles once (not cancelled by doubles). */
    bool acceptToggleEdge (const Binding& b, bool activeHigh)
    {
        const bool isCC = b.ccNumber >= 0;
        const int num = isCC ? b.ccNumber : b.noteNumber;
        const int key = makeKey (b.midiChannel, num, isCC);
        const auto now = juce::Time::getMillisecondCounter();

        auto it = lastToggleState.find (key);
        const int prev = (it != lastToggleState.end()) ? it->second.value : 0;
        const juce::uint32 prevMs = (it != lastToggleState.end()) ? it->second.ms : 0;

        EdgeState st;
        st.value = activeHigh ? 1 : 0;
        st.ms = now;
        lastToggleState[key] = st;

        if (! activeHigh)
            return false; // falling edge — ignore

        // Debounce: ignore second rising edge within 100ms (duplicate MIDI)
        if (prev == 1 && (now - prevMs) < 100)
            return false;

        // Rising edge: was low (or never seen), now high
        if (prev == 0 || (now - prevMs) >= 100)
            return true;

        return false;
    }

    struct EdgeState { int value = 0; juce::uint32 ms = 0; };
    std::map<int, EdgeState> lastToggleState;

    static int makeKey (int channel, int number, bool isCC)
    {
        return (isCC ? 0x10000 : 0) | ((channel & 0x1F) << 8) | (number & 0x7F);
    }

    void commitLearn (int key, int channel, int cc, int note, float valueNow)
    {
        Binding b;
        b.pluginIndex  = learnPlugin;
        b.paramIndex   = learnParam;
        b.globalAction = learnGlobalAction;
        b.midiChannel  = channel;
        b.ccNumber     = cc;
        b.noteNumber   = note;

        if (b.globalAction.isNotEmpty())
            clearGlobal (b.globalAction);
        else
            clearBinding (learnPlugin, learnParam);

        bindings[key] = b;

        // Apply immediately so the first learn press also sets the value
        // (same feel as continuous parameters after mapping)
        juce::MidiMessage dummy;
        if (cc >= 0)
            dummy = juce::MidiMessage::controllerEvent (channel, cc, (int) (valueNow * 127.0f));
        else
            dummy = juce::MidiMessage::noteOn (channel, note, valueNow);

        cancelLearn();

        // Parameter/bypass: apply learn value immediately.
        // Globals: do NOT fire on learn — next physical press triggers once.
        if (b.globalAction.isEmpty())
            applyBinding (b, valueNow, dummy);

        lastToggleState.erase (key);

        if (onBindingsChanged) onBindingsChanged();
    }

    void applyBinding (const Binding& b, float normalisedValue, const juce::MidiMessage&);

    std::map<int, Binding> bindings;
    PluginChain* pluginChain = nullptr;

    bool learning = false;
    int  learnPlugin = -1;
    int  learnParam  = -1;
    juce::String learnGlobalAction;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};
