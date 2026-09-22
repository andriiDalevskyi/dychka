// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include "../Model/EnvelopeModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <vector>

namespace dychka
{

enum class SyncMode : int { host = 0, internal = 1 };

/** Per-block copy of the automatable parameters (read by the processor, consumed by the engine). */
struct EngineParams
{
    std::array<int, kNumKinds> slot { 0, 0 };        // active slot per kind, 0..35 (the parameter - 1)
    bool on = true;
    SyncMode sync = SyncMode::host;
    float tempoBpm = 120.0f;                         // internal sync (and the fallback without a host)
    float smoothing = 0.0f;                          // 0..1 -> one-pole glide of the read position, 0 = clean cross-faded jumps
    float attackMs = 1.0f, releaseMs = 1.0f;         // volume envelope smoothing
    float mix = 1.0f;                                // dry .. wet
    std::array<bool, kNumKinds> midiSlots { true, true }; // MIDI notes select slots
    std::array<int, kNumKinds> baseNote { 60, 24 };  // note of slot 1 per kind
};

/** What the processor learned from the host's play head this block. */
struct HostPosition
{
    bool valid = false;        // false = no play head (standalone) or no tempo info
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;          // position of the first sample in quarter notes
    double barStartPpq = 0.0;  // ppq of the last bar start
    int tsNumerator = 4, tsDenominator = 4;
};

struct EnginePoint { float x = 0.0f, y = 0.0f, tension = 0.0f; bool hold = false; };

/** The audio-thread copy of an Envelope (fixed size, no strings). */
struct EngineEnvelope
{
    int lengthBeats = kDefaultLengthBeats;
    bool retrigger = false, hold = false;
    int numPoints = 1;
    std::array<EnginePoint, kMaxPoints> pts {};

    void set (const Envelope& e)
    {
        lengthBeats = isValidLength (e.lengthBeats) ? e.lengthBeats : kDefaultLengthBeats;
        retrigger = e.retrigger;
        hold = e.hold;
        numPoints = juce::jlimit (1, kMaxPoints, (int) e.points.size());
        for (int i = 0; i < numPoints; ++i)
        {
            const auto& p = e.points[(size_t) i];
            pts[(size_t) i] = { p.x, p.y, p.tension, p.hold };
        }
        if (e.points.empty()) pts[0] = {};
    }

    /** Value at phase 0..1; `cache` remembers the current segment so a running phase costs O(1). */
    float valueAt (float phase, int& cache) const noexcept
    {
        int i = juce::jlimit (0, numPoints - 1, cache);
        if (pts[(size_t) i].x > phase) i = 0;
        while (i + 1 < numPoints && pts[(size_t) (i + 1)].x <= phase) ++i;
        cache = i;
        const auto& p0 = pts[(size_t) i];
        if (i + 1 >= numPoints || p0.hold) return p0.y;
        const auto& p1 = pts[(size_t) (i + 1)];
        const float dx = p1.x - p0.x;
        if (dx <= 1.0e-6f) return p1.y;
        return p0.y + (p1.y - p0.y) * curveShape ((phase - p0.x) / dx, p0.tension);
    }
};

/** The time & volume engine. One instance per plug-in; processBlock() runs on the audio thread
    and never allocates or blocks (the envelope hand-off uses a ScopedTryLock). See ARCHITECTURE.md.

    Time: the input is written into a ring buffer of two bars (at the slowest tempo); the TIME
    envelope says how far behind the write head the read head sits, in beats. Volume: the VOLUME
    envelope is a gain with attack / release smoothing. Both loop with the song position. */
class DychkaEngine
{
public:
    DychkaEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Message thread: publish one slot (after an edit / import). */
    void publishEnvelope (EnvKind kind, int slot, const Envelope& e);
    /** Message thread: publish the whole bank (bank load, state restore). */
    void publishBank (const Bank& bank);

    /** Restart both envelopes from their start at the next block (CUE, tap, footswitch on). In
        internal / free-running sync the beat counter is reset too. */
    void requestRestart() noexcept { restartRequested.store (true, std::memory_order_release); }

    /** buffer: in-place stereo (or mono in / stereo out). MIDI note-ons select slots sample-accurately
        (and are reported through pendingSlot so the processor can move the parameter). */
    void processBlock (juce::AudioBuffer<float>& buffer, int numInputChannels, const juce::MidiBuffer& midi,
                       const EngineParams& params, const HostPosition& host);

    //==============================================================================
    // Readouts (lock-free, for the UI at ~30 Hz).
    float getPhase (EnvKind k) const noexcept { return phaseAtomic[(size_t) k].load (std::memory_order_relaxed); }   // 0..1 within the envelope
    float getValue (EnvKind k) const noexcept { return valueAtomic[(size_t) k].load (std::memory_order_relaxed); }   // envelope value 0..1
    int getActiveSlot (EnvKind k) const noexcept { return activeSlotAtomic[(size_t) k].load (std::memory_order_relaxed); }
    bool isNoteHeld (EnvKind k) const noexcept { return noteHeldAtomic[(size_t) k].load (std::memory_order_relaxed); }
    float getBarPhase() const noexcept { return barPhaseAtomic.load (std::memory_order_relaxed); }       // 0..1 within the bar
    int getBeat() const noexcept { return beatAtomic.load (std::memory_order_relaxed); }                  // 0..beatsPerBar-1
    int getBeatsPerBar() const noexcept { return beatsPerBarAtomic.load (std::memory_order_relaxed); }
    juce::int64 getBarCounter() const noexcept { return barCounter.load (std::memory_order_relaxed); }
    double getEffectiveBpm() const noexcept { return effectiveBpm.load (std::memory_order_relaxed); }
    bool isFollowingHost() const noexcept { return followingHost.load (std::memory_order_relaxed); }
    float getDelayBeats() const noexcept { return delayBeatsAtomic.load (std::memory_order_relaxed); }    // smoothed read-head offset
    float getGain() const noexcept { return gainAtomic.load (std::memory_order_relaxed); }                // smoothed volume gain

    /** True once per note-on; the UI clears it. */
    std::atomic<bool> midiActivity { false };
    /** Slot requests for the processor's timer (-1 = none): a MIDI note selected / released a slot
        and the parameter should follow. Consumed with exchange (-1). */
    std::array<std::atomic<int>, kNumKinds> pendingSlot { -1, -1 };

private:
    void activateSlot (int k, int slot, bool forceRetrigger);
    void syncEnvelope (int k);
    void handleMidi (const juce::uint8* data, int numBytes, const EngineParams& params);
    float readAt (int channel, double pos) const noexcept;

    double sr = 44100.0;

    // envelopes
    std::array<EngineEnvelope, kNumKinds> env {};                                   // audio-thread copies of the active slots
    std::array<std::array<EngineEnvelope, kNumSlots>, kNumKinds> shared {};         // message-thread bank copy, guarded by sharedLock
    juce::SpinLock sharedLock;
    std::atomic<int> sharedVersion { 1 };
    int seenVersion = 0;
    std::array<int, kNumKinds> activeSlot { 0, 0 };
    std::array<bool, kNumKinds> needSync { true, true };
    std::array<bool, kNumKinds> pendingRetrigger { false, false };
    std::array<int, kNumKinds> lastParamSlot { -1, -1 };
    std::array<int, kNumKinds> expectedEcho { -1, -1 };
    std::array<int, kNumKinds> segCache { 0, 0 };

    // MIDI slot state
    std::array<bool, kNumKinds> noteActive { false, false };
    std::array<int, kNumKinds> noteNumber { -1, -1 };
    std::array<int, kNumKinds> slotBeforeNote { 0, 0 };

    // clock
    double beatPos = 0.0;                    // quarter notes since the song start (or since the restart)
    std::array<double, kNumKinds> anchor { 0.0, 0.0 };
    std::atomic<bool> restartRequested { false };

    // ring buffer
    std::array<std::vector<float>, 2> ring;
    int ringLen = 0, ringMask = 0, writePos = 0;
    double curDelay = 0.0, lastTarget = 0.0;   // samples
    double xfDelay = 0.0; float xfPos = 0.0f, xfInc = 1.0f; double jumpThreshold = 48.0;

    // volume
    float gain = 1.0f;
    juce::SmoothedValue<float> onMix;

    // readouts
    std::array<std::atomic<float>, kNumKinds> phaseAtomic { 0.0f, 0.0f };
    std::array<std::atomic<float>, kNumKinds> valueAtomic { 0.0f, 1.0f };
    std::array<std::atomic<int>, kNumKinds> activeSlotAtomic { 0, 0 };
    std::array<std::atomic<bool>, kNumKinds> noteHeldAtomic { false, false };
    std::atomic<float> barPhaseAtomic { 0.0f };
    std::atomic<int> beatAtomic { 0 };
    std::atomic<int> beatsPerBarAtomic { 4 };
    std::atomic<juce::int64> barCounter { 0 };
    std::atomic<double> effectiveBpm { 120.0 };
    std::atomic<bool> followingHost { false };
    std::atomic<float> delayBeatsAtomic { 0.0f };
    std::atomic<float> gainAtomic { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DychkaEngine)
};

} // namespace dychka
