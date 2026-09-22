// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "HeaderBar.h"
#include "DychkaLookAndFeel.h"
#include "../Model/BankStore.h"
#include "../PluginProcessor.h"

namespace dychka
{

void drawDychkaLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Three saw teeth: each ramps down (the read head falling behind = half-time) and snaps back
    // with a teal drop line. Amber / light-amber alternate like the Diskach mark.
    const int teeth = 3;
    const float w = r.getWidth() / (float) teeth;
    const float top = r.getY() + r.getHeight() * 0.18f;
    const float bottom = r.getBottom() - r.getHeight() * 0.12f;

    for (int t = 0; t < teeth; ++t)
    {
        const float x0 = r.getX() + t * w + 1.0f;
        const float x1 = x0 + w - 2.0f;
        juce::Path ramp;
        ramp.startNewSubPath (x0, top);
        ramp.lineTo (x1, bottom);
        g.setColour (t % 2 == 0 ? DychkaLookAndFeel::amber : DychkaLookAndFeel::padHitTop);
        g.strokePath (ramp, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (DychkaLookAndFeel::teal);
        g.fillRoundedRectangle (x1 - 0.8f, top, 1.6f, bottom - top, 0.8f);
    }
}

namespace
{
    juce::Path saveIconPath (juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addRoundedRectangle (r, 2.0f);
        p.startNewSubPath (r.getX() + r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.55f);
        p.lineTo (r.getRight() - r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.55f);
        p.lineTo (r.getRight() - r.getWidth() * 0.2f, r.getBottom());
        p.lineTo (r.getX() + r.getWidth() * 0.2f, r.getBottom());
        p.closeSubPath();
        return p;
    }
}

HeaderBar::HeaderBar (DychkaAudioProcessor& processorIn) : processor (processorIn)
{
    setPaintingIsUnclipped (true);

    saveButton.setTooltip ("Save all 36 + 36 envelope slots as a bank file (Documents\\Dychka Banks)");
    loadButton.setTooltip ("Load a bank file, or an envelope file into the current slot of its kind");
    saveButton.onClick = [this] { saveBank(); };
    loadButton.onClick = [this] { loadBank(); };

    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y / Ctrl+Shift+Z)");
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    outputKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    outputKnob.setTooltip ("Output level (-60..+6 dB)");
    addAndMakeVisible (outputKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::output, outputKnob);

    refresh();
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::refresh()
{
    auto describe = [this] (EnvKind k, juce::String& slotText, juce::String& name)
    {
        const int slot = processor.getCurrentSlot (k);
        const auto& e = processor.getEnvelope (k, slot);
        slotText = juce::String (slot + 1).paddedLeft ('0', 2);
        name = e.isEmpty() ? juce::String ("-") : e.name;
        if (processor.isSlotModified (k, slot)) name += "*";
    };
    describe (EnvKind::time, timeSlotText, timeName);
    describe (EnvKind::volume, volSlotText, volName);
    editingTime = processor.getEditKind() == EnvKind::time;

    undoButton.setEnabled (processor.canUndo());
    undoButton.setAlpha (processor.canUndo() ? 1.0f : 0.35f);
    redoButton.setEnabled (processor.canRedo());
    redoButton.setAlpha (processor.canRedo() ? 1.0f : 0.35f);
    repaint();
}

void HeaderBar::setBeat (int newBeat, bool lit)
{
    if (beat != newBeat || beatLit != lit) { beat = newBeat; beatLit = lit; repaint(); }
}

void HeaderBar::setMidiLedOn (bool on)
{
    if (midiLit != on) { midiLit = on; repaint(); }
}

void HeaderBar::setMeterLevels (float l, float r)
{
    peakL = l; peakR = r;
    repaint (meterBounds.expanded (2));
}

void HeaderBar::saveBank()
{
    auto existing = processor.getBankFile();
    if (existing.getParentDirectory().isDirectory() && existing.getFileName().isNotEmpty())
    {
        if (processor.saveBankToFile (existing))
            if (onBankChanged) onBankChanged ("Bank saved to " + existing.getFileName());
        return;
    }

    auto startDir = BankStore::defaultBanksRoot();
    startDir.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Save bank", startDir.getChildFile ("My Envelopes" + juce::String (BankStore::kBankExtension)),
                                                       juce::String ("*") + BankStore::kBankExtension + ";*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.getFileName().endsWithIgnoreCase (".json"))
                file = file.getParentDirectory().getChildFile (file.getFileName() + BankStore::kBankExtension);
            if (processor.saveBankToFile (file))
                if (onBankChanged) onBankChanged ("Bank saved to " + file.getFileName());
        });
}

void HeaderBar::loadBank()
{
    auto startDir = BankStore::defaultBanksRoot();
    fileChooser = std::make_unique<juce::FileChooser> ("Load bank / envelope", startDir, "*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            juce::String message;
            processor.importFile (file, message);
            if (onBankChanged) onBankChanged (message);
        });
}

void HeaderBar::drawLed (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& caption, bool lit, juce::Colour colour)
{
    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::labelFont (11.0f));
    g.drawText (caption, area.removeFromLeft (36), juce::Justification::centredLeft);

    const float ledR = 4.5f;
    const juce::Point<float> centre ((float) area.getX() + 7.0f, (float) area.getCentreY());
    g.setColour (lit ? colour : colour.withAlpha (0.18f));
    if (lit)
    {
        juce::DropShadow ds (colour.withAlpha (0.8f), 9, {});
        juce::Path dot; dot.addEllipse (centre.x - ledR, centre.y - ledR, ledR * 2.0f, ledR * 2.0f);
        ds.drawForPath (g, dot);
    }
    g.fillEllipse (centre.x - ledR, centre.y - ledR, ledR * 2.0f, ledR * 2.0f);
}

void HeaderBar::paint (juce::Graphics& g)
{
    drawDychkaLogo (g, logoBounds.toFloat().reduced (1.0f));

    g.setColour (juce::Colour (0xffe9e6df));
    g.setFont (DychkaLookAndFeel::sansFont (22.0f, true));
    g.drawText ("DYCHKA", wordmarkBounds, juce::Justification::centredLeft);

    // Slot LCD: "T 03 Half-time 1 bar | V 01 Off" - the edited kind in amber.
    g.setColour (DychkaLookAndFeel::lcdBg);
    g.fillRoundedRectangle (lcdBounds.toFloat(), 4.0f);
    g.setColour (DychkaLookAndFeel::lcdBorder);
    g.drawRoundedRectangle (lcdBounds.toFloat().reduced (0.5f), 4.0f, 1.0f);

    auto lcd = lcdBounds.reduced (8, 0);
    const int half = lcd.getWidth() / 2;
    auto drawHalf = [&] (juce::Rectangle<int> area, const char* tag, const juce::String& slot, const juce::String& name, bool active)
    {
        const auto strong = active ? DychkaLookAndFeel::amber : juce::Colour (0xff8b8f97);
        const auto weak = juce::Colour (0xff6b6f77);
        g.setColour (weak);
        g.setFont (DychkaLookAndFeel::monoFont (10.0f));
        g.drawText (tag, area.removeFromLeft (14), juce::Justification::centredLeft);
        g.setColour (strong);
        g.setFont (DychkaLookAndFeel::monoFont (15.0f, true));
        g.drawText (slot, area.removeFromLeft (26), juce::Justification::centredLeft);
        g.setFont (DychkaLookAndFeel::monoFont (12.0f));
        g.drawText (name, area, juce::Justification::centredLeft, true);
    };
    drawHalf (lcd.removeFromLeft (half), "T", timeSlotText, timeName, editingTime);
    g.setColour (DychkaLookAndFeel::lcdBorder);
    g.fillRect (lcd.getX() - 1, lcd.getY() + 6, 1, lcd.getHeight() - 12);
    drawHalf (lcd.withTrimmedLeft (6), "V", volSlotText, volName, ! editingTime);

    // Save / load icons.
    {
        auto iconArea = juce::Rectangle<float> ((float) saveButton.getX() + 9.0f, saveButton.getBounds().getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour (DychkaLookAndFeel::text);
        g.strokePath (saveIconPath (iconArea), juce::PathStrokeType (1.6f));
        auto loadArea = juce::Rectangle<float> ((float) loadButton.getX() + 9.0f, loadButton.getBounds().getCentreY() - 6.0f, 12.0f, 12.0f);
        juce::Path folder; folder.addRoundedRectangle (loadArea, 2.0f);
        g.strokePath (folder, juce::PathStrokeType (1.6f));
    }

    // LEDs: BEAT (red on 1, green on the others) and MIDI.
    auto ledArea = juce::Rectangle<int> (meterBounds.getX() - 160, 0, 130, getHeight());
    const auto beatColour = beat == 0 ? juce::Colour (0xffe8503a) : juce::Colour (0xff5fd36b);
    drawLed (g, ledArea.removeFromLeft (60), "BEAT", beatLit, beatColour);
    ledArea.removeFromLeft (8);
    drawLed (g, ledArea.removeFromLeft (60), "MIDI", midiLit, DychkaLookAndFeel::teal);

    // Peak meter (8 segments, max(L,R)).
    static const float thresholds[8] = { -40.0f, -30.0f, -24.0f, -18.0f, -12.0f, -8.0f, -4.0f, 0.0f };
    const float level = juce::Decibels::gainToDecibels (juce::jmax (peakL, peakR), -80.0f);

    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::labelFont (11.0f));
    g.drawText ("OUT", meterBounds.withX (meterBounds.getX() - 34).withWidth (30), juce::Justification::centredLeft);

    const int segW = 5, segGap = 2;
    for (int i = 0; i < 8; ++i)
    {
        const bool lit = level >= thresholds[i];
        juce::Colour c = i >= 6 ? juce::Colour (0xffe8503a) : (i < 4 ? DychkaLookAndFeel::teal : DychkaLookAndFeel::amber);
        if (! lit) c = juce::Colour (0xff2a2e33);
        g.setColour (c);
        g.fillRect (meterBounds.getX() + i * (segW + segGap), meterBounds.getY(), segW, meterBounds.getHeight());
    }
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced (24, 0);
    const int midY = getHeight() / 2;

    logoBounds = juce::Rectangle<int> (area.getX(), midY - 11, 36, 22);
    area.removeFromLeft (36 + 10);
    wordmarkBounds = area.removeFromLeft (104).withY (0).withHeight (getHeight());
    area.removeFromLeft (14);

    lcdBounds = area.removeFromLeft (270).withHeight (30).withY (midY - 15);
    area.removeFromLeft (12);

    saveButton.setBounds (area.removeFromLeft (92).withHeight (30).withY (midY - 15));
    area.removeFromLeft (6);
    loadButton.setBounds (area.removeFromLeft (92).withHeight (30).withY (midY - 15));
    area.removeFromLeft (12);
    undoButton.setBounds (area.removeFromLeft (56).withHeight (30).withY (midY - 15));
    area.removeFromLeft (6);
    redoButton.setBounds (area.removeFromLeft (56).withHeight (30).withY (midY - 15));

    auto right = getLocalBounds().reduced (24, 0);
    outputKnob.setBounds (right.removeFromRight (34).withHeight (34).withY (midY - 17));
    right.removeFromRight (18);
    meterBounds = right.removeFromRight (8 * 5 + 7 * 2).withHeight (18).withY (midY - 9);
}

} // namespace dychka
