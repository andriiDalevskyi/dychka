// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "DychkaLookAndFeel.h"

namespace dychka
{

const juce::Colour DychkaLookAndFeel::body            { 0xff23262b };
const juce::Colour DychkaLookAndFeel::bodyDark         { 0xff191b1f };
const juce::Colour DychkaLookAndFeel::panel            { 0xff1f2226 };
const juce::Colour DychkaLookAndFeel::panelBorder      { 0xff2c3036 };
const juce::Colour DychkaLookAndFeel::outerBorder      { 0xff33373e };
const juce::Colour DychkaLookAndFeel::lcdBg            { 0xff0e1011 };
const juce::Colour DychkaLookAndFeel::lcdBorder        { 0xff2a2e33 };
const juce::Colour DychkaLookAndFeel::amber            { 0xfff2a33a };
const juce::Colour DychkaLookAndFeel::teal             { 0xff38c7c1 };
const juce::Colour DychkaLookAndFeel::text             { 0xffd6d3cc };
const juce::Colour DychkaLookAndFeel::label            { 0xff8b8f97 };
const juce::Colour DychkaLookAndFeel::padRubberTop     { 0xffe6e3dd };
const juce::Colour DychkaLookAndFeel::padRubberBottom  { 0xffd3cfc8 };
const juce::Colour DychkaLookAndFeel::padEmptyTop      { 0xffd2cfc9 };
const juce::Colour DychkaLookAndFeel::padEmptyBottom   { 0xffbfbbb4 };
const juce::Colour DychkaLookAndFeel::padHitTop        { 0xffffd08a };
const juce::Colour DychkaLookAndFeel::padHitBottom     { 0xfff2a33a };
const juce::Colour DychkaLookAndFeel::padDropTop       { 0xffcfe9e8 };
const juce::Colour DychkaLookAndFeel::padDropBottom    { 0xffb6dcda };
const juce::Colour DychkaLookAndFeel::btnTop           { 0xff3a3e45 };
const juce::Colour DychkaLookAndFeel::btnBottom        { 0xff2a2d32 };
const juce::Colour DychkaLookAndFeel::btnBorder        { 0xff15171a };
const juce::Colour DychkaLookAndFeel::btnAccentTop     { 0xfff6b25a };
const juce::Colour DychkaLookAndFeel::btnAccentBottom  { 0xffe0912a };
const juce::Colour DychkaLookAndFeel::btnAccentBorder  { 0xff6b4612 };
const juce::Colour DychkaLookAndFeel::segBg            { 0xff262930 };
const juce::Colour DychkaLookAndFeel::knobFace         { 0xff3a3f47 };
const juce::Colour DychkaLookAndFeel::knobRing         { 0xff2b2f35 };

DychkaLookAndFeel::DychkaLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bodyDark);
    setColour (juce::Slider::thumbColourId, amber);
    setColour (juce::Slider::rotarySliderFillColourId, knobFace);
    setColour (juce::Slider::rotarySliderOutlineColourId, btnBorder);
    setColour (juce::TextButton::buttonColourId, btnTop);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::textColourId, amber);
    setColour (juce::TextEditor::backgroundColourId, lcdBg);
    setColour (juce::TextEditor::outlineColourId, lcdBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, teal);
    setColour (juce::ToggleButton::tickColourId, teal);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, teal);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff062524));
    setColour (juce::ComboBox::backgroundColourId, lcdBg);
    setColour (juce::ComboBox::outlineColourId, lcdBorder);
    setColour (juce::ComboBox::textColourId, amber);
    setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff6b6f77));
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff44484f));
}

juce::Font DychkaLookAndFeel::sansFont (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font DychkaLookAndFeel::monoFont (float size, bool bold)
{
    auto opts = juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)
                    .withName ("Consolas");
    return juce::Font (opts);
}

juce::Font DychkaLookAndFeel::labelFont (float size)
{
    return sansFont (size, true);
}

void DychkaLookAndFeel::setAccentButton (juce::Button& b, bool accent)
{
    b.getProperties().set ("mg_accent", accent);
}

bool DychkaLookAndFeel::isAccentButton (const juce::Button& b)
{
    return (bool) b.getProperties().getWithDefault ("mg_accent", false);
}

juce::Font DychkaLookAndFeel::getLabelFont (juce::Label& l)
{
    return sansFont (l.getFont().getHeight(), true);
}

juce::Font DychkaLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return sansFont (juce::jmin (13.0f, buttonHeight * 0.5f), true);
}

void DychkaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();

    // Outer ring: dark disc with a slightly darker ring border.
    auto outerR = radius;
    g.setColour (knobRing);
    g.fillEllipse (centre.x - outerR, centre.y - outerR, outerR * 2.0f, outerR * 2.0f);
    g.setColour (btnBorder);
    g.drawEllipse (centre.x - outerR, centre.y - outerR, outerR * 2.0f, outerR * 2.0f, 1.5f);

    // Inner face.
    auto innerR = outerR * 0.76f;
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.fillEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);

    // Pointer.
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path pointer;
    auto pointerLength = innerR * 0.86f;
    auto pointerStart = innerR * 0.18f;
    pointer.startNewSubPath (centre.x, centre.y - pointerStart);
    pointer.lineTo (centre.x, centre.y - pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (1.8f, radius * 0.13f),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
}

void DychkaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    const bool accent = isAccentButton (button);
    const bool toggledOn = button.getToggleState() && ! accent;

    if (toggledOn)
    {
        auto fill = teal;
        if (shouldDrawButtonAsDown) fill = fill.darker (0.15f);
        else if (shouldDrawButtonAsHighlighted) fill = fill.brighter (0.08f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (teal.darker (0.4f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    auto top = accent ? btnAccentTop : btnTop;
    auto bottom = accent ? btnAccentBottom : btnBottom;
    auto border = accent ? btnAccentBorder : btnBorder;

    if (shouldDrawButtonAsDown)
    {
        top = top.darker (0.15f);
        bottom = bottom.darker (0.15f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        top = top.brighter (0.06f);
        bottom = bottom.brighter (0.06f);
    }

    juce::ColourGradient grad (top, bounds.getX(), bounds.getY(), bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void DychkaLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    const bool accent = isAccentButton (button);
    const bool toggledOn = button.getToggleState() && ! accent;

    if (accent)      g.setColour (juce::Colour (0xff2a1a04));
    else if (toggledOn) g.setColour (juce::Colour (0xff062524));
    else             g.setColour (text);

    g.setFont (sansFont (13.0f, true));

    auto text_ = button.getButtonText().toUpperCase();
    g.drawFittedText (text_, button.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

void DychkaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto pillW = 30.0f, pillH = 16.0f;
    auto pill = juce::Rectangle<float> (bounds.getRight() - pillW, bounds.getCentreY() - pillH * 0.5f, pillW, pillH);

    g.setColour (button.getToggleState() ? teal : juce::Colour (0xff33373e));
    g.fillRoundedRectangle (pill, pillH * 0.5f);

    auto knobD = pillH - 4.0f;
    auto knobX = button.getToggleState() ? pill.getRight() - knobD - 2.0f : pill.getX() + 2.0f;
    g.setColour (lcdBg);
    g.fillEllipse (knobX, pill.getY() + 2.0f, knobD, knobD);

    if (button.getButtonText().isNotEmpty())
    {
        g.setColour (label);
        g.setFont (sansFont (10.0f, true));
        auto textArea = bounds.withTrimmedRight (pillW + 8.0f);
        g.drawFittedText (button.getButtonText().toUpperCase(), textArea.toNearestInt(),
                          juce::Justification::centredLeft, 1);
    }
}

void DychkaLookAndFeel::layerColours (int layerIndex, int layerCount, juce::Colour& top, juce::Colour& bottom)
{
    static const juce::Colour darkTop { 0xff5a4a2c }, darkBottom { 0xff3c3220 };
    static const juce::Colour brightTop { 0xfff2a33a }, brightBottom { 0xffc77d1f };
    const float t = layerCount <= 1 ? 1.0f : juce::jlimit (0.0f, 1.0f, (float) layerIndex / (float) (layerCount - 1));
    top = darkTop.interpolatedWith (brightTop, t);
    bottom = darkBottom.interpolatedWith (brightBottom, t);
}

void DychkaLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    g.setColour (te.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
}

void DychkaLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    if (te.isEnabled())
    {
        g.setColour (te.hasKeyboardFocus (true) ? te.findColour (juce::TextEditor::focusedOutlineColourId)
                                                 : te.findColour (juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (0.5f, 0.5f, width - 1.0f, height - 1.0f, 4.0f, 1.0f);
    }
}

void DychkaLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (lcdBg);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.isPopupActive() ? teal : lcdBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // Small "v" chevron (8 x 5 px) centred in the right-hand 18 px strip.
    auto chevronArea = bounds.removeFromRight (18.0f).withSizeKeepingCentre (8.0f, 5.0f);
    juce::Path p;
    p.startNewSubPath (chevronArea.getX(), chevronArea.getY());
    p.lineTo (chevronArea.getCentreX(), chevronArea.getBottom());
    p.lineTo (chevronArea.getRight(), chevronArea.getY());
    g.setColour (juce::Colour (0xff6b6f77));
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font DychkaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return monoFont (13.0f);
}

void DychkaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& textLabel)
{
    textLabel.setBounds (8, 1, box.getWidth() - 18 - 8, box.getHeight() - 2);
    textLabel.setFont (getComboBoxFont (box));
    textLabel.setColour (juce::Label::textColourId, amber);
    textLabel.setJustificationType (juce::Justification::centredLeft);
}

void DychkaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (panel);
    g.fillRect (0, 0, width, height);
    g.setColour (panelBorder);
    g.drawRect (0, 0, width, height, 1);
}

} // namespace dychka
