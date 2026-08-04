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
        // Fully transparent → no chrome (used by preset ^ / v)
        if (backgroundColour.getAlpha() < 8)
            return;

        auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
        const auto radius = juce::jlimit (8.0f, 14.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.22f);
        auto base = backgroundColour;
        if (shouldDrawButtonAsDown)
            base = base.darker (0.10f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.065f);

        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillRoundedRectangle (bounds.translated (0.0f, shouldDrawButtonAsDown ? 0.5f : 1.5f), radius);
        g.setColour (base);
        g.fillRoundedRectangle (bounds.translated (0.0f, shouldDrawButtonAsDown ? 1.0f : 0.0f), radius);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
        const bool isPresetSelector = box.getComponentID() == "presetSelector";
        if (! isPresetSelector)
        {
            g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (bounds, 12.0f);
        }

        const float arrowX = (float) width - 18.0f;
        const float arrowY = (float) height * 0.5f;
        juce::Path p;
        p.addTriangle (arrowX - 4.0f, arrowY - 2.5f,
                       arrowX + 4.0f, arrowY - 2.5f,
                       arrowX, arrowY + 3.5f);
        g.setColour (box.findColour (juce::ComboBox::textColourId).withAlpha (isPresetSelector ? 0.50f : 0.7f));
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

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        if (box.getComponentID() == "presetSelector")
            return juce::Font (juce::FontOptions (24.0f, juce::Font::bold));
        return juce::Font (juce::FontOptions (15.0f, juce::Font::bold));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        // Honour Label::setFont — do not force a fixed size
        return label.getFont();
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        // Keep label inside short/wide touch buttons (header, settings, etc.)
        return juce::Font (juce::FontOptions (juce::jlimit (11.0f, 18.0f, buttonHeight * 0.32f),
                                              juce::Font::bold));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        auto font = getTextButtonFont (button, button.getHeight());
        const auto text = button.getButtonText();
        auto area = button.getLocalBounds().reduced (6, 2);
        // Shrink until the string fits the button width
        auto textWidth = [&] (const juce::Font& f)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (f, text, 0.0f, 0.0f);
            return ga.getBoundingBox (0, -1, true).getWidth();
        };
        while (textWidth (font) > (float) area.getWidth() && font.getHeight() > 10.0f)
            font = font.withHeight (font.getHeight() - 1.0f);
        g.setFont (font);
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        g.drawText (text, area, juce::Justification::centred, false);
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
    juce::Colour border          { 0x00000000 };
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
        // Always refresh shipped defaults so palette updates apply after rebuilds
        themesDir().createDirectory();
        writePalette ("Dark",          0xff0e1014, 0xff161a21, 0xff1c222c, 0xff1e2430, 0xffeceff4, 0xff9aa3b5, 0xff62b0e8, 0xff1a2f40, 0xffe57373, 0xff81c784, 0xffffb74d, 0xe6080a0e, 0xff2a3140, 0xff3d4a5c, 0xff12151a, 0xff1a1f28, 0xff243040);
        writePalette ("Light",         0xfff3f4f6, 0xfffafbfc, 0xffe8eaee, 0xffffffff, 0xff1f2329, 0xff6b7280, 0xff3584e4, 0xffdceaf8, 0xffe05c5c, 0xff3a9e5c, 0xffd4920a, 0xcc1a1d22, 0xffe4e6ea, 0xffd0d3d9, 0xfff7f8fa, 0xffe8eaee, 0xffd6e4f5);
        writePalette ("OLED",          0xff000000, 0xff050505, 0xff0a0a0a, 0xff0f0f0f, 0xffe8e8e8, 0xff888888, 0xff4fc3f7, 0xff0a2030, 0xffff5252, 0xff69f0ae, 0xffffd740, 0xf0000000, 0xff1a1a1a, 0xff2a2a2a, 0xff000000, 0xff111111, 0xff1a2830);
        writePalette ("Dracula",       0xff282a36, 0xff21222c, 0xff343746, 0xff44475a, 0xfff8f8f2, 0xff6272a4, 0xffbd93f9, 0xff3a2f55, 0xffff5555, 0xff50fa7b, 0xffffb86c, 0xe6151620, 0xff44475a, 0xff6272a4, 0xff21222c, 0xff343746, 0xff44475a);
        writePalette ("Catppuccin",    0xff1e1e2e, 0xff181825, 0xff313244, 0xff45475a, 0xffcdd6f4, 0xffa6adc8, 0xffcba6f7, 0xff2a2040, 0xfff38ba8, 0xffa6e3a1, 0xfff9e2af, 0xe611111b, 0xff313244, 0xff45475a, 0xff181825, 0xff313244, 0xff45475a);
        writePalette ("Nord",          0xff2e3440, 0xff3b4252, 0xff434c5e, 0xff4c566a, 0xffeceff4, 0xffd8dee9, 0xff88c0d0, 0xff2a3a48, 0xffbf616a, 0xffa3be8c, 0xffebcb8b, 0xe62e3440, 0xff434c5e, 0xff4c566a, 0xff2e3440, 0xff3b4252, 0xff434c5e);
        writePalette ("Gruvbox",       0xff282828, 0xff1d2021, 0xff3c3836, 0xff504945, 0xffebdbb2, 0xffa89984, 0xfffe8019, 0xff3a2a18, 0xfffb4934, 0xffb8bb26, 0xfffabd2f, 0xe61d2021, 0xff3c3836, 0xff504945, 0xff1d2021, 0xff3c3836, 0xff504945);
        writePalette ("Tokyo Night",   0xff1a1b26, 0xff16161e, 0xff24283b, 0xff292e42, 0xffc0caf5, 0xff565f89, 0xff7aa2f7, 0xff1a2740, 0xfff7768e, 0xff9ece6a, 0xffe0af68, 0xe6111140, 0xff24283b, 0xff292e42, 0xff16161e, 0xff24283b, 0xff292e42);
        writePalette ("One Dark",      0xff282c34, 0xff21252b, 0xff2c313a, 0xff3e4451, 0xffabb2bf, 0xff5c6370, 0xff61afef, 0xff1e3a55, 0xffe06c75, 0xff98c379, 0xffe5c07b, 0xe61c1f26, 0xff2c313a, 0xff3e4451, 0xff21252b, 0xff2c313a, 0xff3e4451);
        writePalette ("Solarized Dark",0xff002b36, 0xff073642, 0xff094553, 0xff586e75, 0xfffdf6e3, 0xff93a1a1, 0xff268bd2, 0xff0a3a4a, 0xffdc322f, 0xff859900, 0xffb58900, 0xe6001f27, 0xff073642, 0xff094553, 0xff002b36, 0xff073642, 0xff094553);
        writePalette ("Adwaita Dark",  0xff1e1e1e, 0xff242424, 0xff303030, 0xff3a3a3a, 0xffffffff, 0xffc0c0c0, 0xff3584e4, 0xff1a3050, 0xffe01b24, 0xff2ec27e, 0xffe5a50a, 0xe6101010, 0xff303030, 0xff404040, 0xff1e1e1e, 0xff2a2a2a, 0xff3584e4);
        writePalette ("Adwaita Light", 0xfffafafa, 0xffffffff, 0xffebebeb, 0xffffffff, 0xff2e3436, 0xff5e5c64, 0xff3584e4, 0xffdceaf8, 0xffe01b24, 0xff2ec27e, 0xffe5a50a, 0xcc1a1d22, 0xffe4e4e4, 0xffd0d0d0, 0xffffffff, 0xffebebeb, 0xffd6e4f5);
        writePalette ("Breeze Dark",   0xff1b1e20, 0xff232629, 0xff2a2e32, 0xff31363b, 0xffeff0f1, 0xff7f8c8d, 0xff3daee9, 0xff1a3548, 0xffda4453, 0xff27ae60, 0xfff67400, 0xe6121416, 0xff2a2e32, 0xff3b4045, 0xff1b1e20, 0xff2a2e32, 0xff3daee9);
        writePalette ("Breeze Light",  0xffeff0f1, 0xfffcfcfc, 0xffe8e8e8, 0xffffffff, 0xff232629, 0xff7f8c8d, 0xff3daee9, 0xffd6eef9, 0xffda4453, 0xff27ae60, 0xfff67400, 0xcc1a1d22, 0xffe4e4e4, 0xffd0d0d0, 0xfffcfcfc, 0xffe8e8e8, 0xffd6eef9);
        writePalette ("Monokai",       0xff272822, 0xff1e1f1c, 0xff3e3d32, 0xff49483e, 0xfff8f8f2, 0xff75715e, 0xff66d9ef, 0xff1a3a42, 0xfff92672, 0xffa6e22e, 0xffe6db74, 0xe6151612, 0xff3e3d32, 0xff49483e, 0xff1e1f1c, 0xff3e3d32, 0xff49483e);
        writePalette ("Everforest",    0xff2d353b, 0xff232a2e, 0xff3d484d, 0xff475258, 0xffd3c6aa, 0xff859289, 0xff7fbbb3, 0xff1e3538, 0xffe67e80, 0xffa7c080, 0xffdbbc7f, 0xe61a2024, 0xff3d484d, 0xff475258, 0xff232a2e, 0xff3d484d, 0xff475258);
        writePalette ("Rose Pine",     0xff191724, 0xff1f1d2e, 0xff26233a, 0xff403d52, 0xffe0def4, 0xff908caa, 0xffc4a7e7, 0xff2a2040, 0xffeb6f92, 0xff9ccfd8, 0xfff6c177, 0xe612101b, 0xff26233a, 0xff403d52, 0xff1f1d2e, 0xff26233a, 0xff403d52);
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

    void writePalette (const juce::String& name,
                       juce::uint32 background, juce::uint32 surface, juce::uint32 surfaceAlt,
                       juce::uint32 card, juce::uint32 text, juce::uint32 textDim,
                       juce::uint32 accent, juce::uint32 accentSoft,
                       juce::uint32 danger, juce::uint32 success, juce::uint32 warning,
                       juce::uint32 overlay, juce::uint32 keyFace, juce::uint32 keyFacePressed,
                       juce::uint32 topBar, juce::uint32 tabIdleVal, juce::uint32 tabActiveVal)
    {
        auto f = themesDir().getChildFile (name + ".xml");
        juce::XmlElement root ("Theme");
        root.setAttribute ("name", name);
        auto put = [&] (const char* tag, juce::uint32 argb)
        {
            auto* e = root.createNewChildElement (tag);
            e->setAttribute ("argb", juce::String::toHexString ((int) argb).paddedLeft ('0', 8));
        };
        put ("background", background);
        put ("surface", surface);
        put ("surfaceAlt", surfaceAlt);
        put ("card", card);
        put ("border", 0x00000000);
        put ("text", text);
        put ("textDim", textDim);
        put ("accent", accent);
        put ("accentSoft", accentSoft);
        put ("danger", danger);
        put ("success", success);
        put ("warning", warning);
        put ("overlay", overlay);
        put ("keyFace", keyFace);
        put ("keyFacePressed", keyFacePressed);
        put ("topBar", topBar);
        put ("tabIdle", tabIdleVal);
        put ("tabActive", tabActiveVal);
        root.writeTo (f);
    }
};
