#pragma once
#include <JuceHeader.h>
#include "Theme.h"

/** Flat on-screen keyboard. attachEditor() uses caret/selection like a hardware keyboard. */
class OnScreenKeyboard : public juce::Component
{
public:
    OnScreenKeyboard() { rebuild (false); }

    ~OnScreenKeyboard() override
    {
        keyButtons.clear();
    }

    std::function<void(juce::String)> onKey;
    std::function<void()> onEnter;
    std::function<void()> onClose;

    void attachEditor (juce::TextEditor* ed) { boundEditor = ed; }

    void paint (juce::Graphics&) override {}

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
            rowR = rowR.withSizeKeepingCentre (kw * n, rowR.getHeight());
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
    // Custom key that can draw simple vector icons for specials
    class KeyBtn : public juce::Button
    {
    public:
        enum class Kind { Char, Shift, Backspace, Space, Enter };
        Kind kind = Kind::Char;
        juce::String send;

        KeyBtn (Kind k, juce::String label, juce::String s)
            : juce::Button (label), kind (k), send (std::move (s))
        {
            setWantsKeyboardFocus (false);
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto& th = Theme::get();
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour ((down || over) ? th.keyFacePressed : th.keyFace);
            g.fillRoundedRectangle (r, 10.0f);

            g.setColour (th.text);
            if (kind == Kind::Char || kind == Kind::Space)
            {
                g.setFont (juce::FontOptions (juce::jlimit (16.0f, 28.0f, (float) getHeight() * 0.38f),
                                              juce::Font::plain));
                g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
                return;
            }

            // Vector icons
            auto c = r.withSizeKeepingCentre (r.getWidth() * 0.45f, r.getHeight() * 0.4f);
            juce::Path path;
            if (kind == Kind::Shift)
            {
                // Classic shift arrow — full height/width of icon box
                const float midX = c.getCentreX();
                const float top  = c.getY();
                const float midY = c.getY() + c.getHeight() * 0.45f;
                const float bot  = c.getBottom();
                const float stem = c.getWidth() * 0.22f;
                path.startNewSubPath (midX, top);
                path.lineTo (c.getRight(), midY);
                path.lineTo (midX + stem, midY);
                path.lineTo (midX + stem, bot);
                path.lineTo (midX - stem, bot);
                path.lineTo (midX - stem, midY);
                path.lineTo (c.getX(), midY);
                path.closeSubPath();
                g.fillPath (path);
            }
            else if (kind == Kind::Backspace)
            {
                // Backspace chevron + X
                path.startNewSubPath (c.getRight(), c.getY());
                path.lineTo (c.getX() + c.getWidth() * 0.35f, c.getY());
                path.lineTo (c.getX(), c.getCentreY());
                path.lineTo (c.getX() + c.getWidth() * 0.35f, c.getBottom());
                path.lineTo (c.getRight(), c.getBottom());
                path.closeSubPath();
                g.strokePath (path, juce::PathStrokeType (2.0f));
                const float m = c.getWidth() * 0.12f;
                const float cx = c.getX() + c.getWidth() * 0.62f;
                const float cy = c.getCentreY();
                g.drawLine (cx - m, cy - m, cx + m, cy + m, 2.0f);
                g.drawLine (cx + m, cy - m, cx - m, cy + m, 2.0f);
            }
            else if (kind == Kind::Enter)
            {
                // Return arrow
                path.startNewSubPath (c.getRight(), c.getY() + c.getHeight() * 0.25f);
                path.lineTo (c.getRight(), c.getCentreY());
                path.lineTo (c.getX() + c.getWidth() * 0.35f, c.getCentreY());
                g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                path.clear();
                path.startNewSubPath (c.getX() + c.getWidth() * 0.45f, c.getCentreY() - c.getHeight() * 0.28f);
                path.lineTo (c.getX() + c.getWidth() * 0.2f, c.getCentreY());
                path.lineTo (c.getX() + c.getWidth() * 0.45f, c.getCentreY() + c.getHeight() * 0.28f);
                g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
        }
    };

    void handleKey (const juce::String& s)
    {
        if (s == "\n")
        {
            if (onEnter) onEnter();
            if (onKey) onKey (s);
            return;
        }

        if (boundEditor != nullptr)
        {
            // Do NOT grab focus every key — that retriggers selectAllWhenFocused
            boundEditor->setSelectAllWhenFocused (false);
            if (s == "\b")
                boundEditor->deleteBackwards (false);
            else if (s.isNotEmpty())
                boundEditor->insertTextAtCaret (s);
        }

        if (onKey)
            onKey (s);
    }

    void rebuild (bool shift)
    {
        shifted = shift;
        keyButtons.clear();

        auto addChar = [&] (juce::String ch)
        {
            auto* b = keyButtons.add (new KeyBtn (KeyBtn::Kind::Char, ch, ch));
            b->onClick = [this, ch] { handleKey (ch); };
            addAndMakeVisible (b);
        };

        const char* rows[4] = {
            "1234567890",
            shift ? "QWERTYUIOP" : "qwertyuiop",
            shift ? "ASDFGHJKL"  : "asdfghjkl",
            shift ? "ZXCVBNM"    : "zxcvbnm"
        };

        for (int row = 0; row < 3; ++row)
            for (const char* p = rows[row]; *p; ++p)
                addChar (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));

        {
            auto* b = keyButtons.add (new KeyBtn (KeyBtn::Kind::Shift, "shift", {}));
            b->onClick = [this] { rebuild (! shifted); resized(); repaint(); };
            addAndMakeVisible (b);
        }

        for (const char* p = rows[3]; *p; ++p)
            addChar (juce::String::charToString ((juce::juce_wchar) (juce::uint8) *p));

        {
            auto* b = keyButtons.add (new KeyBtn (KeyBtn::Kind::Backspace, "bksp", "\b"));
            b->onClick = [this] { handleKey ("\b"); };
            addAndMakeVisible (b);
        }
        {
            auto* b = keyButtons.add (new KeyBtn (KeyBtn::Kind::Space, "space", " "));
            b->onClick = [this] { handleKey (" "); };
            addAndMakeVisible (b);
        }
        {
            auto* b = keyButtons.add (new KeyBtn (KeyBtn::Kind::Enter, "enter", "\n"));
            b->onClick = [this] { handleKey ("\n"); };
            addAndMakeVisible (b);
        }
    }

    juce::OwnedArray<KeyBtn> keyButtons;
    bool shifted = false;
    juce::TextEditor* boundEditor = nullptr;
};
