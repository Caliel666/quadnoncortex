#pragma once
#include <JuceHeader.h>
#include "AppSettings.h"

//==============================================================================
/** Soft libadwaita / macOS-style LookAndFeel — no harsh white outlines. */
class SoftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoftLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0b0d10));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a3140));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a1f28));
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1f28));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a4158));
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x448b95a8));
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1a1f28));
        setColour (juce::AlertWindow::textColourId, juce::Colours::white);
        setColour (juce::AlertWindow::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff14181e));
        setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
        auto base = backgroundColour;
        if (shouldDrawButtonAsDown)
            base = base.darker (0.12f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.08f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 10.0f);
        // Soft inner edge only — no hard white stroke
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        g.setColour (button.findColour (juce::TextButton::textColourOffId));
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (12.0f, 16.0f, button.getHeight() * 0.42f),
                                                  juce::Font::bold)));
        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred, false);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

        const float arrowX = (float) width - 18.0f;
        const float arrowY = (float) height * 0.5f;
        juce::Path p;
        p.addTriangle (arrowX - 4.0f, arrowY - 2.5f,
                       arrowX + 4.0f, arrowY - 2.5f,
                       arrowX, arrowY + 3.5f);
        g.setColour (box.findColour (juce::ComboBox::textColourId).withAlpha (0.7f));
        g.fillPath (p);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Full opaque fill — avoids white corners behind the rounded menu
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (findColour (juce::PopupMenu::backgroundColourId));
        g.fillRoundedRectangle (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f, 12.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            g.setColour (findColour (juce::PopupMenu::textColourId).withAlpha (0.15f));
            g.fillRect (area.reduced (12, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }
        auto r = area.toFloat().reduced (4.0f, 2.0f);
        if (isHighlighted && isActive)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRoundedRectangle (r, 8.0f);
        }
        auto col = textColour != nullptr ? *textColour
                                         : findColour (isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                                                     : juce::PopupMenu::textColourId);
        g.setColour (isActive ? col : col.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText (text, area.reduced (14, 0), juce::Justification::centredLeft, true);
        juce::ignoreUnused (isTicked, hasSubMenu, shortcutKeyText, icon);
    }

    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override
    {
        // No outline — modern flat field
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto tick = bounds.removeFromLeft (bounds.getHeight()).reduced (4.0f);
        auto bg = button.getToggleState() ? juce::Colour (0xff4fc3f7)
                                          : juce::Colour (0xff2a3140);
        if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.08f);
        g.setColour (bg);
        g.fillRoundedRectangle (tick, 6.0f);
        if (button.getToggleState())
        {
            g.setColour (juce::Colours::white);
            auto tickPath = getTickShape (0.7f);
            g.fillPath (tickPath, tickPath.getTransformToScaleToFit (tick.reduced (3.0f), false));
        }
        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (button.getButtonText(), bounds.reduced (6.0f, 0), juce::Justification::centredLeft);
        juce::ignoreUnused (shouldDrawButtonAsDown);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        const bool vertical = style == juce::Slider::LinearVertical
                           || style == juce::Slider::LinearBarVertical;
        auto trackCol = slider.findColour (juce::Slider::backgroundColourId);
        if (trackCol.isTransparent())
            trackCol = juce::Colour (0xff2a3140);
        auto thumbCol = slider.findColour (juce::Slider::thumbColourId);

        if (vertical)
        {
            const float cx = (float) x + (float) width * 0.5f;
            const float trackW = juce::jlimit (6.0f, 14.0f, (float) width * 0.28f);
            auto track = juce::Rectangle<float> (cx - trackW * 0.5f, (float) y + 6.0f,
                                                trackW, (float) height - 12.0f);
            g.setColour (trackCol);
            g.fillRoundedRectangle (track, trackW * 0.5f);

            // Fill from bottom up to thumb
            const float thumbY = sliderPos;
            auto filled = juce::Rectangle<float> (track.getX(), thumbY,
                                                  track.getWidth(), track.getBottom() - thumbY);
            g.setColour (thumbCol.withAlpha (0.55f));
            g.fillRoundedRectangle (filled, trackW * 0.5f);

            g.setColour (thumbCol);
            g.fillEllipse (cx - 10.0f, thumbY - 10.0f, 20.0f, 20.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (cx - 5.0f, thumbY - 5.0f, 10.0f, 10.0f);
        }
        else
        {
            auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.35f,
                                                 (float) width, (float) height * 0.3f);
            g.setColour (trackCol);
            g.fillRoundedRectangle (track, 4.0f);
            auto filled = track.withWidth (juce::jmax (0.0f, sliderPos - (float) x));
            g.setColour (thumbCol);
            g.fillRoundedRectangle (filled, 4.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (sliderPos - 8.0f, (float) y + (float) height * 0.5f - 8.0f, 16.0f, 16.0f);
        }
        juce::ignoreUnused (minSliderPos, maxSliderPos);
    }

    int getDefaultScrollbarWidth() override { return 8; }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (16.0f));
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (15.0f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (13.0f, 17.0f, buttonHeight * 0.42f),
                                              juce::Font::bold));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (15.0f));
    }

    int getComboBoxHeight() // not override - helper
    {
        return 36;
    }
};

//==============================================================================
class Theme
{
public:
    static Theme& get()
    {
        static Theme t;
        return t;
    }

    SoftLookAndFeel softLaf;

    juce::Colour background      { 0xff0e1014 };
    juce::Colour surface         { 0xff161a21 };
    juce::Colour surfaceAlt      { 0xff1c222c };
    juce::Colour card            { 0xff1e2430 };
    juce::Colour border          { 0x00000000 }; // no harsh borders by default
    juce::Colour text            { 0xffeceff4 };
    juce::Colour textDim         { 0xff9aa3b5 };
    juce::Colour accent          { 0xff62b0e8 };
    juce::Colour accentSoft      { 0xff1a2f40 };
    juce::Colour danger          { 0xffe57373 };
    juce::Colour success         { 0xff81c784 };
    juce::Colour warning         { 0xffffb74d };
    juce::Colour overlay         { 0xe6080a0e };
    juce::Colour keyFace         { 0xff2a3140 };
    juce::Colour keyFacePressed  { 0xff3d4a5c };
    juce::Colour topBar          { 0xff12151a };
    juce::Colour tabIdle         { 0xff1a1f28 };
    juce::Colour tabActive       { 0xff243040 };

    juce::String currentName { "Dark" };
    std::function<void()> onThemeChanged;

    static juce::File themesDir()
    {
        auto d = AppSettings::getDataDir().getChildFile ("Themes");
        d.createDirectory();
        return d;
    }

    void ensureDefaults()
    {
        // Always refresh shipped defaults so light/dark stay correct after updates
        writeDefault ("Dark", true);
        writeDefault ("Light", false);
    }

    juce::StringArray listThemes()
    {
        ensureDefaults();
        juce::StringArray names;
        for (auto& f : themesDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (f.getFileNameWithoutExtension());
        names.sort (true);
        return names;
    }

    bool load (const juce::String& name)
    {
        ensureDefaults();
        auto f = themesDir().getChildFile (name + ".xml");
        if (! f.existsAsFile()) return false;
        if (auto xml = juce::XmlDocument::parse (f))
        {
            auto col = [&] (const char* tag, juce::Colour& c)
            {
                if (auto* e = xml->getChildByName (tag))
                    c = juce::Colour ((juce::uint32) e->getStringAttribute ("argb").getHexValue32());
            };
            col ("background", background);
            col ("surface", surface);
            col ("surfaceAlt", surfaceAlt);
            col ("card", card);
            col ("border", border);
            col ("text", text);
            col ("textDim", textDim);
            col ("accent", accent);
            col ("accentSoft", accentSoft);
            col ("danger", danger);
            col ("success", success);
            col ("warning", warning);
            col ("overlay", overlay);
            col ("keyFace", keyFace);
            col ("keyFacePressed", keyFacePressed);
            col ("topBar", topBar);
            col ("tabIdle", tabIdle);
            col ("tabActive", tabActive);
            currentName = name;
            applyToLookAndFeel();
            if (onThemeChanged) onThemeChanged();
            return true;
        }
        return false;
    }

    void applyToLookAndFeel()
    {
        softLaf.setColour (juce::ResizableWindow::backgroundColourId, background);
        softLaf.setColour (juce::TextButton::buttonColourId, surfaceAlt);
        softLaf.setColour (juce::TextButton::textColourOffId, text);
        softLaf.setColour (juce::TextButton::textColourOnId, text);
        softLaf.setColour (juce::ComboBox::backgroundColourId, surfaceAlt);
        softLaf.setColour (juce::ComboBox::textColourId, text);
        softLaf.setColour (juce::ComboBox::arrowColourId, textDim);
        softLaf.setColour (juce::PopupMenu::backgroundColourId, surface);
        softLaf.setColour (juce::PopupMenu::textColourId, text);
        softLaf.setColour (juce::PopupMenu::highlightedBackgroundColourId, accentSoft);
        softLaf.setColour (juce::PopupMenu::highlightedTextColourId, text);
        softLaf.setColour (juce::Label::textColourId, text);
        softLaf.setColour (juce::TextEditor::backgroundColourId, surfaceAlt);
        softLaf.setColour (juce::TextEditor::textColourId, text);
        softLaf.setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
        softLaf.setColour (juce::ListBox::backgroundColourId, surface);
        softLaf.setColour (juce::ListBox::textColourId, text);
        softLaf.setColour (juce::AlertWindow::backgroundColourId, surface);
        softLaf.setColour (juce::AlertWindow::textColourId, text);
        softLaf.setColour (juce::Slider::thumbColourId, accent);
        softLaf.setColour (juce::Slider::trackColourId, accent);
        softLaf.setColour (juce::Slider::backgroundColourId, surfaceAlt);
        softLaf.setColour (juce::ToggleButton::textColourId, text);
        softLaf.setColour (juce::ScrollBar::thumbColourId, textDim.withAlpha (0.35f));
    }

    void applyButton (juce::TextButton& b, bool primary = false, bool dangerBtn = false) const
    {
        if (dangerBtn)
            b.setColour (juce::TextButton::buttonColourId, danger);
        else if (primary)
            b.setColour (juce::TextButton::buttonColourId, accent.darker (0.05f));
        else
            b.setColour (juce::TextButton::buttonColourId, surfaceAlt);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, text);
        b.setColour (juce::TextButton::buttonOnColourId, accentSoft);
    }

    void applyToggleTab (juce::TextButton& b, bool active) const
    {
        b.setColour (juce::TextButton::buttonColourId, active ? tabActive : tabIdle);
        b.setColour (juce::TextButton::buttonOnColourId, tabActive);
        b.setColour (juce::TextButton::textColourOffId, active ? text : textDim);
        b.setColour (juce::TextButton::textColourOnId, text);
    }

private:
    Theme()
    {
        ensureDefaults();
        const auto saved = AppSettings::get().themeName;
        if (saved.isNotEmpty())
            load (saved);
        else
            load ("Dark");
    }

    void writeDefault (const juce::String& name, bool dark)
    {
        auto f = themesDir().getChildFile (name + ".xml");
        juce::XmlElement root ("Theme");
        root.setAttribute ("name", name);
        auto put = [&] (const char* tag, juce::uint32 argb)
        {
            auto* e = root.createNewChildElement (tag);
            e->setAttribute ("argb", juce::String::toHexString ((int) argb).paddedLeft ('0', 8));
        };

        if (dark)
        {
            put ("background", 0xff0e1014);
            put ("surface",    0xff161a21);
            put ("surfaceAlt", 0xff1c222c);
            put ("card",       0xff1e2430);
            put ("border",     0x00000000);
            put ("text",       0xffeceff4);
            put ("textDim",    0xff9aa3b5);
            put ("accent",     0xff62b0e8);
            put ("accentSoft", 0xff1a2f40);
            put ("danger",     0xffe57373);
            put ("success",    0xff81c784);
            put ("warning",    0xffffb74d);
            put ("overlay",    0xe6080a0e);
            put ("keyFace",    0xff2a3140);
            put ("keyFacePressed", 0xff3d4a5c);
            put ("topBar",     0xff12151a);
            put ("tabIdle",    0xff1a1f28);
            put ("tabActive",  0xff243040);
        }
        else
        {
            // Soft light-gray (libadwaita-ish), not pure white
            put ("background", 0xfff3f4f6);
            put ("surface",    0xfffafbfc);
            put ("surfaceAlt", 0xffe8eaee);
            put ("card",       0xffffffff);
            put ("border",     0x00000000);
            put ("text",       0xff1f2329);
            put ("textDim",    0xff6b7280);
            put ("accent",     0xff3584e4);
            put ("accentSoft", 0xffdceaf8);
            put ("danger",     0xffe05c5c);
            put ("success",    0xff3a9e5c);
            put ("warning",    0xffd4920a);
            put ("overlay",    0xcc1a1d22);
            put ("keyFace",    0xffe4e6ea);
            put ("keyFacePressed", 0xffd0d3d9);
            put ("topBar",     0xfff7f8fa);
            put ("tabIdle",    0xffe8eaee);
            put ("tabActive",  0xffd6e4f5);
        }
        root.writeTo (f);
    }
};
