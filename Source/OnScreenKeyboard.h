#pragma once
#include <JuceHeader.h>

/** Simple US QWERTY on-screen keyboard for touch preset naming. */
class OnScreenKeyboard : public juce::Component
{
public:
    OnScreenKeyboard()
    {
        keyLaf.setDefaultSansSerifTypefaceName (juce::Font::getDefaultSansSerifFontName());
        buildKeys (false);
        closeBtn.setButtonText ("X");
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
        closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        closeBtn.setLookAndFeel (&keyLaf);
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeBtn);
    }

    ~OnScreenKeyboard() override
    {
        for (auto* b : keyButtons)
            b->setLookAndFeel (nullptr);
        closeBtn.setLookAndFeel (nullptr);
    }

    std::function<void(juce::String)> onKey;
    std::function<void()> onClose;
    std::function<void()> onEnter;

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
        g.fillAll (juce::Colour (0xff1a1a1a));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        closeBtn.setBounds (r.getRight() - 48, r.getY(), 44, 40);
        r.removeFromTop (44);
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
                keyButtons[idx]->setBounds (rowR.removeFromLeft (kw).reduced (4));
        };

        placeRow (rowChars[0], r.removeFromTop (rowH));
        placeRow (rowChars[1], r.removeFromTop (rowH));
        placeRow (rowChars[2], r.removeFromTop (rowH));

        auto rowR = r;
        const int specialW = juce::jmax (70, rowR.getWidth() / 8);
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (specialW).reduced (4)); // Shift
        placeRow (rowChars[3], rowR.removeFromLeft (rowR.getWidth() * 4 / 6));
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (specialW).reduced (4)); // Bksp
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.removeFromLeft (rowR.getWidth() / 2).reduced (4)); // Space
        if (idx < keyButtons.size())
            keyButtons[idx++]->setBounds (rowR.reduced (4)); // Enter
    }

private:
    struct BigKeyLaf : public juce::LookAndFeel_V4
    {
        juce::Font getTextButtonFont (juce::TextButton& b, int buttonHeight) override
        {
            // Special keys a bit smaller label-wise but still large
            const float size = juce::jlimit (18.0f, 36.0f, (float) buttonHeight * 0.42f);
            return juce::Font (juce::FontOptions (size, juce::Font::bold));
        }
    };

    void buildKeys (bool shift)
    {
        keyButtons.clear();
        auto addKey = [this] (const juce::String& label, const juce::String& insert)
        {
            auto* b = keyButtons.add (new juce::TextButton (label));
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff37474f));
            b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            b->setLookAndFeel (&keyLaf);
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

    BigKeyLaf keyLaf;
    juce::OwnedArray<juce::TextButton> keyButtons;
    juce::TextButton closeBtn;
    bool shifted = false;
};
