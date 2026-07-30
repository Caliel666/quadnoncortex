#pragma once
#include <JuceHeader.h>
#include "Theme.h"

/** Modern flat on-screen keyboard (no close button — host overlay handles dismiss). */
class OnScreenKeyboard : public juce::Component
{
public:
    OnScreenKeyboard()
    {
        buildKeys (false);
    }

    ~OnScreenKeyboard() override
    {
        for (auto* b : keyButtons)
            b->setLookAndFeel (nullptr);
    }

    std::function<void(juce::String)> onKey;
    std::function<void()> onEnter;
    std::function<void()> onClose; // unused, kept for API compat

    void setShift (bool s)
    {
        if (shifted == s) return;
        shifted = s;
        for (auto* b : keyButtons)
            b->setLookAndFeel (nullptr);
        keyButtons.clear();
        buildKeys (shifted);
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // Transparent — sits on overlay
        juce::ignoreUnused (g);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        if (keyButtons.isEmpty()) return;

        const char* rowChars[4] = {
            "1234567890",
            shifted ? "QWERTYUIOP" : "qwertyuiop",
            shifted ? "ASDFGHJKL"  : "asdfghjkl",
            shifted ? "ZXCVBNM"    : "zxcvbnm"
        };
        const int rowH = r.getHeight() / 4;
        int idx = 0;

        auto placeRow = [&] (const char* chars, juce::Rectangle<int> rowR)
        {
            const int n = (int) std::strlen (chars);
            const int kw = rowR.getWidth() / juce::jmax (1, n);
            const int total = kw * n;
            rowR = rowR.withSizeKeepingCentre (total, rowR.getHeight());
            for (int i = 0; i < n && idx < keyButtons.size(); ++i, ++idx)
                keyButtons[idx]->setBounds (rowR.removeFromLeft (kw).reduced (3, 4));
        };

        placeRow (rowChars[0], r.removeFromTop (rowH));
        placeRow (rowChars[1], r.removeFromTop (rowH));
        placeRow (rowChars[2], r.removeFromTop (rowH));

        auto rowR = r;
        const int specialW = juce::jmax (70, rowR.getWidth() / 8);
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (specialW).reduced (3, 4));
        placeRow (rowChars[3], rowR.removeFromLeft (rowR.getWidth() * 4 / 6));
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (specialW).reduced (3, 4));
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (rowR.getWidth() / 2).reduced (3, 4));
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.reduced (3, 4));
    }

private:
    struct SoftKeyLaf : public juce::LookAndFeel_V4
    {
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions (juce::jlimit (16.0f, 28.0f, buttonHeight * 0.38f),
                                                  juce::Font::bold));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                   const juce::Colour&, bool isHighlighted, bool isDown) override
        {
            auto& th = Theme::get();
            auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            auto fill = isDown ? th.keyFacePressed
                               : (isHighlighted ? th.keyFace.brighter (0.12f) : th.keyFace);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 10.0f);
            g.setColour (th.border.withAlpha (0.5f));
            g.drawRoundedRectangle (bounds, 10.0f, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool, bool) override
        {
            g.setColour (Theme::get().text);
            g.setFont (getTextButtonFont (button, button.getHeight()));
            g.drawText (button.getButtonText(), button.getLocalBounds(),
                        juce::Justification::centred, false);
        }
    };

    void buildKeys (bool shift)
    {
        keyButtons.clear();
        auto addKey = [this] (const juce::String& label, const juce::String& insert)
        {
            auto* b = keyButtons.add (new juce::TextButton (label));
            b->setLookAndFeel (&keyLaf);
            b->setColour (juce::TextButton::textColourOffId, Theme::get().text);
            b->onClick = [this, insert, label]
            {
                if (label == "Shift") { setShift (! shifted); return; }
                if (label == "Enter") { if (onEnter) onEnter(); return; }
                if (onKey) onKey (insert);
            };
            addAndMakeVisible (b);
        };

        for (const char* p = "1234567890"; *p; ++p)
            addKey (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p),
                    juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));
        const char* r2 = shift ? "QWERTYUIOP" : "qwertyuiop";
        for (const char* p = r2; *p; ++p)
            addKey (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p),
                    juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));
        const char* r3 = shift ? "ASDFGHJKL" : "asdfghjkl";
        for (const char* p = r3; *p; ++p)
            addKey (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p),
                    juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));
        addKey ("Shift", {});
        const char* r4 = shift ? "ZXCVBNM" : "zxcvbnm";
        for (const char* p = r4; *p; ++p)
            addKey (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p),
                    juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));
        addKey ("Bksp", "\b");
        addKey ("Space", " ");
        addKey ("Enter", "\n");
    }

    SoftKeyLaf keyLaf;
    juce::OwnedArray<juce::TextButton> keyButtons;
    bool shifted = false;
};
