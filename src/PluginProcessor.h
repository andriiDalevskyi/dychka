// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include "Model/EnvelopeModel.h"
#include "Engine/DychkaEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

/** Parameter IDs (apvts). Everything on the control panel is a host-visible, automatable
    parameter; the envelopes live in the Bank model. */
namespace dychka::ParamIDs
{
    constexpr const char* on        = "on";        // effect on/off
    constexpr const char* timeSlot  = "timeSlot";  // 1..36
    constexpr const char* volSlot   = "volSlot";   // 1..36
    constexpr const char* sync      = "sync";      // Host / Internal
    constexpr const char* tempo     = "tempo";     // 40..300 BPM (internal)
    constexpr const char* smoothing = "smoothing"; // 0..100: glide of the read head
    constexpr const char* attack    = "attack";    // volume attack ms
    constexpr const char* release   = "release";   // volume release ms
    constexpr const char* mix       = "mix";       // 0..100 dry .. wet
    constexpr const char* output    = "output";    // -60..+6 dB
    constexpr const char* midiTime  = "midiTime";  // MIDI notes select TIME slots
    constexpr const char* midiVol   = "midiVol";   // MIDI notes select VOLUME slots
    constexpr const char* timeNote  = "timeNote";  // note of TIME slot 1 (0..92)
    constexpr const char* volNote   = "volNote";   // note of VOLUME slot 1 (0..92)

    inline const char* slot (EnvKind k) noexcept { return k == EnvKind::volume ? volSlot : timeSlot; }
}

/** Owner of the Bank model and the real-time DychkaEngine; implements the Processor <-> UI
    contract documented in ARCHITECTURE.md. */
class DychkaAudioProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster,
                             private juce::Timer,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    DychkaAudioProcessor();
    ~DychkaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Processor <-> UI contract (see ARCHITECTURE.md). Message thread only.

    dychka::Bank& getBank() noexcept { return bank; }
    const dychka::Bank& getBank() const noexcept { return bank; }

    /** Which envelope kind the editor shows (saved with the project). */
    dychka::EnvKind getEditKind() const noexcept { return editKind; }
    void setEditKind (dychka::EnvKind kind);

    /** The selected slot (0-based) of a kind = its slot parameter - 1. */
    int getCurrentSlot (dychka::EnvKind kind) const noexcept;
    /** Selects a slot: sets the slot parameter as a host gesture. */
    void setCurrentSlot (dychka::EnvKind kind, int slot);

    dychka::Envelope& getCurrentEnvelope (dychka::EnvKind kind) noexcept { return bank.of (kind)[(size_t) getCurrentSlot (kind)]; }
    const dychka::Envelope& getEnvelope (dychka::EnvKind kind, int slot) const noexcept { return bank.of (kind)[(size_t) juce::jlimit (0, dychka::kNumSlots - 1, slot)]; }

    /** Call after ANY edit to the current envelope of `kind`: records an undo step, publishes the
        envelope to the audio thread and calls sendChangeMessage() so every panel refreshes. */
    void envelopeEdited (dychka::EnvKind kind);
    /** Replaces a slot (import, paste, reset, library) with undo. */
    void replaceSlot (dychka::EnvKind kind, int slot, const dychka::Envelope& envelope);
    void resetSlotToFactory (dychka::EnvKind kind, int slot);
    bool isSlotModified (dychka::EnvKind kind, int slot) const;

    //==============================================================================
    // Undo / redo over envelope edits (parameters are the host's business).
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void beginUndoGesture() noexcept { ++undoGestureDepth; }
    void endUndoGesture() noexcept   { undoGestureDepth = juce::jmax (0, undoGestureDepth - 1); }

    //==============================================================================
    // Bank files.
    bool saveBankToFile (const juce::File& file);
    bool loadBankFromFile (const juce::File& file, juce::String* message = nullptr);
    juce::File getBankFile() const noexcept { return bankFile; }

    /** Imports an envelope JSON (into the current slot of the kind stored in the file) or a bank
        JSON. Returns a human-readable report; false on failure. */
    bool importFile (const juce::File& file, juce::String& message);
    bool exportEnvelopeJson (dychka::EnvKind kind, int slot, const juce::File& file);

    //==============================================================================
    // Transport actions.
    /** Tap tempo (UI button): sets TEMPO in internal sync and restarts the envelopes. */
    void tap();
    /** Restart both envelopes from their start now (CUE). */
    void restart() { engine.requestRestart(); }

    const dychka::DychkaEngine& getEngine() const noexcept { return engine; }
    /** True once per MIDI note-on since the last call (for the MIDI LED). */
    bool consumeMidiActivity() noexcept { return engine.midiActivity.exchange (false); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    bool hasHostTransport() const noexcept { return lastHostValid.load (std::memory_order_relaxed); }
    bool isHostPlaying() const noexcept { return lastHostPlaying.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static BusesProperties makeBusesProperties();

    dychka::EngineParams readParams() const noexcept;
    dychka::HostPosition readHostPosition();
    void setParam (const char* id, float plainValue);

    dychka::Bank bank;
    juce::File bankFile;
    dychka::EnvKind editKind = dychka::EnvKind::time;

    // Undo history: (kind, slot, envelope JSON before the edit).
    struct UndoEntry { int kind; int slot; juce::String json; };
    std::vector<UndoEntry> undoStack, redoStack;
    juce::String committedJson;   // JSON of the committed (kind, slot) as last committed
    int committedKind = -1, committedSlot = -1;
    juce::uint32 lastCommitMs = 0;
    int undoGestureDepth = 0;
    static constexpr size_t kMaxUndoSteps = 200;
    static constexpr juce::uint32 kUndoMergeWindowMs = 700;

    void rebaseCommit (dychka::EnvKind kind, int slot);
    void commitCurrent (dychka::EnvKind kind);
    void applyEntry (UndoEntry entry, std::vector<UndoEntry>& pushTo);
    void resetUndoHistory();
    void publishAll();

    dychka::DychkaEngine engine;
    double currentSampleRate = 44100.0;
    bool prevOn = true;

    std::atomic<bool> lastHostValid { false };
    std::atomic<bool> lastHostPlaying { false };
    std::atomic<bool> slotParamChanged { false };

    // tap tempo
    std::vector<juce::uint32> tapTimesMs;

    struct RawParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* timeSlot = nullptr;
        std::atomic<float>* volSlot = nullptr;
        std::atomic<float>* sync = nullptr;
        std::atomic<float>* tempo = nullptr;
        std::atomic<float>* smoothing = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* midiTime = nullptr;
        std::atomic<float>* midiVol = nullptr;
        std::atomic<float>* timeNote = nullptr;
        std::atomic<float>* volNote = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DychkaAudioProcessor)
};
