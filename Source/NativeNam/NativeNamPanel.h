#pragma once
#include <JuceHeader.h>
#include "NativeNamProcessor.h"
#include "NamLibrary.h"
#include "Tone3000Client.h"
#include "MidiLearnManager.h"
#include "OnScreenKeyboard.h"
#include "Theme.h"

/** Amp / pedal / cab panel for Native NAM — CONFIG + TONE3000.
    CONFIG layout matches ParameterPanel (flat rows + MIDI LEARN / X). */
class NativeNamPanel : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    NativeNamPanel (NativeNamProcessor& proc, MidiLearnManager& learnMgr, int pluginIndex);
    ~NativeNamPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setTab (int t);
    void rebuildLibraryList (NamLibrary::Kind kind);
    void selectLibraryEntry (const NamLibrary::Entry& e);
    void openLibraryBrowser (NamLibrary::Kind kind);
    void closeLibraryBrowser();
    void refreshModelLabels();
    void doSearch();
    void downloadTone (int toneId, const juce::String& gears);
    void refreshMidiButtons();
    void applyTheme();

    /** Compact control row: label | slider/toggle | LEARN | X — same idea as ParameterPanel::ParamRow */
    struct ControlRow : public juce::Component
    {
        ControlRow (const juce::String& name, juce::AudioProcessorParameter* param,
                    MidiLearnManager& mgr, int pluginIdx, int paramIdx);
        void resized() override;
        void updateLearnButton();
        void syncFromParam();

        juce::Label nameLabel;
        juce::Slider slider;
        juce::ToggleButton toggle;
        juce::TextButton learnBtn { "LEARN" }, clearBtn { "X" };
        juce::AudioProcessorParameter* parameter = nullptr;
        MidiLearnManager& learnManager;
        int pluginIndex = -1;
        int paramIndex = -1;
        bool isToggle = false;
    };

    NativeNamProcessor& processor;
    MidiLearnManager& midiLearn;
    int pluginIndex = -1;

    juce::TextButton tabConfig { "CONFIG" };
    juce::TextButton tabT3k    { "TONE3000" };
    int currentTab = 0;

    // ---- CONFIG ----
    juce::Component configPage;

    juce::Label pedalTitle { {}, "PEDAL" };
    juce::Label ampTitle   { {}, "AMP" };
    juce::Label cabTitle   { {}, "CAB" };
    juce::TextButton pedalBtn { "Load Pedal" };
    juce::TextButton ampBtn   { "Load Amp" };
    juce::TextButton cabBtn   { "Load Cab" };
    juce::TextButton pedalClear { "X" };
    juce::TextButton ampClear   { "X" };
    juce::TextButton cabClear   { "X" };
    juce::Label pedalName, ampName, cabName;

    std::unique_ptr<ControlRow> pedalMixRow;
    std::unique_ptr<ControlRow> ampGainRow, ampLowRow, ampMidRow, ampHighRow;
    std::unique_ptr<ControlRow> bypassPedalRow, bypassAmpRow, bypassCabRow;
    std::unique_ptr<ControlRow> liteRow, inGainRow, outGainRow;

    // library browser overlay
    juce::Component libOverlay;
    juce::Label libTitle;
    juce::Viewport libViewport;
    juce::Component libContent;
    juce::TextButton libClose { "CLOSE" };
    NamLibrary::Kind browsingKind = NamLibrary::Kind::Amp;
    juce::OwnedArray<juce::TextButton> libRows;

    // ---- TONE3000 ----
    juce::Component t3kPage;
    juce::Label t3kStatus;
    juce::TextEditor t3kKeyEditor;
    juce::TextButton t3kSaveKey { "SAVE KEY" };
    juce::TextButton t3kPasteKey;  // clipboard icon
    juce::TextButton t3kPasteCode; // clipboard icon
    juce::TextButton t3kLogin { "CONNECT" };
    juce::TextButton t3kLogout { "LOGOUT" };
    juce::TextEditor t3kSearch;
    juce::ComboBox t3kGear;
    juce::ComboBox t3kSort;
    juce::ComboBox t3kSource; // Catalog / Favorites / My tones
    juce::TextButton t3kSearchBtn { "SEARCH" };
    juce::TextButton t3kPrevPage { "<" };
    juce::TextButton t3kNextPage { ">" };
    juce::Label t3kPageLabel;
    juce::Viewport t3kViewport;
    juce::Component t3kContent;
    juce::Label t3kHint;
    juce::TextEditor t3kCodePaste;
    juce::TextButton t3kCodeSubmit { "SUBMIT CODE" };
    juce::OwnedArray<juce::TextButton> t3kRows;
    juce::Array<Tone3000Client::ToneInfo> lastTones;
    int t3kPageIndex = 1;
    int t3kTotalPages = 1;
    void showSearchResults (const Tone3000Client::SearchResult& result);
    void runCurrentSearch();
    static NamLibrary::Kind kindFromGear (const juce::String& gear, const juce::String& format);


    // Same keyboard overlay as PluginBrowser / preset rename
    class KeyboardOverlay : public juce::Component, private juce::Timer
    {
    public:
        OnScreenKeyboard keyboard;
        juce::TextEditor field;
        juce::TextButton doneBtn { "DONE" };
        juce::Label hint;
        float fade = 0.0f;
        std::function<void(juce::String)> onDone;

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

            hint.setText ("Search TONE3000", juce::dontSendNotification);
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
            if (! keyboard.getBounds().contains (e.getPosition())
                && ! field.getBounds().contains (e.getPosition())
                && ! doneBtn.getBounds().contains (e.getPosition()))
            {
                if (onDone) onDone (field.getText());
            }
        }
    };

    std::unique_ptr<KeyboardOverlay> kbOverlay;
    void openSearchKeyboard();
    void closeSearchKeyboard (const juce::String& text);
    void updateAuthVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeNamPanel)
};
