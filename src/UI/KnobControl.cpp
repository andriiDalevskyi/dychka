// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "KnobControl.h"
#include "DychkaLookAndFeel.h"

namespace dychka
{

void KnobControl::ValueSlider::mouseDown (const juce::MouseEvent& e)
{
    // Ctrl+click = reset. Swallow the whole click so the knob never jumps to the mouse position.
    if (e.mods.isCommandDown() && e.mods.isLeftButtonDown())
    {
        swallowingResetClick = true;
        if (onResetClick)
            onResetClick();
        return;
    }

    swallowingResetClick = false;
    juce::Slider::mouseDown (e);
}

void KnobControl::ValueSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (swallowingResetClick)
        return;
    juce::Slider::mouseDrag (e);
}

void KnobControl::ValueSlider::mouseUp (const juce::MouseEvent& e)
{
    if (swallowingResetClick)
    {
        swallowingResetClick = false;
        return;
    }
    juce::Slider::mouseUp (e);
}

KnobControl::KnobControl (const juce::String& captionText, Layout layoutToUse) : layout (layoutToUse)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.onResetClick = [this]
    {
        if (! slider.isEnabled())
            return;
        slider.setValue (defaultValue, juce::sendNotificationSync);
    };
    slider.onValueChange = [this]
    {
        refreshField();
        if (onValueChange)
            onValueChange (slider.getValue());
    };
    slider.onDragStart = [this] { if (onGestureStart) onGestureStart(); };
    slider.onDragEnd   = [this] { if (onGestureEnd) onGestureEnd(); };
    addAndMakeVisible (slider);

    caption.setText (captionText, juce::dontSendNotification);
    caption.setFont (DychkaLookAndFeel::labelFont (10.0f));
    caption.setJustificationType (layout == Layout::vertical ? juce::Justification::centred
                                                             : juce::Justification::centredLeft);
    caption.setColour (juce::Label::textColourId, DychkaLookAndFeel::label);
    caption.setInterceptsMouseClicks (false, false); // clicks on the caption belong to the panel (focus release)
    addAndMakeVisible (caption);

    field.setFont (DychkaLookAndFeel::monoFont (11.0f));
    field.setJustification (juce::Justification::centred);
    field.setSelectAllWhenFocused (true);
    field.setMultiLine (false);
    field.setReturnKeyStartsNewLine (false);
    field.onFocusLost = [this] { commitField(); };
    field.onReturnKey = [this] { commitField(); giveAwayKeyboardFocus(); };
    field.onEscapeKey = [this] { refreshField(); giveAwayKeyboardFocus(); };
    addAndMakeVisible (field);
}

void KnobControl::setRange (double lo, double hi, double interval)
{
    slider.setRange (lo, hi, interval);
}

void KnobControl::setDefaultValue (double newDefault)
{
    defaultValue = newDefault;
    slider.setDoubleClickReturnValue (true, newDefault);
}

void KnobControl::setValue (double newValue)
{
    slider.setValue (newValue, juce::dontSendNotification);
    refreshField();
}

void KnobControl::setThumbColour (juce::Colour colour)
{
    slider.setColour (juce::Slider::thumbColourId, colour);
}

void KnobControl::setTooltipText (const juce::String& tip)
{
    const auto full = tip.isEmpty() ? juce::String() : tip + "  (Ctrl+click = default, type a value below)";
    slider.setTooltip (full);
    field.setTooltip (full);
}

void KnobControl::setControlEnabled (bool shouldBeEnabled)
{
    slider.setEnabled (shouldBeEnabled);
    field.setEnabled (shouldBeEnabled);
    field.setReadOnly (! shouldBeEnabled);
    setAlpha (shouldBeEnabled ? 1.0f : 0.4f);
}

void KnobControl::refreshField (bool evenWhileTyping)
{
    if (! evenWhileTyping && field.hasKeyboardFocus (false))
        return; // never overwrite what the user is in the middle of typing

    field.setText (format ? format (slider.getValue()) : juce::String (slider.getValue(), 2),
                   juce::dontSendNotification);
}

void KnobControl::commitField()
{
    const auto typed = field.getText().trim();
    if (typed.isNotEmpty())
    {
        const double parsed = parse ? parse (typed) : typed.getDoubleValue();
        slider.setValue (parsed, juce::sendNotificationSync); // clamps to the range; fires onValueChange
    }

    refreshField (true); // echoes back what the knob actually took (clamped / snapped)
}

void KnobControl::resized()
{
    auto area = getLocalBounds();

    if (layout == Layout::vertical)
    {
        auto fieldArea = area.removeFromBottom (kFieldHeight);
        auto captionArea = area.removeFromBottom (kCaptionHeight);
        const int knobSize = juce::jmin (area.getWidth(), area.getHeight());
        slider.setBounds (area.withSizeKeepingCentre (knobSize, knobSize));
        caption.setBounds (captionArea);
        field.setBounds (fieldArea.withSizeKeepingCentre (juce::jmin (52, fieldArea.getWidth()), kFieldHeight));
        return;
    }

    const int knobSize = juce::jmin (area.getHeight(), 36);
    slider.setBounds (area.removeFromLeft (knobSize).withSizeKeepingCentre (knobSize, knobSize));
    area.removeFromLeft (6);

    auto stack = area.withSizeKeepingCentre (area.getWidth(), kCaptionHeight + kFieldHeight);
    caption.setBounds (stack.removeFromTop (kCaptionHeight));
    field.setBounds (stack.removeFromTop (kFieldHeight));
}

} // namespace dychka
