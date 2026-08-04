#pragma once
#include <JuceHeader.h>
#include "MidiLearnManager.h"
#include "NativeNam/NativeNamPanel.h"
#include "OnScreenKeyboard.h"
#include "Theme.h"

//==============================================================================
/** Thin iOS-style switch (actual control, not a ToggleButton chrome). */
class SwitchToggle : public juce::Component
{
public:
    std::function<void(bool)> onChange;

    void setOn (bool v, juce::NotificationType n = juce::sendNotification)
    {
        if (on == v) return;
        on = v;
        repaint();
        if (n != juce::dontSendNotification && onChange)
            onChange (on);
    }
    bool isOn() const { return on; }

    void paint (juce::Graphics& g) override
    {
        auto& th = Theme::get();
        auto r = getLocalBounds().toFloat().reduced (2.0f, 4.0f);
        const float h = juce::jmin (r.getHeight(), 28.0f);
        const float w = juce::jmin (r.getWidth(), 52.0f);
        r = r.withSizeKeepingCentre (w, h);
        g.setColour (on ? th.accent : th.surfaceAlt);
        g.fillRoundedRectangle (r, h * 0.5f);
        const float pad = 3.0f;
        const float d = h - pad * 2.0f;
        const float x = on ? (r.getRight() - pad - d) : (r.getX() + pad);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x, r.getY() + pad, d, d);
    }
    void mouseDown (const juce::MouseEvent&) override { setOn (! on); }

private:
    bool on = false;
};

//==============================================================================
class ParamKnobCell : public juce::Component,
                      public juce::Slider::Listener
{
public:
    ParamKnobCell (int paramIdx, juce::AudioProcessorParameter* param,
                   MidiLearnManager& mgr, int pluginIdx,
                   std::function<void(ParamKnobCell*)> onOpenMidiMenu,
                   std::function<void(ParamKnobCell*)> onOpenValueEdit);
    ~ParamKnobCell() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged (juce::Slider*) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setFromNormalised (float v);
    void refreshMidiIndicator();

    int paramIndex = -1;
    int pluginIndex = -1;
    juce::AudioProcessorParameter* parameter = nullptr;
    bool isToggle = false;
    bool isChoice = false;

    juce::Slider knob;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::TextButton resetBtn;
    SwitchToggle switchToggle;
    juce::ComboBox choiceBox;
    MidiLearnManager& learnManager;

private:
    std::function<void(ParamKnobCell*)> openMidiMenu;
    std::function<void(ParamKnobCell*)> openValueEdit;
    float defaultNorm = 0.0f;
};

//==============================================================================
/** Viewport that only scrolls via the scrollbar (no wheel / drag). */
class ScrollbarOnlyViewport : public juce::Viewport
{
public:
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
};

//==============================================================================
class ParameterPanel : public juce::Component,
                       public juce::AudioProcessorParameter::Listener
{
public:
    ParameterPanel (MidiLearnManager& learnMgr);
    ~ParameterPanel() override;

    void setPlugin (juce::AudioPluginInstance* instance, int pluginIndex,
                    bool bypassed, bool mono, std::function<void()> onBypass,
                    std::function<void()> onMonoToggle,
                    std::function<void()> onColour = nullptr,
                    std::function<void()> onOpenEditor = nullptr);
    void clear();
    void updateParamValue (int paramIndex, float value);
    void updateBypass (bool bypassed);
    void updateMono (bool mono);
    void refreshMidiButtons();
    void openBypassMidiMenuFor (int pluginIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

private:
    void rebuildGrid();
    void updateCellVisibility();
    void openMidiMenuFor (ParamKnobCell* cell);
    void openValueEditFor (ParamKnobCell* cell);
    void closeOverlays();
    void styleHeaderButton (juce::TextButton& b, bool primary = false);

    /** Same PS-style overlay as preset rename / NativeNam search. */
    class ValueEditOverlay : public juce::Component, private juce::Timer
    {
    public:
        ValueEditOverlay()
        {
            auto& th = Theme::get();
            setInterceptsMouseClicks (true, true);

            titleLab.setText ("Value", juce::dontSendNotification);
            titleLab.setFont (juce::FontOptions (18.0f, juce::Font::bold));
            titleLab.setColour (juce::Label::textColourId, th.textDim);
            titleLab.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (titleLab);

            const juce::Font f (juce::FontOptions (28.0f, juce::Font::bold));
            editor.setFont (f);
            editor.applyFontToAllText (f);
            editor.setColour (juce::TextEditor::textColourId, th.text);
            editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::highlightColourId, th.accent.withAlpha (0.35f));
            editor.setColour (juce::CaretComponent::caretColourId, th.accent);
            editor.setMultiLine (false);
            editor.setSelectAllWhenFocused (false);
            editor.setJustification (juce::Justification::centred);
            editor.setInputRestrictions (32, "0123456789.-+eE");
            editor.onReturnKey = [this] { if (onDone) onDone (editor.getText()); };
            addAndMakeVisible (editor);

            th.applyButton (okBtn, true);
            th.applyButton (cancelBtn, false);
            okBtn.setButtonText ("OK");
            cancelBtn.setButtonText ("CANCEL");
            okBtn.onClick = [this] { if (onDone) onDone (editor.getText()); };
            cancelBtn.onClick = [this] { if (onDone) onDone ({}); };
            addAndMakeVisible (okBtn);
            addAndMakeVisible (cancelBtn);

            keyboard.attachEditor (&editor);
            keyboard.onEnter = [this] { if (onDone) onDone (editor.getText()); };
            addAndMakeVisible (keyboard);
            startTimerHz (60);
        }

        void setInitial (const juce::String& t)
        {
            const juce::Font f (juce::FontOptions (28.0f, juce::Font::bold));
            editor.setText (t, false);
            editor.applyFontToAllText (f);
            editor.selectAll();
            fade = 0.0f;
        }

        void timerCallback() override
        {
            fade = juce::jmin (1.0f, fade + 0.12f);
            repaint();
            if (fade >= 1.0f) stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            auto& th = Theme::get();
            g.setColour (th.overlay.withMultipliedAlpha (fade));
            g.fillAll();
            juce::ColourGradient vig (juce::Colours::transparentBlack,
                                      (float) getWidth() * 0.5f, (float) getHeight() * 0.35f,
                                      juce::Colours::black.withAlpha (0.35f * fade),
                                      (float) getWidth() * 0.5f, (float) getHeight(), true);
            g.setGradientFill (vig);
            g.fillAll();
            auto field = editor.getBounds().toFloat().expanded (8.0f, 4.0f);
            g.setColour (th.accent.withAlpha (0.85f * fade));
            g.fillRoundedRectangle (field.getX(), field.getBottom() - 2.0f, field.getWidth(), 2.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (getWidth() / 18, getHeight() / 14);
            titleLab.setBounds (r.removeFromTop (28));
            r.removeFromTop (12);
            editor.setBounds (r.removeFromTop (52).reduced (r.getWidth() / 10, 0));
            r.removeFromTop (16);
            auto actions = r.removeFromTop (44);
            const int bw = 130;
            okBtn.setBounds (actions.getCentreX() - bw - 8, actions.getY(), bw, 40);
            cancelBtn.setBounds (actions.getCentreX() + 8, actions.getY(), bw, 40);
            r.removeFromTop (12);
            keyboard.setBounds (r);
        }

        std::function<void(juce::String)> onDone;
        juce::Label titleLab;
        juce::TextEditor editor;
        OnScreenKeyboard keyboard;
        juce::TextButton okBtn, cancelBtn;
        float fade = 0.0f;
    };

    class MidiMenuPopup : public juce::Component
    {
    public:
        MidiMenuPopup()
        {
            auto& th = Theme::get();
            // Match Settings: soft LookAndFeel, no harsh outlines
            setLookAndFeel (&th.softLaf);

            title.setJustificationType (juce::Justification::centred);
            title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, th.text);
            title.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (title);

            auto prep = [&] (juce::TextButton& b, bool primary = false)
            {
                b.setLookAndFeel (&th.softLaf);
                th.applyButton (b, primary);
                b.setColour (juce::TextButton::textColourOffId, th.text);
                b.setColour (juce::TextButton::textColourOnId, th.text);
                // Kill default square borders
                b.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            };
            prep (learnBtn, true);
            prep (clearBtn, false);
            prep (modeInstant, false);
            prep (modeToggle, false);
            prep (closeBtn, false);

            modeInstant.setClickingTogglesState (true);
            modeToggle.setClickingTogglesState (true);
            modeInstant.setRadioGroupId (7701);
            modeToggle.setRadioGroupId (7701);
            modeInstant.setColour (juce::TextButton::buttonOnColourId, th.accent);
            modeToggle.setColour (juce::TextButton::buttonOnColourId, th.accent);

            learnBtn.setButtonText ("LEARN");
            clearBtn.setButtonText ("Clear MIDI");
            modeInstant.setButtonText ("Instant / Hold");
            modeToggle.setButtonText ("Toggle");
            closeBtn.setButtonText ("Close");

            for (auto* b : { &learnBtn, &clearBtn, &modeInstant, &modeToggle, &closeBtn })
                addAndMakeVisible (b);
        }
        ~MidiMenuPopup() override
        {
            setLookAndFeel (nullptr);
            for (auto* b : { &learnBtn, &clearBtn, &modeInstant, &modeToggle, &closeBtn })
                b->setLookAndFeel (nullptr);
        }
        void paint (juce::Graphics& g) override
        {
            auto& th = Theme::get();
            g.setColour (th.overlay);
            g.fillAll();
            auto card = cardBounds().toFloat();
            // Soft filled card, no hard white border — same as settings sheets
            g.setColour (th.surface);
            g.fillRoundedRectangle (card, 20.0f);
            g.setColour (th.surfaceAlt.withAlpha (0.9f));
            g.drawRoundedRectangle (card.reduced (0.5f), 20.0f, 1.0f);
        }
        juce::Rectangle<int> cardBounds() const
        {
            return getLocalBounds().withSizeKeepingCentre (
                juce::jmin (340, getWidth() - 32),
                juce::jmin (360, getHeight() - 32));
        }
        void resized() override
        {
            auto c = cardBounds().reduced (18);
            title.setBounds (c.removeFromTop (40));
            c.removeFromTop (6);
            auto slot = [&] (juce::TextButton& b)
            {
                b.setBounds (c.removeFromTop (46).reduced (0, 4));
            };
            slot (learnBtn);
            slot (clearBtn);
            c.removeFromTop (10);
            slot (modeInstant);
            slot (modeToggle);
            closeBtn.setBounds (c.removeFromBottom (46).reduced (0, 4));
        }
        juce::Label title;
        juce::TextButton learnBtn, clearBtn, closeBtn;
        juce::TextButton modeInstant, modeToggle;
    };

    /** Window-frame icon for "open plugin UI". */
    class WindowIconButton : public juce::Button
    {
    public:
        WindowIconButton() : juce::Button ("ui") {}
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto& th = Theme::get();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            g.setColour ((over || down) ? th.accent.withAlpha (0.25f) : th.surfaceAlt);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (th.text.withAlpha (0.9f));
            auto win = r.reduced (5.0f);
            g.drawRoundedRectangle (win, 3.0f, 1.6f);
            // title bar
            g.fillRoundedRectangle (win.getX(), win.getY(), win.getWidth(), juce::jmax (4.0f, win.getHeight() * 0.22f), 2.0f);
        }
    };

    MidiLearnManager& midiLearn;
    juce::AudioPluginInstance* currentPlugin = nullptr;
    int currentPluginIndex = -1;

    ScrollbarOnlyViewport viewport;
    juce::Component content;
    juce::OwnedArray<ParamKnobCell> cells;
    juce::Label titleLabel;
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton monoButton { "STEREO" };
    juce::TextButton bypassLearnButton { "LEARN" };
    juce::TextButton bypassClearButton { "X" };
    juce::TextButton colourButton { "COLOR" };
    WindowIconButton openEditorButton;
    std::function<void()> bypassCallback;
    std::function<void()> monoToggleCallback;
    std::function<void()> colourCallback;
    std::function<void()> openEditorCallback;
    std::unique_ptr<NativeNamPanel> nativeNamPanel;
    std::unique_ptr<ValueEditOverlay> valueOverlay;
    std::unique_ptr<MidiMenuPopup> midiPopup;
    ParamKnobCell* menuTarget = nullptr;

    static constexpr int kCellW = 152;
    static constexpr int kCellH = 176;
    static constexpr int kHeaderH = 52;
    static constexpr int kScrollBar = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterPanel)
};
