// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "DychkaEngine.h"

namespace dychka
{

namespace
{
    constexpr double kSlowestBpm = 40.0;          // the ring buffer holds two bars at this tempo
    constexpr float kMaxSmoothingMs = 60.0f;      // SMOOTH 100 = 60 ms one-pole glide
    constexpr double kCrossfadeSeconds = 0.002;   // declick when the read head jumps (SMOOTH 0)
    constexpr double kJumpSeconds = 0.001;        // a target change above this within one sample is a jump

    inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }

    inline float onePoleCoef (float ms, double sr) noexcept
    {
        if (ms < 0.05f) return 1.0f;
        return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr));
    }
}

DychkaEngine::DychkaEngine()
{
    for (int k = 0; k < kNumKinds; ++k)
    {
        env[(size_t) k].set (Envelope::flat ((EnvKind) k));
        for (auto& s : shared[(size_t) k]) s = env[(size_t) k];
    }
}

void DychkaEngine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double seconds = kMaxDelayBeats * 60.0 / kSlowestBpm + 0.5;
    ringLen = juce::nextPowerOfTwo ((int) std::ceil (seconds * sr));
    ringMask = ringLen - 1;
    for (auto& r : ring) r.assign ((size_t) ringLen, 0.0f);
    xfInc = 1.0f / (float) juce::jmax (8, (int) (kCrossfadeSeconds * sr));
    jumpThreshold = kJumpSeconds * sr;
    onMix.reset (sr, 0.006);
    reset();
}

void DychkaEngine::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    writePos = 0;
    curDelay = lastTarget = 0.0;
    xfDelay = 0.0; xfPos = 0.0f;
    gain = 1.0f;
    beatPos = 0.0;
    anchor = { 0.0, 0.0 };
    segCache = { 0, 0 };
    onMix.setCurrentAndTargetValue (1.0f);
    for (int k = 0; k < kNumKinds; ++k)
    {
        noteActive[(size_t) k] = false;
        noteNumber[(size_t) k] = -1;
        noteHeldAtomic[(size_t) k].store (false, std::memory_order_relaxed);
    }
}

//==============================================================================
void DychkaEngine::publishEnvelope (EnvKind kind, int slot, const Envelope& e)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return;
    const juce::SpinLock::ScopedLockType lock (sharedLock);
    shared[(size_t) kind][(size_t) slot].set (e);
    sharedVersion.fetch_add (1, std::memory_order_release);
}

void DychkaEngine::publishBank (const Bank& bank)
{
    const juce::SpinLock::ScopedLockType lock (sharedLock);
    for (int k = 0; k < kNumKinds; ++k)
        for (int i = 0; i < kNumSlots; ++i)
            shared[(size_t) k][(size_t) i].set (bank.slots[(size_t) k][(size_t) i]);
    sharedVersion.fetch_add (1, std::memory_order_release);
}

void DychkaEngine::activateSlot (int k, int slot, bool forceRetrigger)
{
    slot = juce::jlimit (0, kNumSlots - 1, slot);
    const bool changed = slot != activeSlot[(size_t) k];
    activeSlot[(size_t) k] = slot;
    activeSlotAtomic[(size_t) k].store (slot, std::memory_order_relaxed);
    if (changed || forceRetrigger)
    {
        needSync[(size_t) k] = true;
        pendingRetrigger[(size_t) k] = true;
    }
    syncEnvelope (k);
}

void DychkaEngine::syncEnvelope (int k)
{
    const int version = sharedVersion.load (std::memory_order_acquire);
    if (! needSync[(size_t) k] && version == seenVersion) return;

    const juce::SpinLock::ScopedTryLockType lock (sharedLock);
    if (! lock.isLocked()) return; // the message thread is publishing: try again next block

    // a version bump re-copies both kinds (an edit may concern either)
    for (int j = 0; j < kNumKinds; ++j)
        if (j == k || version != seenVersion)
            env[(size_t) j] = shared[(size_t) j][(size_t) activeSlot[(size_t) j]];
    seenVersion = version;
    needSync[(size_t) k] = false;
    segCache[(size_t) k] = 0;

    if (pendingRetrigger[(size_t) k])
    {
        pendingRetrigger[(size_t) k] = false;
        if (env[(size_t) k].retrigger) anchor[(size_t) k] = beatPos;
    }
}

//==============================================================================
void DychkaEngine::handleMidi (const juce::uint8* data, int numBytes, const EngineParams& params)
{
    if (numBytes < 2) return;
    const int status = data[0] & 0xF0;
    const int note = data[1];
    const bool noteOn = status == 0x90 && numBytes >= 3 && data[2] > 0;
    const bool noteOff = status == 0x80 || (status == 0x90 && numBytes >= 3 && data[2] == 0);

    if (noteOn)
    {
        midiActivity.store (true, std::memory_order_release);
        for (int k = 0; k < kNumKinds; ++k)
        {
            if (! params.midiSlots[(size_t) k]) continue;
            const int s = note - params.baseNote[(size_t) k];
            if (! juce::isPositiveAndBelow (s, kNumSlots)) continue;
            if (! noteActive[(size_t) k]) slotBeforeNote[(size_t) k] = activeSlot[(size_t) k];
            noteActive[(size_t) k] = true;
            noteNumber[(size_t) k] = note;
            noteHeldAtomic[(size_t) k].store (true, std::memory_order_relaxed);
            activateSlot (k, s, true);
            expectedEcho[(size_t) k] = s;
            pendingSlot[(size_t) k].store (s, std::memory_order_release);
        }
    }
    else if (noteOff)
    {
        for (int k = 0; k < kNumKinds; ++k)
        {
            if (! noteActive[(size_t) k] || noteNumber[(size_t) k] != note) continue;
            noteActive[(size_t) k] = false;
            noteNumber[(size_t) k] = -1;
            noteHeldAtomic[(size_t) k].store (false, std::memory_order_relaxed);
            if (env[(size_t) k].hold) continue; // latched: the slot stays until the next note or click
            const int back = slotBeforeNote[(size_t) k];
            activateSlot (k, back, false);
            expectedEcho[(size_t) k] = back;
            pendingSlot[(size_t) k].store (back, std::memory_order_release);
        }
    }
}

float DychkaEngine::readAt (int channel, double pos) const noexcept
{
    const double fl = std::floor (pos);
    const int i0 = (int) fl;
    const float t = (float) (pos - fl);
    const float* r = ring[(size_t) channel].data();
    const unsigned m = (unsigned) ringMask;
    const float xm1 = r[(unsigned) (i0 - 1) & m];
    const float x0  = r[(unsigned) i0 & m];
    const float x1  = r[(unsigned) (i0 + 1) & m];
    const float x2  = r[(unsigned) (i0 + 2) & m];
    return hermite (xm1, x0, x1, x2, t);
}

//==============================================================================
void DychkaEngine::processBlock (juce::AudioBuffer<float>& buffer, int numInputChannels, const juce::MidiBuffer& midi,
                                 const EngineParams& params, const HostPosition& host)
{
    const int numSamples = buffer.getNumSamples();
    const int numOut = juce::jmin (2, buffer.getNumChannels());
    if (numSamples <= 0 || numOut <= 0 || ringLen == 0) return;

    // --- Slot parameters (edge-triggered; an echo of our own MIDI request is ignored).
    for (int k = 0; k < kNumKinds; ++k)
    {
        const int p = juce::jlimit (0, kNumSlots - 1, params.slot[(size_t) k]);
        if (p != lastParamSlot[(size_t) k])
        {
            lastParamSlot[(size_t) k] = p;
            if (p == expectedEcho[(size_t) k]) { expectedEcho[(size_t) k] = -1; }
            else
            {
                noteActive[(size_t) k] = false;
                noteNumber[(size_t) k] = -1;
                noteHeldAtomic[(size_t) k].store (false, std::memory_order_relaxed);
                expectedEcho[(size_t) k] = -1;
                activateSlot (k, p, false);
            }
        }
        else if (p == expectedEcho[(size_t) k])
            expectedEcho[(size_t) k] = -1;
        syncEnvelope (k);
    }

    // --- Clock for this block.
    const bool followHost = params.sync == SyncMode::host && host.valid && host.playing && host.bpm > 0.0;
    const double bpm = followHost ? host.bpm
                     : (params.sync == SyncMode::host && host.valid && host.bpm > 0.0 ? host.bpm : (double) params.tempoBpm);
    const int beatsPerBar = followHost ? juce::jlimit (1, 32, host.tsNumerator) : 4;
    const double quartersPerBar = followHost ? juce::jmax (1.0, host.tsNumerator * 4.0 / juce::jmax (1, host.tsDenominator)) : 4.0;
    const double beatInc = bpm / 60.0 / sr;
    const double samplesPerBeat = 60.0 / bpm * sr;
    effectiveBpm.store (bpm, std::memory_order_relaxed);
    followingHost.store (followHost, std::memory_order_relaxed);
    beatsPerBarAtomic.store (beatsPerBar, std::memory_order_relaxed);

    if (followHost) beatPos = host.ppq;

    if (restartRequested.exchange (false))
    {
        if (! followHost) beatPos = 0.0;
        anchor = { beatPos, beatPos };
        segCache = { 0, 0 };
    }

    const float smoothMs = juce::jlimit (0.0f, 1.0f, params.smoothing) * kMaxSmoothingMs;
    const float smoothCoef = onePoleCoef (smoothMs, sr);
    const bool instantJumps = smoothCoef >= 1.0f;
    const float aCoef = onePoleCoef (params.attackMs, sr);
    const float rCoef = onePoleCoef (params.releaseMs, sr);
    const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
    const double maxDelay = (double) (ringLen - 8);
    onMix.setTargetValue (params.on ? 1.0f : 0.0f);

    const int numIn = juce::jmax (0, juce::jmin (numInputChannels, buffer.getNumChannels()));
    const float* in[2] = { numIn > 0 ? buffer.getReadPointer (0) : nullptr, numIn > 1 ? buffer.getReadPointer (1) : (numIn > 0 ? buffer.getReadPointer (0) : nullptr) };
    float* out[2] = { buffer.getWritePointer (0), numOut > 1 ? buffer.getWritePointer (1) : nullptr };

    auto it = midi.begin();
    const auto end = midi.end();

    float phaseNow[kNumKinds] = { 0.0f, 0.0f };
    float valNow[kNumKinds] = { 0.0f, 1.0f };

    for (int i = 0; i < numSamples; ++i)
    {
        while (it != end && (*it).samplePosition <= i)
        {
            const auto meta = *it;
            handleMidi (meta.data, meta.numBytes, params);
            ++it;
        }

        // ---- envelopes
        for (int k = 0; k < kNumKinds; ++k)
        {
            const double local = (beatPos - anchor[(size_t) k]) / (double) env[(size_t) k].lengthBeats;
            double ph = local - std::floor (local);
            if (ph >= 1.0) ph = 0.0;
            phaseNow[k] = (float) ph;
            valNow[k] = env[(size_t) k].valueAt ((float) ph, segCache[(size_t) k]);
        }

        // ---- read head
        double target = (double) valNow[0] * (double) kMaxDelayBeats * samplesPerBeat;
        target = juce::jlimit (0.0, maxDelay, target);
        if (target > 0.0 && target < 2.0) target = 2.0;
        if (instantJumps)
        {
            if (std::abs (target - lastTarget) > jumpThreshold && i + writePos != 0)
            {
                xfDelay = curDelay;   // the old head keeps playing at its offset while it fades out
                xfPos = 1.0f;
            }
            curDelay = target;
        }
        else
            curDelay += (target - curDelay) * smoothCoef;
        lastTarget = target;

        // ---- write the input, read the delayed signal
        const float dryL = in[0] != nullptr ? in[0][i] : 0.0f;
        const float dryR = in[1] != nullptr ? in[1][i] : 0.0f;
        ring[0][(size_t) writePos] = dryL;
        ring[1][(size_t) writePos] = dryR;

        const double readPos = (double) writePos - curDelay;
        float wetL = readAt (0, readPos);
        float wetR = readAt (1, readPos);
        if (xfPos > 0.0f)
        {
            const double oldPos = (double) writePos - xfDelay;
            const float f = xfPos;
            wetL = wetL * (1.0f - f) + readAt (0, oldPos) * f;
            wetR = wetR * (1.0f - f) + readAt (1, oldPos) * f;
            xfPos = juce::jmax (0.0f, xfPos - xfInc);
        }

        // ---- volume
        const float gTarget = valNow[1];
        gain += (gTarget - gain) * (gTarget > gain ? aCoef : rCoef);
        wetL *= gain;
        wetR *= gain;

        // ---- mix / bypass
        const float wetAmt = mix * onMix.getNextValue();
        out[0][i] = dryL + (wetL - dryL) * wetAmt;
        if (out[1] != nullptr) out[1][i] = dryR + (wetR - dryR) * wetAmt;

        // ---- advance
        writePos = (writePos + 1) & ringMask;
        beatPos += beatInc;
    }

    // --- readouts
    for (int k = 0; k < kNumKinds; ++k)
    {
        phaseAtomic[(size_t) k].store (phaseNow[k], std::memory_order_relaxed);
        valueAtomic[(size_t) k].store (valNow[k], std::memory_order_relaxed);
    }
    const double barPos = beatPos / quartersPerBar;
    const double barPhase = barPos - std::floor (barPos);
    barPhaseAtomic.store ((float) barPhase, std::memory_order_relaxed);
    beatAtomic.store (juce::jlimit (0, beatsPerBar - 1, (int) (barPhase * beatsPerBar)), std::memory_order_relaxed);
    barCounter.store ((juce::int64) std::floor (barPos), std::memory_order_relaxed);
    delayBeatsAtomic.store ((float) (curDelay / samplesPerBeat), std::memory_order_relaxed);
    gainAtomic.store (gain, std::memory_order_relaxed);
}

} // namespace dychka
