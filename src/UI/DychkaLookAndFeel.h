// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dychka
{

/** Central colour/font palette + custom widget drawing for Dychka (shared with Minigun and Pitarda).
    Colours are taken verbatim from design/Main.dc.html and design/PadStates.dc.html. */
class DychkaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DychkaLookAndFeel();
    ~DychkaLookAndFeel() override = default;

    //==============================================================================
    // Palette (hex values from the mockup CSS).
    static const juce::Colour body;            // #23262b (top of body gradient)
    static const juce::Colour bodyDark;        // #191b1f (bottom of body gradient)
    static const juce::Colour panel;           // #1f2226
    static const juce::Colour panelBorder;     // #2c3036
    static const juce::Colour outerBorder;     // #33373e
    static const juce::Colour lcdBg;           // #0e1011
    static const juce::Colour lcdBorder;       // #2a2e33
    static const juce::Colour amber;           // #f2a33a
    static const juce::Colour teal;            // #38c7c1
    static const juce::Colour text;            // #d6d3cc
    static const juce::Colour label;           // #8b8f97
    static const juce::Colour padRubberTop;    // #e6e3dd
    static const juce::Colour padRubberBottom; // #d3cfc8
    static const juce::Colour padEmptyTop;     // #d2cfc9
    static const juce::Colour padEmptyBottom;  // #bfbbb4
    static const juce::Colour padHitTop;       // #ffd08a
    static const juce::Colour padHitBottom;    // #f2a33a
    static const juce::Colour padDropTop;      // #cfe9e8
    static const juce::Colour padDropBottom;   // #b6dcda
    static const juce::Colour btnTop;          // #3a3e45
    static const juce::Colour btnBottom;       // #2a2d32
    static const juce::Colour btnBorder;       // #15171a
    static const juce::Colour btnAccentTop;    // #f6b25a
    static const juce::Colour btnAccentBottom; // #e0912a
    static const juce::Colour btnAccentBorder; // #6b4612
    static const juce::Colour segBg;           // #262930
    static const juce::Colour knobFace;        // #3a3f47
    static const juce::Colour knobRing;        // #2b2f35

    //==============================================================================
    // Fonts (system fonts stand in for the mockup's Google fonts; JUCE_WEB_BROWSER=0).
    /** Amber lightness ramp shared by the layer editor and the pad glow: layer 0 darkest, last brightest. */
    static void layerColours (int layerIndex, int layerCount, juce::Colour& top, juce::Colour& bottom);

    static juce::Font sansFont (float size, bool bold = false);
    static juce::Font monoFont (float size, bool bold = false);
    static juce::Font labelFont (float size = 11.0f); // uppercase, letter-spaced caller side

    // Marks a TextButton to be drawn in the mockup's ".btn-accent" amber style.
    static void setAccentButton (juce::Button& b, bool accent = true);
    static bool isAccentButton (const juce::Button& b);

    //==============================================================================
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // ComboBox: styled like the LCDs (dark bg, amber mono text, grey chevron).
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Dark popup menu to match the plugin chrome.
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};

} // namespace dychka
