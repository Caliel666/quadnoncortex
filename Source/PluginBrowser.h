#pragma once

#include <JuceHeader.h>
#include "PluginChain.h"
#include "OnScreenKeyboard.h"
#include "Theme.h"

//==============================================================================
/** Full-screen plugin list — minimal chrome, search + rename-style keyboard overlay. */
class PluginBrowser : public juce::Component
{
public:
    explicit PluginBrowser (PluginChain& chain);
    ~PluginBrowser() override = default;

    void show (int replaceIndex = -1);
    void rebuildList();

    std::function<void()> onClosed;
    std::function<void(int)> onPluginChosen;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    PluginChain& pluginChain;
    int replaceIndex = -1;

    juce::TextButton closeButton { "CLOSE" };
    juce::Label      titleLabel;
    juce::TextEditor searchBox;
    juce::ComboBox filterBox; // All / Native / Third-party

    /** Full-screen glass overlay hosting the OSK (same feel as rename preset). */
    class KeyboardOverlay : public juce::Component, private juce::Timer
    {
    public:
        OnScreenKeyboard keyboard;
        juce::TextEditor field;
        juce::TextButton doneBtn { "DONE" };
        juce::Label hint;
        float fade = 0.0f;
        std::function<void(juce::String)> onDone;
        std::function<void()> onDismiss;

        KeyboardOverlay()
        {
            auto& th = Theme::get();
            field.setFont (juce::FontOptions (22.0f, juce::Font::bold));
            field.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            field.setColour (juce::TextEditor::textColourId, th.text);
            field.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            field.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            field.setJustification (juce::Justification::centred);
            addAndMakeVisible (field);

            hint.setText ("Search plugins", juce::dontSendNotification);
            hint.setJustificationType (juce::Justification::centred);
            hint.setColour (juce::Label::textColourId, th.textDim);
            hint.setFont (juce::FontOptions (14.0f));
            addAndMakeVisible (hint);

            th.applyButton (doneBtn, true);
            doneBtn.onClick = [this]
            {
                if (onDone) onDone (field.getText());
            };
            addAndMakeVisible (doneBtn);

            keyboard.onKey = [this] (juce::String s)
            {
                if (s == "\b")
                {
                    auto t = field.getText();
                    if (t.isNotEmpty())
                        field.setText (t.dropLastCharacters (1), juce::dontSendNotification);
                }
                else if (s == "\n")
                {
                    if (onDone) onDone (field.getText());
                }
                else
                {
                    field.setText (field.getText() + s, juce::dontSendNotification);
                }
            };
            keyboard.onEnter = [this]
            {
                if (onDone) onDone (field.getText());
            };
            addAndMakeVisible (keyboard);
            startTimerHz (60);
        }

        void setInitial (const juce::String& text)
        {
            field.setText (text, juce::dontSendNotification);
            fade = 0.0f;
        }

        void timerCallback() override
        {
            fade = juce::jmin (1.0f, fade + 0.14f);
            repaint();
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

            auto fieldR = field.getBounds().toFloat().expanded (8.0f, 4.0f);
            g.setColour (th.accent.withAlpha (0.85f * fade));
            g.fillRoundedRectangle (fieldR.getX(), fieldR.getBottom() - 2.0f,
                                    fieldR.getWidth(), 2.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (getWidth() / 18, getHeight() / 16);
            hint.setBounds (r.removeFromTop (24));
            r.removeFromTop (8);
            field.setBounds (r.removeFromTop (52).reduced (r.getWidth() / 12, 0));
            r.removeFromTop (12);
            doneBtn.setBounds (r.removeFromTop (44).withSizeKeepingCentre (140, 40));
            r.removeFromTop (12);
            keyboard.setBounds (r);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            // Tap outside keyboard/field dismisses without applying? keep text
            if (! keyboard.getBounds().contains (e.getPosition())
                && ! field.getBounds().contains (e.getPosition())
                && ! doneBtn.getBounds().contains (e.getPosition()))
            {
                if (onDone) onDone (field.getText());
            }
        }
    };

    std::unique_ptr<KeyboardOverlay> kbOverlay;

    juce::Viewport viewport;
    juce::Component content;

    struct PluginRow : public juce::Component
    {
        PluginRow (const juce::PluginDescription& d, PluginBrowser& owner);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        juce::PluginDescription desc;
        PluginBrowser& browser;
    };

    juce::OwnedArray<PluginRow> rows;
    juce::String filterText;

    void choosePlugin (const juce::PluginDescription& desc);
    void applyFilter();
    void openSearchKeyboard();
    void closeSearchKeyboard (const juce::String& text);

    static constexpr int kRowHeight = 56;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBrowser)
};
