// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "SlotPanel.h"
#include "DychkaLookAndFeel.h"
#include "../Model/FactoryEnvelopes.h"
#include "../Model/BankStore.h"
#include "../PluginProcessor.h"

namespace dychka
{

namespace
{
    constexpr int kCols = 4, kRows = 9;

    juce::String slotLabel (EnvKind k, int slot)
    {
        return juce::String::charToString (kindLetter (k)) + juce::String (slot + 1).paddedLeft ('0', 2);
    }
}

void drawEnvelopeThumbnail (juce::Graphics& g, const Envelope& e, EnvKind kind, juce::Rectangle<float> r, juce::Colour colour, float alpha)
{
    juce::Path p;
    const int n = juce::jmax (8, (int) r.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const float ph = juce::jmin (0.99999f, (float) i / (float) n);
        const float v = e.valueAt (ph);
        const float y = kind == EnvKind::time ? r.getY() + v * r.getHeight() : r.getBottom() - v * r.getHeight();
        const float x = r.getX() + (float) i / (float) n * r.getWidth();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colour.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
SlotPanel::SlotPanel (DychkaAudioProcessor& processorIn) : processor (processorIn)
{
    setPaintingIsUnclipped (true);

    importButton.setTooltip ("Load an envelope file into the edited slot (or a bank file)");
    importButton.onClick = [this] { importInto (processor.getEditKind(), processor.getCurrentSlot (processor.getEditKind())); };
    exportButton.setTooltip ("Save the edited slot as an envelope file");
    exportButton.onClick = [this] { exportSlot (processor.getEditKind(), processor.getCurrentSlot (processor.getEditKind())); };
    copyButton.setTooltip ("Copy the edited envelope to the clipboard (as JSON text)");
    copyButton.onClick = [this] { copySlot (processor.getEditKind(), processor.getCurrentSlot (processor.getEditKind())); };
    pasteButton.setTooltip ("Paste an envelope from the clipboard into the edited slot");
    pasteButton.onClick = [this] { pasteSlot (processor.getEditKind(), processor.getCurrentSlot (processor.getEditKind())); };
    resetButton.setTooltip ("Reset the edited slot to its factory shape (flat for user slots)");
    resetButton.onClick = [this] { processor.resetSlotToFactory (processor.getEditKind(), processor.getCurrentSlot (processor.getEditKind())); };
    for (auto* b : { &importButton, &exportButton, &copyButton, &pasteButton, &resetButton })
        addAndMakeVisible (*b);

    refresh();
}

SlotPanel::~SlotPanel() = default;

void SlotPanel::refresh()
{
    repaint();
}

void SlotPanel::tick()
{
    const auto& eng = processor.getEngine();
    bool changed = false;
    for (int k = 0; k < kNumKinds; ++k)
    {
        const int a = eng.getActiveSlot ((EnvKind) k);
        const bool h = eng.isNoteHeld ((EnvKind) k);
        if (a != activeShown[(size_t) k] || h != heldShown[(size_t) k]) { activeShown[(size_t) k] = a; heldShown[(size_t) k] = h; changed = true; }
    }
    if (changed) repaint();
}

juce::Rectangle<int> SlotPanel::cellRect (int kind, int slot) const
{
    const auto g = gridBounds[(size_t) kind];
    const int col = slot % kCols, row = slot / kCols;
    return { g.getX() + col * cellW, g.getY() + row * cellH, cellW, cellH };
}

bool SlotPanel::cellAt (juce::Point<int> p, int& kind, int& slot) const
{
    for (int k = 0; k < kNumKinds; ++k)
    {
        const auto g = gridBounds[(size_t) k];
        if (! g.contains (p)) continue;
        const int col = (p.x - g.getX()) / cellW, row = (p.y - g.getY()) / cellH;
        if (col < 0 || col >= kCols || row < 0 || row >= kRows) return false;
        kind = k;
        slot = row * kCols + col;
        return slot < kNumSlots;
    }
    return false;
}

void SlotPanel::paintCell (juce::Graphics& g, int kind, int slot, juce::Rectangle<int> r)
{
    const auto k = (EnvKind) kind;
    const auto& e = processor.getEnvelope (k, slot);
    const bool active = activeShown[(size_t) kind] == slot;
    const bool held = active && heldShown[(size_t) kind];
    const bool editing = processor.getEditKind() == k && processor.getCurrentSlot (k) == slot;
    const bool hovered = hoverKind == kind && hoverSlot == slot;
    const bool flat = e.isFlat (k);
    const auto colour = k == EnvKind::time ? DychkaLookAndFeel::amber : DychkaLookAndFeel::teal;

    auto cell = r.reduced (1).toFloat();
    g.setColour (active ? colour.withAlpha (held ? 0.36f : 0.22f) : DychkaLookAndFeel::lcdBg);
    g.fillRoundedRectangle (cell, 3.0f);

    drawEnvelopeThumbnail (g, e, k, cell.reduced (4.0f, 3.0f).withTrimmedTop (7.0f), colour, flat ? 0.3f : 0.95f);

    g.setColour (active ? juce::Colour (0xffe9e6df) : juce::Colour (0xff6b6f77));
    g.setFont (DychkaLookAndFeel::monoFont (8.0f));
    g.drawText (juce::String (slot + 1), r.reduced (3, 1).withHeight (9), juce::Justification::topLeft);

    if (processor.isSlotModified (k, slot))
    {
        g.setColour (DychkaLookAndFeel::amber);
        g.fillEllipse ((float) r.getRight() - 6.0f, (float) r.getY() + 3.0f, 3.0f, 3.0f);
    }

    g.setColour (editing ? DychkaLookAndFeel::teal : (active ? colour : DychkaLookAndFeel::lcdBorder));
    g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, editing ? 1.8f : 1.0f);
    if (hovered && ! editing)
    {
        g.setColour (DychkaLookAndFeel::text.withAlpha (0.3f));
        g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);
    }
}

juce::String SlotPanel::lcdText() const
{
    int kind = hoverKind, slot = hoverSlot;
    if (kind < 0)
    {
        kind = (int) processor.getEditKind();
        slot = processor.getCurrentSlot ((EnvKind) kind);
    }
    const auto k = (EnvKind) kind;
    const auto& e = processor.getEnvelope (k, slot);
    juce::String s = slotLabel (k, slot) + juce::String::fromUTF8 (" \xC2\xB7 ") + (e.isEmpty() ? juce::String ("-") : e.name)
                   + juce::String::fromUTF8 (" \xC2\xB7 ") + lengthName (e.lengthBeats);
    if (e.retrigger) s += juce::String::fromUTF8 (" \xC2\xB7 RETRIG");
    if (e.hold) s += juce::String::fromUTF8 (" \xC2\xB7 HOLD");
    return s;
}

void SlotPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (DychkaLookAndFeel::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (DychkaLookAndFeel::panelBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::labelFont (11.0f));
    auto title = titleBounds;
    g.drawText ("SLOTS", title.removeFromLeft (60), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff6b6f77));
    g.setFont (DychkaLookAndFeel::monoFont (10.0f));
    int modified = 0;
    for (int k = 0; k < kNumKinds; ++k) for (int i = 0; i < kNumSlots; ++i) if (processor.isSlotModified ((EnvKind) k, i)) ++modified;
    g.drawText (processor.getBank().name + juce::String::fromUTF8 (" \xC2\xB7 ") + juce::String (modified) + " edited", title, juce::Justification::centredRight, true);

    for (int k = 0; k < kNumKinds; ++k)
    {
        const auto kind = (EnvKind) k;
        const bool editing = processor.getEditKind() == kind;
        g.setColour (editing ? (kind == EnvKind::time ? DychkaLookAndFeel::amber : DychkaLookAndFeel::teal) : DychkaLookAndFeel::label);
        g.setFont (DychkaLookAndFeel::labelFont (10.0f));
        const int base = (int) std::lround (processor.apvts.getRawParameterValue (kind == EnvKind::time ? ParamIDs::timeNote : ParamIDs::volNote)->load());
        g.drawText (kind == EnvKind::time ? "TIME" : "VOLUME", captionBounds[(size_t) k], juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff6b6f77));
        g.setFont (DychkaLookAndFeel::monoFont (9.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (base, true, true, 4) + "-" + juce::MidiMessage::getMidiNoteName (base + kNumSlots - 1, true, true, 4),
                    captionBounds[(size_t) k], juce::Justification::centredRight);
        for (int slot = 0; slot < kNumSlots; ++slot)
            paintCell (g, k, slot, cellRect (k, slot));
    }

    // Slot LCD.
    g.setColour (DychkaLookAndFeel::lcdBg);
    g.fillRoundedRectangle (lcdBounds.toFloat(), 4.0f);
    g.setColour (DychkaLookAndFeel::lcdBorder);
    g.drawRoundedRectangle (lcdBounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.setColour (hoverKind >= 0 ? DychkaLookAndFeel::text : DychkaLookAndFeel::amber);
    g.setFont (DychkaLookAndFeel::monoFont (11.0f));
    g.drawText (lcdText(), lcdBounds.reduced (8, 0), juce::Justification::centredLeft, true);
}

void SlotPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    titleBounds = area.removeFromTop (20);
    area.removeFromTop (6);

    auto buttons2 = area.removeFromBottom (26);
    area.removeFromBottom (4);
    auto buttons1 = area.removeFromBottom (26);
    area.removeFromBottom (8);
    lcdBounds = area.removeFromBottom (26);
    area.removeFromBottom (8);

    const int w1 = (buttons1.getWidth() - 4) / 2;
    importButton.setBounds (buttons1.removeFromLeft (w1));
    buttons1.removeFromLeft (4);
    exportButton.setBounds (buttons1);
    const int w2 = (buttons2.getWidth() - 8) / 3;
    copyButton.setBounds (buttons2.removeFromLeft (w2));
    buttons2.removeFromLeft (4);
    pasteButton.setBounds (buttons2.removeFromLeft (w2));
    buttons2.removeFromLeft (4);
    resetButton.setBounds (buttons2);

    const int gap = 8;
    cellW = (area.getWidth() - gap) / (2 * kCols);
    const int gridW = cellW * kCols;
    auto captions = area.removeFromTop (14);
    area.removeFromTop (3);
    cellH = juce::jlimit (18, 48, area.getHeight() / kRows);
    for (int k = 0; k < kNumKinds; ++k)
    {
        const int x = area.getX() + k * (gridW + gap);
        captionBounds[(size_t) k] = juce::Rectangle<int> (x, captions.getY(), gridW, captions.getHeight());
        gridBounds[(size_t) k] = juce::Rectangle<int> (x, area.getY(), gridW, cellH * kRows);
    }
}

//==============================================================================
void SlotPanel::mouseMove (const juce::MouseEvent& e)
{
    int kind = -1, slot = -1;
    if (! cellAt (e.getPosition(), kind, slot)) { kind = -1; slot = -1; }
    if (kind != hoverKind || slot != hoverSlot)
    {
        hoverKind = kind; hoverSlot = slot;
        if (kind >= 0)
        {
            const auto& env = processor.getEnvelope ((EnvKind) kind, slot);
            setTooltip (slotLabel ((EnvKind) kind, slot) + "  " + env.name + (env.info.isNotEmpty() ? "\n" + env.info : juce::String())
                        + "\nClick: select for playing and editing. Right-click: copy / paste / reset / load a factory shape.");
        }
        else setTooltip ({});
        repaint();
    }
}

void SlotPanel::mouseExit (const juce::MouseEvent&)
{
    if (hoverKind >= 0) { hoverKind = hoverSlot = -1; setTooltip ({}); repaint(); }
}

void SlotPanel::mouseDown (const juce::MouseEvent& e)
{
    giveAwayKeyboardFocus();
    int kind = -1, slot = -1;
    if (! cellAt (e.getPosition(), kind, slot)) return;
    const auto k = (EnvKind) kind;
    processor.setEditKind (k);
    processor.setCurrentSlot (k, slot);
    if (e.mods.isPopupMenu())
        showSlotMenu (kind, slot);
}

void SlotPanel::showSlotMenu (int kind, int slot)
{
    const auto k = (EnvKind) kind;
    juce::PopupMenu menu;
    menu.addSectionHeader (slotLabel (k, slot) + "  " + processor.getEnvelope (k, slot).name);
    menu.addItem (1, "Copy");
    menu.addItem (2, "Paste");
    menu.addItem (3, "Reset to factory");
    menu.addSeparator();
    juce::PopupMenu shapes;
    const auto& factory = factoryBank().of (k);
    for (int i = 0; i < kNumSlots; ++i)
        if (i == 0 || factory[(size_t) i].name != "Init")
            shapes.addItem (100 + i, factory[(size_t) i].name);
    menu.addSubMenu ("Load factory shape", shapes);
    menu.addSeparator();
    menu.addItem (4, "Export envelope...");
    menu.addItem (5, "Import envelope...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cellRect (kind, slot))), [this, k, slot] (int result)
    {
        if (result == 1) copySlot (k, slot);
        else if (result == 2) pasteSlot (k, slot);
        else if (result == 3) processor.resetSlotToFactory (k, slot);
        else if (result == 4) exportSlot (k, slot);
        else if (result == 5) importInto (k, slot);
        else if (result >= 100)
        {
            const auto& shape = factoryEnvelope (k, result - 100);
            processor.replaceSlot (k, slot, shape);
            if (onMessage) onMessage ("Loaded \"" + shape.name + "\" into " + slotLabel (k, slot));
        }
    });
}

void SlotPanel::copySlot (EnvKind kind, int slot)
{
    juce::SystemClipboard::copyTextToClipboard (processor.getEnvelope (kind, slot).toJsonString (kind));
    if (onMessage) onMessage (slotLabel (kind, slot) + " copied to the clipboard");
}

void SlotPanel::pasteSlot (EnvKind kind, int slot)
{
    Envelope e;
    EnvKind fileKind = kind;
    if (! e.fromJsonString (juce::SystemClipboard::getTextFromClipboard(), &fileKind))
    {
        if (onMessage) onMessage ("Clipboard holds no Dychka envelope");
        return;
    }
    processor.replaceSlot (kind, slot, e);
    if (onMessage) onMessage ("Pasted \"" + e.name + "\" into " + slotLabel (kind, slot)
                              + (fileKind != kind ? juce::String (" (a ") + kindName (fileKind) + " envelope)" : juce::String()));
}

void SlotPanel::importInto (EnvKind kind, int slot)
{
    processor.setEditKind (kind);
    processor.setCurrentSlot (kind, slot);
    fileChooser = std::make_unique<juce::FileChooser> ("Import into " + slotLabel (kind, slot), BankStore::defaultBanksRoot(), "*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            juce::String message;
            processor.importFile (file, message);
            if (onMessage) onMessage (message);
        });
}

void SlotPanel::exportSlot (EnvKind kind, int slot)
{
    const auto& e = processor.getEnvelope (kind, slot);
    auto safeName = juce::File::createLegalFileName (e.name.isNotEmpty() ? e.name : "envelope");
    auto root = BankStore::defaultBanksRoot();
    root.createDirectory();
    auto suggested = root.getChildFile (safeName + juce::String (BankStore::kEnvelopeExtension));
    fileChooser = std::make_unique<juce::FileChooser> ("Export envelope", suggested, "*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, kind, slot] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.getFileName().endsWithIgnoreCase (".json")) file = file.getParentDirectory().getChildFile (file.getFileName() + BankStore::kEnvelopeExtension);
            const bool ok = processor.exportEnvelopeJson (kind, slot, file);
            if (onMessage) onMessage (ok ? "Exported " + file.getFileName() : "Export failed: " + file.getFileName());
        });
}

} // namespace dychka
