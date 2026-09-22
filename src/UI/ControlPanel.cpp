// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "ControlPanel.h"
#include "DychkaLookAndFeel.h"
#include "../PluginProcessor.h"

namespace dychka
{

namespace
{
    const char* const kHelpLines[] =
    {
        "TIME: a line going down = the read head",
        "falls behind. Slope 1/2 = half speed,",
        "1 = frozen, 2 = reverse; steps = stutter.",
        "VOLUME: a gain curve. Both loop with the",
        "song. MIDI notes pick slots (HOLD = latch).",
    };
}

ControlPanel::ControlPanel (DychkaAudioProcessor& processorIn) : processor (processorIn)
{
    setPaintingIsUnclipped (true);

    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Effect on / off (6 ms crossfade). Switching on restarts the envelopes");
    onAttachment = std::make_unique<juce::ButtonParameterAttachment> (param (ParamIDs::on), onButton);
    addAndMakeVisible (onButton);

    tapButton.setTooltip ("Tap tempo (internal sync): tap quarter notes. The first tap restarts the envelopes");
    tapButton.onClick = [this] { processor.tap(); };
    DychkaLookAndFeel::setAccentButton (tapButton, true);
    addAndMakeVisible (tapButton);

    cueButton.setTooltip ("Restart both envelopes from their start now");
    cueButton.onClick = [this] { processor.restart(); };
    addAndMakeVisible (cueButton);

    syncCombo.addItem ("HOST (DAW position)", 1);
    syncCombo.addItem ("INTERNAL (TEMPO / TAP)", 2);
    syncCombo.setTooltip ("Where the clock comes from. HOST follows the DAW song position, so the envelopes line up with the bars");
    syncAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (param (ParamIDs::sync), syncCombo);
    addAndMakeVisible (syncCombo);

    auto pct = [] (double v) { return juce::String ((int) std::round (v)); };
    auto ms = [] (double v) { return juce::String (v, v < 10.0 ? 1 : 0) + " ms"; };
    addKnob ("SMOOTH",  ParamIDs::smoothing, false, "Glide of the read head when the TIME envelope jumps: 0 = clean cross-faded cuts (stutters), higher = tape-like pitch glides (up to 60 ms)", pct);
    addKnob ("MIX",     ParamIDs::mix,       false, "Dry .. wet balance of the whole effect", pct);
    addKnob ("ATTACK",  ParamIDs::attack,    true,  "Rise time of the VOLUME envelope (ms) - softens the start of every gate", ms);
    addKnob ("RELEASE", ParamIDs::release,   true,  "Fall time of the VOLUME envelope (ms) - softens the end of every gate", ms);
    addKnob ("TEMPO",   ParamIDs::tempo,     true,  "Internal tempo 40..300 BPM (ignored in HOST sync)",
             [] (double v) { return juce::String (v, 1); });

    midiTimeToggle.setTooltip ("MIDI notes select TIME slots: the base note is slot 1, the next 35 notes are slots 2..36");
    midiTimeAttachment = std::make_unique<juce::ButtonParameterAttachment> (param (ParamIDs::midiTime), midiTimeToggle);
    addAndMakeVisible (midiTimeToggle);
    midiVolToggle.setTooltip ("MIDI notes select VOLUME slots: the base note is slot 1, the next 35 notes are slots 2..36");
    midiVolAttachment = std::make_unique<juce::ButtonParameterAttachment> (param (ParamIDs::midiVol), midiVolToggle);
    addAndMakeVisible (midiVolToggle);

    fillNoteCombo (timeNoteCombo);
    timeNoteCombo.setTooltip ("Note of TIME slot 1 (C4 = 60; Gross Beat calls it C5)");
    timeNoteAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (param (ParamIDs::timeNote), timeNoteCombo);
    addAndMakeVisible (timeNoteCombo);
    fillNoteCombo (volNoteCombo);
    volNoteCombo.setTooltip ("Note of VOLUME slot 1 (C1 = 24)");
    volNoteAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (param (ParamIDs::volNote), volNoteCombo);
    addAndMakeVisible (volNoteCombo);
}

ControlPanel::~ControlPanel() = default;

juce::RangedAudioParameter& ControlPanel::param (const char* id) const
{
    auto* p = processor.apvts.getParameter (id);
    jassert (p != nullptr);
    return *p;
}

void ControlPanel::fillNoteCombo (juce::ComboBox& box)
{
    // item count = parameter step count (0 .. 127 - 35)
    for (int n = 0; n <= 127 - kNumSlots + 1; ++n)
        box.addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 4) + " .. " + juce::MidiMessage::getMidiNoteName (n + kNumSlots - 1, true, true, 4)
                     + "  (" + juce::String (n) + ")", n + 1);
}

void ControlPanel::addKnob (const char* caption, const char* paramId, bool teal, const char* tooltip,
                            std::function<juce::String (double)> format)
{
    auto& p = param (paramId);
    BoundKnob b;
    b.knob = std::make_unique<KnobControl> (caption);
    b.knob->setTooltipText (tooltip);
    if (teal) b.knob->setThumbColour (DychkaLookAndFeel::teal);
    b.attachment = std::make_unique<juce::SliderParameterAttachment> (p, b.knob->getSlider());
    b.knob->setDefaultValue ((double) p.convertFrom0to1 (p.getDefaultValue()));
    if (format) b.knob->format = std::move (format);
    else        b.knob->format = [&p] (double v) { return p.getText (p.convertTo0to1 ((float) v), 0); };
    b.knob->parse = [&p] (const juce::String& t) { return (double) p.convertFrom0to1 (p.getValueForText (t)); };
    b.knob->refreshField (true);
    addAndMakeVisible (*b.knob);
    knobs.push_back (std::move (b));
}

void ControlPanel::tick()
{
    const auto& eng = processor.getEngine();
    const bool internal = processor.apvts.getRawParameterValue (ParamIDs::sync)->load() >= 0.5f;
    juce::String t = juce::String (eng.getEffectiveBpm(), 1) + " BPM";
    juce::String d;
    if (internal) d = "INTERNAL";
    else if (eng.isFollowingHost()) d = juce::String::fromUTF8 ("\xE2\x96\xB6 HOST");        // playing
    else if (processor.hasHostTransport()) d = juce::String::fromUTF8 ("\xE2\x96\xA0 HOST"); // stopped: free run
    else d = "NO HOST";
    if (t != tempoText || d != tempoDetail)
    {
        tempoText = t; tempoDetail = d;
        repaint (tempoLcdBounds);
    }
}

void ControlPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleBounds = area.removeFromTop (20);
    area.removeFromTop (8);

    onButton.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);

    auto tempoRow = area.removeFromTop (28);
    tapButton.setBounds (tempoRow.removeFromRight (46));
    tempoRow.removeFromRight (4);
    cueButton.setBounds (tempoRow.removeFromRight (44));
    tempoRow.removeFromRight (6);
    tempoLcdBounds = tempoRow;

    area.removeFromTop (8);
    syncCaptionBounds = area.removeFromTop (14);
    area.removeFromTop (2);
    syncCombo.setBounds (area.removeFromTop (26));

    area.removeFromTop (10);
    const int colW = area.getWidth() / 2;
    auto placeRow = [&] (int firstKnob, int count)
    {
        auto row = area.removeFromTop (KnobControl::kPreferredVerticalHeight);
        for (int i = 0; i < 2; ++i)
        {
            auto cell = row.removeFromLeft (i == 1 ? row.getWidth() : colW);
            if (i < count && firstKnob + i < (int) knobs.size())
                knobs[(size_t) (firstKnob + i)].knob->setBounds (cell);
        }
    };
    placeRow (0, 2);
    area.removeFromTop (6);
    placeRow (2, 2);
    area.removeFromTop (6);
    placeRow (4, 1);

    area.removeFromTop (10);
    midiCaptionBounds = area.removeFromTop (14);
    area.removeFromTop (2);
    midiTimeToggle.setBounds (area.removeFromTop (22));
    timeNoteCombo.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    midiVolToggle.setBounds (area.removeFromTop (22));
    volNoteCombo.setBounds (area.removeFromTop (24));

    area.removeFromTop (10);
    helpBounds = area;
}

void ControlPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (DychkaLookAndFeel::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (DychkaLookAndFeel::panelBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::labelFont (11.0f));
    g.drawText ("CONTROLS", titleBounds, juce::Justification::centredLeft);

    // Tempo LCD.
    g.setColour (DychkaLookAndFeel::lcdBg);
    g.fillRoundedRectangle (tempoLcdBounds.toFloat(), 4.0f);
    g.setColour (DychkaLookAndFeel::lcdBorder);
    g.drawRoundedRectangle (tempoLcdBounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
    {
        auto lcd = tempoLcdBounds.reduced (6, 0);
        g.setColour (DychkaLookAndFeel::amber);
        g.setFont (DychkaLookAndFeel::monoFont (12.0f, true));
        g.drawText (tempoText, lcd, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff6b6f77));
        g.setFont (DychkaLookAndFeel::monoFont (9.0f));
        g.drawText (tempoDetail, lcd, juce::Justification::centredRight);
    }

    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::labelFont (10.0f));
    g.drawText ("SYNC", syncCaptionBounds, juce::Justification::centredLeft);
    g.drawText ("MIDI SLOT SELECT", midiCaptionBounds, juce::Justification::centredLeft);

    g.setColour (DychkaLookAndFeel::label);
    g.setFont (DychkaLookAndFeel::sansFont (10.0f));
    const int lineHeight = 12;
    const int numLines = juce::jmin ((int) (sizeof (kHelpLines) / sizeof (kHelpLines[0])), juce::jmax (0, helpBounds.getHeight() / lineHeight));
    auto line = helpBounds.withHeight (lineHeight);
    for (int i = 0; i < numLines; ++i)
    {
        g.drawText (kHelpLines[i], line, juce::Justification::centredLeft);
        line.translate (0, lineHeight);
    }
}

void ControlPanel::mouseDown (const juce::MouseEvent&)
{
    giveAwayKeyboardFocus();
}

} // namespace dychka
