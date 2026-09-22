// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ClickFocusTextEditor.h"
#include <functional>

namespace dychka
{

/** A rotary knob with its caption and an editable value field.

    - drag / mouse wheel: the usual rotary behaviour
    - Ctrl (Cmd) + click: snaps the knob back to its default value
    - the value field takes a typed number ("-3.5", "120", "L40") and applies it on Return / focus loss

    `format` turns the knob value into the field text and `parse` turns typed text back into a
    value; both are supplied by the owner so the same control serves dB, pan, cents and ms. */
class KnobControl : public juce::Component
{
public:
    /** vertical   = knob, caption and field stacked (pad editor row)
        horizontal = knob on the left, caption over field on the right (compact advanced block) */
    enum class Layout { vertical, horizontal };

    explicit KnobControl (const juce::String& captionText, Layout layoutToUse = Layout::vertical);

    void setRange (double lo, double hi, double interval);
    /** The value Ctrl+click (and double click) returns to. */
    void setDefaultValue (double newDefault);
    /** Sets the knob without firing onValueChange, and refreshes the field text. */
    void setValue (double newValue);
    double getValue() const noexcept { return slider.getValue(); }

    /** The underlying slider, for a juce::SliderParameterAttachment: the attachment then owns the
        range, the value and the host gestures; only `format` / `parse` / setDefaultValue() stay
        with the owner. */
    juce::Slider& getSlider() noexcept { return slider; }

    void setThumbColour (juce::Colour colour);
    void setTooltipText (const juce::String& tip);
    /** Greys out and blocks knob + field (used while trigger mode drives the values). */
    void setControlEnabled (bool shouldBeEnabled);
    /** Re-renders the field text from the current knob value; a field being typed into is left
        alone unless evenWhileTyping is set (used to echo back the committed, clamped value). */
    void refreshField (bool evenWhileTyping = false);

    std::function<juce::String (double)> format;       // value -> field text
    std::function<double (const juce::String&)> parse; // typed text -> value (default: getDoubleValue)
    std::function<void (double)> onValueChange;        // drag, typed value or Ctrl+click reset
    std::function<void()> onGestureStart, onGestureEnd;

    void resized() override;

    static constexpr int kCaptionHeight = 12;
    static constexpr int kFieldHeight = 18;
    /** Height a vertical knob column wants: knob + caption + field. */
    static constexpr int kPreferredVerticalHeight = 38 + kCaptionHeight + kFieldHeight;

private:
    /** Slider that treats Ctrl+click as "reset to default" instead of the start of a drag. */
    class ValueSlider : public juce::Slider
    {
    public:
        std::function<void()> onResetClick;

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        bool swallowingResetClick = false;
    };

    ValueSlider slider;
    juce::Label caption;
    ClickFocusTextEditor field;
    Layout layout;
    double defaultValue = 0.0;

    void commitField();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobControl)
};

} // namespace dychka
