// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Model/BankStore.h"

using namespace dychka;

namespace
{
    constexpr int kWindowWidth = 1100;
    constexpr int kWindowHeight = 720;
    constexpr int kHeaderHeight = 56;
    constexpr int kFooterHeight = 28;
    constexpr int kSlotsWidth = 300;
    constexpr int kControlsWidth = 225;
}

DychkaAudioProcessorEditor::DychkaAudioProcessorEditor (DychkaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      audioProcessor (p),
      header (p),
      slots (p),
      editor (p),
      controls (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    addMouseListener (this, true);

    addAndMakeVisible (header);
    addAndMakeVisible (slots);
    addAndMakeVisible (editor);
    addAndMakeVisible (controls);

    header.onBankChanged = [this] (const juce::String& m) { showMessage (m); refreshAll(); };
    slots.onMessage = [this] (const juce::String& m) { showMessage (m); };
    editor.onMessage = [this] (const juce::String& m) { showMessage (m); };

    audioProcessor.addChangeListener (this);

    setSize (kWindowWidth, kWindowHeight);
    setResizable (false, false);

    refreshAll();
    startTimerHz (30);
}

DychkaAudioProcessorEditor::~DychkaAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DychkaAudioProcessorEditor::showMessage (const juce::String& message)
{
    statusMessage = message;
    statusMessageMs = (juce::int64) juce::Time::getMillisecondCounter();
    repaint (footerBounds);
}

void DychkaAudioProcessorEditor::refreshAll()
{
    header.refresh();
    slots.refresh();
    editor.refresh();
    repaint (footerBounds);
}

void DychkaAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void DychkaAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (auto* focused = dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()))
    {
        auto* clicked = e.eventComponent;
        if (clicked != focused && ! focused->isParentOf (clicked))
            grabKeyboardFocus();
    }
}

bool DychkaAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && ! mods.isAltDown())
    {
        const auto code = key.getKeyCode();
        if (code == 'Z' || code == 'z' || code == 26)
        {
            if (mods.isShiftDown()) audioProcessor.redo(); else audioProcessor.undo();
            return true;
        }
        if ((code == 'Y' || code == 'y' || code == 25) && ! mods.isShiftDown())
        {
            audioProcessor.redo();
            return true;
        }
    }
    return false;
}

//==============================================================================
bool DychkaAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (BankStore::kindOfFile (juce::File (f)).isNotEmpty()) return true;
    return false;
}

void DychkaAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int) { dragHover = true; repaint(); }
void DychkaAudioProcessorEditor::fileDragExit (const juce::StringArray&) { dragHover = false; repaint(); }

void DychkaAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    juce::String lastMessage;
    int imported = 0;
    for (const auto& path : files)
    {
        juce::File f (path);
        if (BankStore::kindOfFile (f).isEmpty()) continue;
        juce::String message;
        if (audioProcessor.importFile (f, message)) ++imported;
        lastMessage = message;
        // several envelope files: continue in the next slot of the edited kind
        const auto k = audioProcessor.getEditKind();
        audioProcessor.setCurrentSlot (k, juce::jmin (kNumSlots - 1, audioProcessor.getCurrentSlot (k) + 1));
    }
    if (imported > 0)
    {
        const auto k = audioProcessor.getEditKind();
        audioProcessor.setCurrentSlot (k, juce::jmax (0, audioProcessor.getCurrentSlot (k) - 1));
    }
    if (imported > 1) lastMessage = juce::String (imported) + " files imported; last: " + lastMessage;
    showMessage (lastMessage);
    repaint();
}

//==============================================================================
void DychkaAudioProcessorEditor::timerCallback()
{
    const auto& eng = audioProcessor.getEngine();
    const auto now = (juce::int64) juce::Time::getMillisecondCounter();

    const bool on = audioProcessor.apvts.getRawParameterValue (ParamIDs::on)->load() >= 0.5f;
    const float barPhase = eng.getBarPhase();
    const int beat = eng.getBeat();
    const int beatsPerBar = juce::jmax (1, eng.getBeatsPerBar());
    const float beatFrac = barPhase * (float) beatsPerBar - (float) beat;
    header.setBeat (beat, on && beatFrac < 0.35f);

    header.setMeterLevels (audioProcessor.outputPeakL.load(), audioProcessor.outputPeakR.load());

    if (audioProcessor.consumeMidiActivity())
        lastMidiActivityMs = now;
    header.setMidiLedOn ((now - lastMidiActivityMs) < 100);

    controls.tick();
    editor.tick();
    slots.tick();

    juce::String right = juce::String ("v") + JucePlugin_VersionString
                       + juce::String::fromUTF8 (" \xC2\xB7 ") + (eng.isFollowingHost() ? "HOST " : (audioProcessor.apvts.getRawParameterValue (ParamIDs::sync)->load() >= 0.5f ? "INT " : "FREE "))
                       + juce::String (eng.getEffectiveBpm(), 1) + " BPM"
                       + juce::String::fromUTF8 (" \xC2\xB7 bar ") + juce::String (eng.getBarCounter() + 1)
                       + juce::String::fromUTF8 (" \xC2\xB7 T") + juce::String (eng.getActiveSlot (EnvKind::time) + 1).paddedLeft ('0', 2)
                       + " V" + juce::String (eng.getActiveSlot (EnvKind::volume) + 1).paddedLeft ('0', 2)
                       + juce::String::fromUTF8 (" \xC2\xB7 ") + juce::String (-eng.getDelayBeats(), 2) + " b"
                       + juce::String::fromUTF8 (" \xC2\xB7 ") + juce::String ((int) std::round (eng.getGain() * 100.0f)) + " %";
    if (right != lastFooterRight) { lastFooterRight = right; repaint (footerBounds); }
    if (statusMessage.isNotEmpty() && now - statusMessageMs > 6000) { statusMessage.clear(); repaint (footerBounds); }
}

void DychkaAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bg (DychkaLookAndFeel::body, 0.0f, 0.0f, DychkaLookAndFeel::bodyDark, 0.0f, bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (dragHover ? DychkaLookAndFeel::teal : DychkaLookAndFeel::outerBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, dragHover ? 2.0f : 1.0f);

    g.setColour (juce::Colour (0xff0f1113));
    g.drawLine (0.0f, (float) kHeaderHeight, bounds.getWidth(), (float) kHeaderHeight, 1.0f);
    g.drawLine (0.0f, bounds.getHeight() - kFooterHeight, bounds.getWidth(), bounds.getHeight() - kFooterHeight, 1.0f);

    g.setFont (DychkaLookAndFeel::monoFont (10.0f));
    juce::String left;
    if (statusMessage.isNotEmpty())
    {
        g.setColour (DychkaLookAndFeel::amber);
        left = statusMessage;
    }
    else
    {
        g.setColour (juce::Colour (0xff6b6f77));
        const auto file = audioProcessor.getBankFile();
        left = file.getFileName().isNotEmpty() ? "Bank: " + file.getFullPathName().replaceCharacter ('\\', '/')
                                                : juce::String::fromUTF8 ("Bank: (factory, unsaved) \xC2\xB7 drop .dychka-envelope.json / .dychka-bank.json files here to import");
    }
    auto footer = footerBounds;
    auto rightArea = footer.removeFromRight (420);
    g.drawText (left, footer, juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xff6b6f77));
    g.drawText (lastFooterRight, rightArea, juce::Justification::centredRight);
}

void DychkaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds (area.removeFromTop (kHeaderHeight));

    footerBounds = area.removeFromBottom (kFooterHeight).reduced (24, 0);

    auto body = area.reduced (24, 0);
    body.removeFromTop (18);
    body.removeFromBottom (10);

    slots.setBounds (body.removeFromLeft (kSlotsWidth));
    body.removeFromLeft (15);
    controls.setBounds (body.removeFromRight (kControlsWidth));
    body.removeFromRight (15);
    editor.setBounds (body);
}
