// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "KnobControl.h"
#include <memory>
#include <vector>

class DychkaAudioProcessor;

namespace dychka
{

/** Right column: the global controls. EFFECT ON, tempo LCD + CUE + TAP, SYNC, the knobs
    (SMOOTH MIX / ATTACK RELEASE / TEMPO), the MIDI slot mapping (toggle + base note per kind)
    and a short help text. Every control is a parameter attachment; the LCD is refreshed from tick(). */
class ControlPanel : public juce::Component
{
public:
    explicit ControlPanel (DychkaAudioProcessor& processor);
    ~ControlPanel() override;

    /** ~30 Hz: tempo / sync / bar readout. */
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    DychkaAudioProcessor& processor;

    juce::TextButton onButton { "EFFECT ON" };
    std::unique_ptr<juce::ButtonParameterAttachment> onAttachment;
    juce::TextButton tapButton { "TAP" };
    juce::TextButton cueButton { "CUE" };
    juce::ComboBox syncCombo;
    std::unique_ptr<juce::ComboBoxParameterAttachment> syncAttachment;

    struct BoundKnob
    {
        std::unique_ptr<KnobControl> knob;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    std::vector<BoundKnob> knobs;

    juce::ToggleButton midiTimeToggle { "TIME NOTES" };
    juce::ToggleButton midiVolToggle { "VOLUME NOTES" };
    juce::ComboBox timeNoteCombo, volNoteCombo;
    std::unique_ptr<juce::ButtonParameterAttachment> midiTimeAttachment, midiVolAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> timeNoteAttachment, volNoteAttachment;

    juce::Rectangle<int> titleBounds, tempoLcdBounds, syncCaptionBounds, midiCaptionBounds, helpBounds;
    juce::String tempoText, tempoDetail;

    juce::RangedAudioParameter& param (const char* id) const;
    void addKnob (const char* caption, const char* paramId, bool teal, const char* tooltip,
                  std::function<juce::String (double)> format = nullptr);
    void fillNoteCombo (juce::ComboBox& box);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanel)
};

} // namespace dychka
