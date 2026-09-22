// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/EnvelopeModel.h"
#include <functional>
#include <memory>

class DychkaAudioProcessor;

namespace dychka
{

/** Left column: two 4 x 9 grids of slot buttons (TIME | VOLUME) with a thumbnail of every
    envelope, the slot LCD and Import / Export / Copy / Paste / Reset for the edited slot.
    Click selects a slot for playing and editing (the slot parameter + the edit kind); right-click
    opens the slot menu (copy / paste / reset / load a factory shape / export / import). */
class SlotPanel : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    explicit SlotPanel (DychkaAudioProcessor& processor);
    ~SlotPanel() override;

    /** Re-reads names / thumbnails / selection / modified flags. */
    void refresh();
    /** ~30 Hz: the slots the engine is actually playing (MIDI may differ from the parameter). */
    void tick();

    std::function<void (const juce::String& message)> onMessage;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    DychkaAudioProcessor& processor;

    std::array<juce::Rectangle<int>, kNumKinds> gridBounds, captionBounds;
    juce::Rectangle<int> titleBounds, lcdBounds;
    int cellW = 33, cellH = 30;

    juce::TextButton importButton { "Import" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton pasteButton { "Paste" };
    juce::TextButton resetButton { "Reset" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    int hoverKind = -1, hoverSlot = -1;
    std::array<int, kNumKinds> activeShown { -1, -1 };
    std::array<bool, kNumKinds> heldShown { false, false };

    bool cellAt (juce::Point<int> p, int& kind, int& slot) const;
    juce::Rectangle<int> cellRect (int kind, int slot) const;
    void paintCell (juce::Graphics&, int kind, int slot, juce::Rectangle<int> r);
    void showSlotMenu (int kind, int slot);
    void copySlot (EnvKind kind, int slot);
    void pasteSlot (EnvKind kind, int slot);
    void importInto (EnvKind kind, int slot);
    void exportSlot (EnvKind kind, int slot);
    juce::String lcdText() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotPanel)
};

/** Draws a small preview of an envelope into `r` (used by the slot grid and the library menu). */
void drawEnvelopeThumbnail (juce::Graphics& g, const Envelope& e, EnvKind kind, juce::Rectangle<float> r, juce::Colour colour, float alpha);

} // namespace dychka
