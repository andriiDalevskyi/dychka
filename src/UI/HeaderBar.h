// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <functional>

class DychkaAudioProcessor;

namespace dychka
{

/** Top header: wordmark, slot LCD (TIME and VOLUME slot + name), Save/Load bank, Undo/Redo,
    BEAT and MIDI LEDs, output peak meter and the output gain knob (bound to apvts "output"). */
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (DychkaAudioProcessor& processor);
    ~HeaderBar() override;

    /** Re-reads the slot names / numbers and the undo state. */
    void refresh();

    /** beat 0..n; lit = the LED is in its flash window. Beat 1 flashes red, the others green. */
    void setBeat (int beat, bool lit);
    void setMidiLedOn (bool on);
    void setMeterLevels (float peakL, float peakR);

    /** Called after a successful bank Save/Load so the editor can refresh every panel. */
    std::function<void (const juce::String& message)> onBankChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DychkaAudioProcessor& processor;

    juce::TextButton saveButton { "Save Bank" };
    juce::TextButton loadButton { "Load Bank" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::Slider outputKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> logoBounds, wordmarkBounds, lcdBounds, meterBounds;
    juce::String timeSlotText, timeName, volSlotText, volName;
    bool editingTime = true;

    int beat = 0;
    bool beatLit = false, midiLit = false;
    float peakL = 0.0f, peakR = 0.0f;

    void saveBank();
    void loadBank();
    void drawLed (juce::Graphics&, juce::Rectangle<int> area, const juce::String& caption, bool lit, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};

/** The Dychka mark: a saw-tooth time ramp (three half-time teeth) with teal drop lines - the same
    drawing as resources/icon_512.png. Shared by the header and the slot grid. */
void drawDychkaLogo (juce::Graphics& g, juce::Rectangle<float> r);

} // namespace dychka
