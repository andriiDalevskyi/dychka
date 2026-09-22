// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

// Offline checks: envelope maths, factory data, JSON / file round trips and the engine on synthetic
// signals (pass-through, delay, half-time, reverse, stutter, gate, mix / bypass, host sync,
// retrigger, MIDI slot selection). Exit code 0 = all passed.

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Model/EnvelopeModel.h"
#include "Model/FactoryEnvelopes.h"
#include "Model/BankStore.h"
#include "Engine/DychkaEngine.h"
#include <cstdio>
#include <cmath>
#include <functional>

using namespace dychka;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); } } while (0)

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    constexpr int kBeat = 24000;   // samples per beat at 120 BPM

    struct RunResult
    {
        juce::AudioBuffer<float> out;
        bool finite = true;
    };

    using InputFn = std::function<float (juce::int64 sampleIndex)>;

    float dc (juce::int64) { return 1.0f; }
    float sine1k (juce::int64 n) { return std::sin ((float) n * juce::MathConstants<float>::twoPi * 1000.0f / (float) kSr); }

    // Runs the engine over `seconds`; `firstSample` is the absolute index of the first sample so
    // several consecutive runs can share one continuous input signal.
    RunResult run (DychkaEngine& eng, const EngineParams& params, HostPosition host, double seconds, const InputFn& input,
                   juce::int64 firstSample = 0, const juce::MidiBuffer* midiAtFirstBlock = nullptr, bool hostAdvances = false)
    {
        const int total = (int) (seconds * kSr);
        RunResult r;
        r.out.setSize (2, total);
        juce::AudioBuffer<float> block (2, kBlock);
        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);
            for (int i = 0; i < n; ++i)
            {
                const float v = input (firstSample + pos + i);
                block.setSample (0, i, v);
                block.setSample (1, i, v);
            }
            juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, n);
            juce::MidiBuffer midi;
            if (midiAtFirstBlock != nullptr && pos == 0) midi = *midiAtFirstBlock;
            eng.processBlock (view, 2, midi, params, host);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float v = view.getSample (c, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.out.setSample (c, pos + i, v);
                }
            if (hostAdvances) host.ppq += n / kSr * host.bpm / 60.0;
        }
        return r;
    }

    float meanAbs (const juce::AudioBuffer<float>& b, int ch, int from, int to)
    {
        double s = 0.0;
        for (int i = from; i < to; ++i) s += std::abs (b.getSample (ch, i));
        return (float) (s / juce::jmax (1, to - from));
    }

    int zeroCrossings (const juce::AudioBuffer<float>& b, int ch, int from, int to)
    {
        int n = 0;
        for (int i = from + 1; i < to; ++i)
            if ((b.getSample (ch, i - 1) < 0.0f) != (b.getSample (ch, i) < 0.0f)) ++n;
        return n;
    }

    int zeroCrossingsInput (const InputFn& f, juce::int64 from, juce::int64 to)
    {
        int n = 0;
        for (juce::int64 i = from + 1; i < to; ++i)
            if ((f (i - 1) < 0.0f) != (f (i) < 0.0f)) ++n;
        return n;
    }

    Envelope ramp (float y0, float y1, float tension = 0.0f, int length = 4)
    {
        Envelope e;
        e.name = "ramp";
        e.lengthBeats = length;
        e.points = { { 0.0f, y0, tension, false }, { 1.0f, y1, 0.0f, false } };
        e.clampAll();
        return e;
    }

    EngineParams internalParams()
    {
        EngineParams p;
        p.sync = SyncMode::internal;
        p.tempoBpm = 120.0f;
        p.attackMs = 0.0f;
        p.releaseMs = 0.0f;
        return p;
    }
}

//==============================================================================
static void testEnvelopeMath()
{
    CHECK (std::abs (curveShape (0.5f, 0.0f) - 0.5f) < 1.0e-6f, "tension 0 is linear");
    CHECK (curveShape (0.5f, 0.7f) < 0.5f && curveShape (0.5f, -0.7f) > 0.5f, "tension bends the curve");
    CHECK (std::abs (curveShape (0.3f, 0.6f) - (1.0f - curveShape (0.7f, -0.6f))) < 1.0e-5f, "tension is point-symmetric");
    CHECK (curveShape (0.0f, 0.9f) == 0.0f && std::abs (curveShape (1.0f, 0.9f) - 1.0f) < 1.0e-6f, "curve endpoints");

    auto lin = ramp (0.0f, 1.0f);
    CHECK (std::abs (lin.valueAt (0.25f) - 0.25f) < 1.0e-5f && std::abs (lin.valueAt (0.5f) - 0.5f) < 1.0e-5f, "linear ramp values");

    Envelope hold;
    hold.points = { { 0.0f, 0.3f, 0.0f, true }, { 0.5f, 0.7f, 0.0f, false } };
    hold.clampAll();
    CHECK (std::abs (hold.valueAt (0.25f) - 0.3f) < 1.0e-6f, "hold keeps the start value");
    CHECK (std::abs (hold.valueAt (0.5f) - 0.7f) < 1.0e-6f && std::abs (hold.valueAt (0.9f) - 0.7f) < 1.0e-6f, "tail holds the last value");

    auto bent = ramp (0.0f, 1.0f, 0.5f);
    CHECK (bent.valueAt (0.5f) < 0.3f, "positive tension stays low at first");
    bool mono = true;
    for (int i = 1; i <= 100; ++i) mono = mono && bent.valueAt (i / 100.0f) >= bent.valueAt ((i - 1) / 100.0f) - 1.0e-6f;
    CHECK (mono, "tension curve is monotonic");

    Envelope jump;
    jump.points = { { 0.0f, 0.0f, 0.0f, false }, { 0.5f, 1.0f, 0.0f, false }, { 0.5f, 0.0f, 0.0f, false }, { 1.0f, 1.0f, 0.0f, false } };
    jump.clampAll();
    CHECK (jump.valueAt (0.49f) > 0.97f && jump.valueAt (0.5f) < 1.0e-6f && std::abs (jump.valueAt (0.75f) - 0.5f) < 1.0e-5f, "two points at the same x make a jump");
    CHECK (jump.segmentAt (0.5f) == 2, "segmentAt picks the later of coincident points");

    // shift a ramp by a quarter: the wrap value moves to the start
    auto shifted = ramp (0.0f, 1.0f);
    shifted.shift (0.25f);
    CHECK (std::abs (shifted.valueAt (0.1f) - 0.85f) < 0.02f, "shift: head continues the wrapped segment");
    CHECK (std::abs (shifted.valueAt (0.5f) - 0.25f) < 0.02f, "shift: body moved later");
    CHECK (shifted.points.front().x == 0.0f, "shift keeps a point at 0");

    auto flipped = ramp (0.0f, 1.0f);
    flipped.flipX();
    CHECK (std::abs (flipped.valueAt (0.25f) - 0.75f) < 0.02f && std::abs (flipped.valueAt (0.75f) - 0.25f) < 0.02f, "flipX mirrors the shape");

    auto inv = ramp (0.2f, 0.9f);
    inv.flipY();
    CHECK (std::abs (inv.points[0].y - 0.8f) < 1.0e-6f && std::abs (inv.points[1].y - 0.1f) < 1.0e-6f, "flipY inverts values");

    Envelope messy;
    messy.points = { { 0.7f, 2.0f, 3.0f, false }, { 0.2f, -1.0f, 0.0f, true } };
    messy.lengthBeats = 5;
    messy.clampAll();
    CHECK (messy.points.size() == 2 && messy.points[0].x == 0.0f && messy.points[0].y == 0.0f && messy.points[1].y == 1.0f && messy.points[1].tension == 1.0f && messy.lengthBeats == kDefaultLengthBeats,
           "clampAll sorts, clamps and anchors the first point");

    CHECK (Envelope::flat (EnvKind::time).isFlat (EnvKind::time) && Envelope::flat (EnvKind::volume).isFlat (EnvKind::volume), "flat envelopes are flat");
    CHECK (! ramp (0.0f, 1.0f).isFlat (EnvKind::time), "a ramp is not flat");
}

static void testFactory()
{
    const auto& bank = factoryBank();
    CHECK (numFactoryShapes (EnvKind::time) == 25, "25 time shapes");
    CHECK (numFactoryShapes (EnvKind::volume) == 24, "24 volume shapes");
    for (int k = 0; k < kNumKinds; ++k)
    {
        const auto kind = (EnvKind) k;
        const auto& slots = bank.of (kind);
        CHECK (slots[0].name == "Off" && slots[0].isFlat (kind), "slot 1 is Off");
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto& e = slots[(size_t) i];
            CHECK (e.name.isNotEmpty() && ! e.points.empty() && e.points.front().x == 0.0f && isValidLength (e.lengthBeats), "factory slot is well formed");
            for (size_t j = 1; j < e.points.size(); ++j)
                CHECK (e.points[j].x >= e.points[j - 1].x && e.points[j].y >= 0.0f && e.points[j].y <= 1.0f, "factory points sorted and in range");
            if (i > 0 && e.name != "Init") CHECK (! e.isFlat (kind) || e.name == "Off", "a factory shape does something");
        }
    }
    const auto& half = factoryEnvelope (EnvKind::time, 1);
    CHECK (half.lengthBeats == 8 && std::abs (half.valueAt (0.999f) - 0.5f) < 0.01f, "Half-time 2 bars ends one bar back");
    const auto& st16 = factoryEnvelope (EnvKind::time, 12);
    CHECK (st16.lengthBeats == 1 && st16.points.size() == 4 && st16.points[1].hold && std::abs (st16.valueAt (0.3f) - 0.25f / 8.0f) < 1.0e-6f, "Stutter 1/16 = four hold steps");
    const auto& gate8 = factoryEnvelope (EnvKind::volume, 1);
    CHECK (gate8.valueAt (0.25f) == 1.0f && gate8.valueAt (0.75f) == 0.0f, "Gate 1/8 = on / off");
    const auto& chop = factoryEnvelope (EnvKind::volume, 20);
    CHECK (chop.name == "Chop 3-3-2" && chop.valueAt (0.05f) == 1.0f && chop.valueAt (0.3f) == 0.0f && chop.valueAt (0.4f) == 1.0f && chop.valueAt (0.9f) == 1.0f, "Chop 3-3-2 pattern");
}

static void testJson()
{
    const auto& bank = factoryBank();
    for (int k = 0; k < kNumKinds; ++k)
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto kind = (EnvKind) k;
            Envelope copy;
            EnvKind fileKind = EnvKind::time;
            CHECK (copy.fromJsonString (bank.of (kind)[(size_t) i].toJsonString (kind), &fileKind), "envelope JSON parses");
            CHECK (copy == bank.of (kind)[(size_t) i] && fileKind == kind, "envelope JSON round trip is lossless");
        }

    Bank edited = bank;
    edited.of (EnvKind::volume)[5].name = "Edited";
    edited.of (EnvKind::volume)[5].points.push_back ({ 0.9f, 0.4f, 0.2f, true });
    std::array<std::array<bool, kNumSlots>, kNumKinds> only {};
    only[1][5] = true;
    const auto json = edited.toJsonString (&only);
    Bank restored = bank;
    CHECK (restored.fromJsonString (json) == 1, "partial bank JSON lists one slot");
    CHECK (restored.of (EnvKind::volume)[5] == edited.of (EnvKind::volume)[5] && restored.of (EnvKind::volume)[6] == bank.of (EnvKind::volume)[6]
           && restored.of (EnvKind::time)[5] == bank.of (EnvKind::time)[5], "partial bank restore touches only that slot");

    Envelope bad;
    CHECK (! bad.fromJsonString ("{ \"format\": \"something-else\" }"), "foreign JSON is rejected");
    Envelope objectPoints;
    CHECK (objectPoints.fromJsonString ("{ \"format\": \"dychka-envelope\", \"kind\": \"volume\", \"name\": \"o\", \"points\": [ { \"x\": 0, \"y\": 0.5 }, { \"x\": 0.5, \"y\": 1, \"t\": 0.3, \"h\": true } ] }")
           && objectPoints.points.size() == 2 && objectPoints.points[1].hold && std::abs (objectPoints.points[1].tension - 0.3f) < 1.0e-6f, "object-style points are accepted");

    // files
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("dychka-test");
    dir.createDirectory();
    auto bankFile = dir.getChildFile ("t" + juce::String (BankStore::kBankExtension));
    auto envFile = dir.getChildFile ("e" + juce::String (BankStore::kEnvelopeExtension));
    CHECK (BankStore::saveBank (edited, bankFile) && BankStore::kindOfFile (bankFile) == "bank", "bank file saved and recognised");
    Bank fromDisk;
    int n = 0;
    CHECK (BankStore::loadBank (bankFile, fromDisk, &n) && n == kNumSlots * kNumKinds && fromDisk.of (EnvKind::volume)[5].name == "Edited", "bank file loads");
    CHECK (BankStore::saveEnvelope (edited.of (EnvKind::volume)[5], EnvKind::volume, envFile) && BankStore::kindOfFile (envFile) == "envelope", "envelope file saved and recognised");
    Envelope fromDiskEnv;
    EnvKind k2 = EnvKind::time;
    CHECK (BankStore::loadEnvelope (envFile, fromDiskEnv, &k2) && k2 == EnvKind::volume && fromDiskEnv == edited.of (EnvKind::volume)[5], "envelope file loads with its kind");
    auto plain = dir.getChildFile ("plain.json");
    plain.replaceWithText (edited.of (EnvKind::time)[2].toJsonString (EnvKind::time));
    CHECK (BankStore::kindOfFile (plain) == "envelope", "plain .json is sniffed by its format field");
    CHECK (BankStore::kindOfFile (dir.getChildFile ("x.txt")).isEmpty(), "unknown files are ignored");
    dir.deleteRecursively();
}

//==============================================================================
static void testEnginePassThrough()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    HostPosition host;
    auto r = run (eng, p, host, 1.0, sine1k);
    CHECK (r.finite, "pass-through output finite");
    bool same = true;
    for (int i = 100; i < 48000; ++i) same = same && std::abs (r.out.getSample (0, i) - sine1k (i)) < 1.0e-4f;
    CHECK (same, "Off / Off = exact pass-through");
    CHECK (std::abs (eng.getGain() - 1.0f) < 1.0e-6f && eng.getDelayBeats() == 0.0f, "readouts: no delay, full gain");
}

static void testEngineDelay()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    p.slot[0] = 24; // Delay 1 beat
    HostPosition host;
    auto r = run (eng, p, host, 1.5, sine1k);
    CHECK (r.finite, "delay output finite");
    bool delayed = true;
    for (int i = kBeat + 500; i < 72000; ++i) delayed = delayed && std::abs (r.out.getSample (0, i) - sine1k (i - kBeat)) < 1.0e-3f;
    CHECK (delayed, "Delay 1 beat = the input one beat ago");
    CHECK (std::abs (eng.getDelayBeats() - 1.0f) < 1.0e-3f, "readout: one beat of delay");
    CHECK (meanAbs (r.out, 0, 200, kBeat - 200) < 1.0e-3f, "before the first beat the buffer is silent");
}

static void testEngineHalfTime()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    p.slot[0] = 2; // Half-time 1 bar (length 4 -> 2 s at 120 BPM)
    HostPosition host;
    auto r = run (eng, p, host, 2.0, sine1k);
    CHECK (r.finite, "half-time output finite");
    const int zcOut = zeroCrossings (r.out, 0, 10000, 86000);
    const int zcIn = zeroCrossingsInput (sine1k, 10000, 86000);
    CHECK (std::abs ((double) zcOut / (double) zcIn - 0.5) < 0.03, "half-time halves the frequency");
    CHECK (std::abs (eng.getDelayBeats() - 2.0f) < 0.02f, "at the end of the bar the head is two beats behind");
}

static void testEngineReverse()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    p.slot[0] = 6; // Reverse 1 bar
    HostPosition host;
    auto rampIn = [] (juce::int64 n) { return (float) n * 1.0e-6f; };
    auto r = run (eng, p, host, 4.0, rampIn);   // two bars: the second bar plays the first one backwards
    CHECK (r.finite, "reverse output finite");
    bool decreasing = true, positive = true;
    const int start = 4 * kBeat + 2000, stop = 8 * kBeat - 2000;
    for (int i = start; i + 200 < stop; i += 200)
    {
        decreasing = decreasing && r.out.getSample (0, i + 200) < r.out.getSample (0, i);
        positive = positive && r.out.getSample (0, i) > 0.0f;
    }
    CHECK (decreasing && positive, "reverse plays the previous bar backwards");
    // speed -1: the read position moves back one sample per sample -> the ramp falls at the input's rate
    const float slope = (r.out.getSample (0, start + 20000) - r.out.getSample (0, start)) / 20000.0f;
    CHECK (std::abs (slope + 1.0e-6f) < 1.0e-8f, "reverse speed is exactly -1x");
}

static void testEngineStutter()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    p.slot[0] = 12; // Stutter 1/16
    HostPosition host;
    auto impulses = [] (juce::int64 n) { return (n % kBeat) == 100 ? 1.0f : 0.0f; };
    auto r = run (eng, p, host, 1.0, impulses);
    CHECK (r.finite, "stutter output finite");
    const int sixteenth = kBeat / 4;
    bool repeats = true, silentBetween = true;
    for (int k = 0; k < 4; ++k)
    {
        repeats = repeats && std::abs (r.out.getSample (0, 100 + k * sixteenth)) > 0.9f;
        silentBetween = silentBetween && meanAbs (r.out, 0, 300 + k * sixteenth, (k + 1) * sixteenth - 50) < 1.0e-4f;
    }
    CHECK (repeats, "the first sixteenth is repeated four times");
    CHECK (silentBetween, "nothing else is heard");
    CHECK (std::abs (r.out.getSample (0, 100 + kBeat)) > 0.9f, "the next beat starts live again");
}

static void testEngineGateMixBypass()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    auto p = internalParams();
    p.slot[1] = 1; // Gate 1/8
    HostPosition host;
    auto r = run (eng, p, host, 1.0, dc);
    CHECK (r.finite, "gate output finite");
    CHECK (meanAbs (r.out, 0, 100, kBeat / 2 - 100) > 0.99f, "gate open in the first half of the beat");
    CHECK (meanAbs (r.out, 0, kBeat / 2 + 100, kBeat - 100) < 1.0e-3f, "gate closed in the second half");

    p.attackMs = 20.0f;
    eng.reset();
    r = run (eng, p, host, 1.0, dc);
    CHECK (r.out.getSample (0, kBeat + 50) < 0.6f && r.out.getSample (0, kBeat + 4000) > 0.95f, "attack softens the gate edge");

    p.attackMs = 0.0f;
    p.mix = 0.5f;
    eng.reset();
    r = run (eng, p, host, 1.0, dc);
    CHECK (std::abs (meanAbs (r.out, 0, kBeat / 2 + 100, kBeat - 100) - 0.5f) < 1.0e-3f, "MIX 50 = half dry in the gaps");

    p.mix = 0.0f;
    eng.reset();
    r = run (eng, p, host, 0.5, dc);
    CHECK (std::abs (r.out.getSample (0, kBeat / 2 + 500) - 1.0f) < 1.0e-5f, "MIX 0 = dry");

    p.mix = 1.0f;
    p.on = false;
    eng.reset();
    r = run (eng, p, host, 0.5, dc);
    CHECK (std::abs (r.out.getSample (0, kBeat / 2 + 500) - 1.0f) < 1.0e-5f, "effect off = bypass");
}

static void testEngineHostSync()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    EngineParams p;
    p.attackMs = p.releaseMs = 0.0f;
    p.slot[1] = 4; // Gate 1/4 (length 2: beat on, beat off)
    HostPosition host;
    host.valid = true;
    host.playing = true;
    host.bpm = 100.0;
    host.ppq = 4.0 + 1.5;   // bar 2, beat 2.5 -> length 2 -> phase 0.75 -> gate closed
    host.barStartPpq = 4.0;
    auto r = run (eng, p, host, 0.02, dc, 0, nullptr, true);
    CHECK (eng.isFollowingHost() && std::abs (eng.getEffectiveBpm() - 100.0) < 1.0e-6, "engine follows the host");
    CHECK (std::abs (eng.getPhase (EnvKind::volume) - 0.76f) < 0.02f, "host position maps to the envelope phase");
    CHECK (meanAbs (r.out, 0, 100, 900) < 1.0e-3f, "the gate is closed at that position");
    CHECK (eng.getBarCounter() == 1 && eng.getBeat() == 1, "bar / beat readouts from the host position");

    // stopped transport: free-running at the host tempo, still gating
    host.playing = false;
    host.ppq = 0.0;
    eng.reset();
    r = run (eng, p, host, 1.5, dc);   // two beats at 100 BPM = 1.2 s
    CHECK (! eng.isFollowingHost() && std::abs (eng.getEffectiveBpm() - 100.0) < 1.0e-6, "stopped host: free run at host tempo");
    const int beat100 = (int) (kSr * 0.6);
    const float openMean = meanAbs (r.out, 0, 100, beat100 - 100), closedMean = meanAbs (r.out, 0, beat100 + 100, 2 * beat100 - 100);
    CHECK (openMean > 0.99f && closedMean < 1.0e-3f, (juce::String ("gating continues while stopped (open ") + juce::String (openMean, 4) + ", closed " + juce::String (closedMean, 4)
                                                     + ", phase " + juce::String (eng.getPhase (EnvKind::volume), 3) + ", bpm " + juce::String (eng.getEffectiveBpm(), 1) + ")").toRawUTF8());
}

static void testEngineRetrigger()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    Bank bank = factoryBank();
    Envelope gate = factoryEnvelope (EnvKind::volume, 4); // Gate 1/4, length 2
    gate.name = "retrig gate";
    gate.retrigger = true;
    bank.of (EnvKind::volume)[30] = gate;
    eng.publishBank (bank);
    auto p = internalParams();
    HostPosition host;
    run (eng, p, host, 0.7, dc);             // 1.4 beats in
    p.slot[1] = 4;                           // plain Gate 1/4 follows the grid: phase = 1.4 / 2 = 0.7
    run (eng, p, host, 0.001, dc);
    CHECK (std::abs (eng.getPhase (EnvKind::volume) - 0.7f) < 0.01f, "without RETRIG the envelope follows the song grid");
    p.slot[1] = 30;                          // RETRIG: restarts at the switch
    run (eng, p, host, 0.001, dc);
    CHECK (eng.getPhase (EnvKind::volume) < 0.01f, "RETRIG restarts the envelope when the slot is selected");
    CHECK (eng.getActiveSlot (EnvKind::volume) == 30, "active slot readout");

    // CUE restarts everything
    p.slot[1] = 4;
    run (eng, p, host, 0.3, dc);
    eng.requestRestart();
    run (eng, p, host, 0.001, dc);
    CHECK (eng.getPhase (EnvKind::volume) < 0.01f, "restart request cues the envelopes");
}

static void testEngineMidi()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    Bank bank = factoryBank();
    bank.of (EnvKind::time)[3].hold = true;   // a latching slot
    eng.publishBank (bank);
    auto p = internalParams();
    p.baseNote[0] = 60;
    p.baseNote[1] = 24;
    HostPosition host;

    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);   // time slot 3 (index 2)
    on.addEvent (juce::MidiMessage::noteOn (1, 25, (juce::uint8) 100), 0);   // volume slot 2 (index 1)
    run (eng, p, host, 0.05, dc, 0, &on);
    CHECK (eng.midiActivity.load(), "note-on lights the MIDI LED");
    CHECK (eng.getActiveSlot (EnvKind::time) == 2 && eng.getActiveSlot (EnvKind::volume) == 1, "notes select slots sample-accurately");
    CHECK (eng.pendingSlot[0].load() == 2 && eng.pendingSlot[1].load() == 1, "slot requests posted for the processor");
    CHECK (eng.isNoteHeld (EnvKind::time) && eng.isNoteHeld (EnvKind::volume), "notes are held");

    // the processor echoes the request into the parameter: nothing changes
    eng.pendingSlot[0].exchange (-1); eng.pendingSlot[1].exchange (-1);
    p.slot[0] = 2; p.slot[1] = 1;
    run (eng, p, host, 0.02, dc);
    CHECK (eng.getActiveSlot (EnvKind::time) == 2 && eng.isNoteHeld (EnvKind::time), "parameter echo keeps the note state");

    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
    off.addEvent (juce::MidiMessage::noteOff (1, 25), 0);
    run (eng, p, host, 0.02, dc, 0, &off);
    CHECK (eng.getActiveSlot (EnvKind::time) == 0 && eng.getActiveSlot (EnvKind::volume) == 0, "note-off returns to the previous slots (momentary)");
    CHECK (eng.pendingSlot[0].load() == 0 && eng.pendingSlot[1].load() == 0, "return requests posted");
    eng.pendingSlot[0].exchange (-1); eng.pendingSlot[1].exchange (-1);
    p.slot[0] = 0; p.slot[1] = 0;
    run (eng, p, host, 0.02, dc);
    CHECK (eng.getActiveSlot (EnvKind::time) == 0, "echo of the return is ignored");

    // a HOLD slot latches
    juce::MidiBuffer holdOn;
    holdOn.addEvent (juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100), 0);   // time slot 4 (index 3, hold)
    holdOn.addEvent (juce::MidiMessage::noteOff (1, 63), 100);
    run (eng, p, host, 0.02, dc, 0, &holdOn);
    CHECK (eng.getActiveSlot (EnvKind::time) == 3 && ! eng.isNoteHeld (EnvKind::time), "HOLD slot stays after note-off");
    eng.pendingSlot[0].exchange (-1);
    p.slot[0] = 3;
    run (eng, p, host, 0.02, dc);
    // a user click on another slot ends the latch
    p.slot[0] = 5;
    run (eng, p, host, 0.02, dc);
    CHECK (eng.getActiveSlot (EnvKind::time) == 5, "a parameter change overrides the MIDI selection");

    // notes outside the ranges / disabled mapping do nothing
    p.midiSlots[0] = false;
    juce::MidiBuffer ignored;
    ignored.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100), 0);
    ignored.addEvent (juce::MidiMessage::noteOn (1, 100, (juce::uint8) 100), 0);
    run (eng, p, host, 0.02, dc, 0, &ignored);
    CHECK (eng.getActiveSlot (EnvKind::time) == 5 && eng.getActiveSlot (EnvKind::volume) == 0, "disabled mapping and out-of-range notes are ignored");
}

static void testAllFactoryFinite()
{
    DychkaEngine eng;
    eng.prepare (kSr, kBlock);
    eng.publishBank (factoryBank());
    HostPosition host;
    for (int smooth = 0; smooth <= 1; ++smooth)
        for (int t = 0; t < kNumSlots; ++t)
        {
            auto p = internalParams();
            p.slot[0] = t;
            p.slot[1] = t % kNumSlots;
            p.smoothing = (float) smooth;
            p.attackMs = 5.0f;
            p.releaseMs = 5.0f;
            eng.reset();
            auto r = run (eng, p, host, 0.4, sine1k);
            CHECK (r.finite, (juce::String ("finite output for time slot ") + juce::String (t + 1) + " smoothing " + juce::String (smooth)).toRawUTF8());
            if (t == 0) CHECK (meanAbs (r.out, 0, 1000, 19000) > 0.1f, "sound comes through");
        }
}

//==============================================================================
int main()
{
    std::printf ("Dychka engine test\n");
    testEnvelopeMath();
    testFactory();
    testJson();
    testEnginePassThrough();
    testEngineDelay();
    testEngineHalfTime();
    testEngineReverse();
    testEngineStutter();
    testEngineGateMixBypass();
    testEngineHostSync();
    testEngineRetrigger();
    testEngineMidi();
    testAllFactoryFinite();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
