// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/DychkaLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/SlotPanel.h"
#include "UI/EnvelopeEditor.h"
#include "UI/ControlPanel.h"

class DychkaAudioProcessor;

/** Top-level editor: fixed 1100x720 layout per ARCHITECTURE.md. Polls the processor at 30 Hz for
    the play head / LEDs / meters, refreshes every panel when the processor broadcasts a change
    and accepts dropped .json envelope / bank files. */
class DychkaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer,
                                   private juce::ChangeListener,
                                   public juce::FileDragAndDropTarget
{
public:
    explicit DychkaAudioProcessorEditor (DychkaAudioProcessor&);
    ~DychkaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override; // Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;

private:
    DychkaAudioProcessor& audioProcessor;

    dychka::DychkaLookAndFeel lookAndFeel;
    dychka::HeaderBar header;
    dychka::SlotPanel slots;
    dychka::EnvelopeEditor editor;
    dychka::ControlPanel controls;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> footerBounds;
    juce::String statusMessage;
    juce::int64 statusMessageMs = 0;
    juce::int64 lastMidiActivityMs = -100000;
    juce::String lastFooterRight;
    bool dragHover = false;

    void refreshAll();
    void showMessage (const juce::String& message);
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DychkaAudioProcessorEditor)
};
