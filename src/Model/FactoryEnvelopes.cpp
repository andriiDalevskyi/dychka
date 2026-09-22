// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "FactoryEnvelopes.h"

namespace dychka
{

namespace
{
    // y of a TIME envelope for `beats` of delay
    constexpr float d (float beats) noexcept { return beats / kMaxDelayBeats; }

    Envelope make (const char* name, const char* info, int lengthBeats, std::initializer_list<Point> pts)
    {
        Envelope e;
        e.name = name;
        e.info = info;
        e.lengthBeats = lengthBeats;
        e.points.assign (pts.begin(), pts.end());
        e.clampAll();
        return e;
    }

    /** TIME stutter: the first `sliceBeats` of every `lengthBeats` is repeated until the length is
        full - a staircase of hold steps, each one `sliceBeats` further back. */
    Envelope stutter (const char* name, const char* info, int lengthBeats, float sliceBeats)
    {
        Envelope e;
        e.name = name;
        e.info = info;
        e.lengthBeats = lengthBeats;
        const int n = (int) std::round ((float) lengthBeats / sliceBeats);
        for (int k = 0; k < n; ++k)
            e.points.push_back ({ (float) k * sliceBeats / (float) lengthBeats, d ((float) k * sliceBeats), 0.0f, true });
        e.clampAll();
        return e;
    }

    /** VOLUME step pattern: one character per step, '0'..'9' = 0..90 %, 'x' or '1' = 100 %,
        spaces ignored. Consecutive equal steps are merged into one hold segment. */
    Envelope steps (const char* name, const char* info, int lengthBeats, const char* pattern)
    {
        Envelope e;
        e.name = name;
        e.info = info;
        e.lengthBeats = lengthBeats;
        juce::String s = juce::String (pattern).removeCharacters (" ");
        const int n = s.length();
        float last = -1.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto c = s[i];
            float v = 1.0f;
            if (c == 'x' || c == 'X' || c == '1') v = 1.0f;
            else if (c >= '0' && c <= '9') v = (float) (c - '0') * 0.1f;
            if (juce::approximatelyEqual (v, last)) continue;
            e.points.push_back ({ (float) i / (float) n, v, 0.0f, true });
            last = v;
        }
        e.clampAll();
        return e;
    }

    Bank build()
    {
        Bank b;
        b.name = "Factory";
        for (int k = 0; k < kNumKinds; ++k)
            for (auto& s : b.slots[(size_t) k]) s = Envelope::flat ((EnvKind) k);

        //==========================================================================
        // TIME (y = how far back the read head sits; slope 1 = frozen, slope 2 = reverse)
        auto& T = b.of (EnvKind::time);
        int i = 0;
        T[(size_t) i++] = make ("Off", "No time change: the read head plays the live input. Select it to switch the time effect off.", 8,
                                 { { 0.0f, 0.0f, 0.0f, false } });
        T[(size_t) i++] = make ("Half-time 2 bars", "Half speed across two bars: the head falls one bar behind, then the phrase restarts.", 8,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (4.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Half-time 1 bar", "Every bar is played at half speed (its first half, slowed down).", 4,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (2.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Half-time 1/2 bar", "Half speed restarting every two beats.", 2,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (1.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Half-time 1 beat", "Half speed restarting every beat - a slow-motion shuffle.", 1,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (0.5f), 0.0f, false } });
        T[(size_t) i++] = make ("Quarter-time 1 bar", "Quarter speed: the first beat of the bar stretched over the bar.", 4,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (3.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Reverse 1 bar", "The previous bar played backwards (slope 2: the head moves back twice as fast as time).", 4,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (8.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Reverse 1/2 bar", "Two beats played backwards, restarting every two beats.", 2,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (4.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Reverse 1 beat", "Every beat played backwards.", 1,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (2.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Reverse 1/8", "Every eighth played backwards - a fluttering reverse.", 1,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 0.5f, d (1.0f), 0.0f, false }, { 0.5f, 0.0f, 0.0f, false }, { 1.0f, d (1.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Reverse half-time 1 bar", "Backwards at half speed (slope 1.5).", 4,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, d (6.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Double-time 1 bar", "Starts one bar back and catches up with the live input: the previous bar at double speed.", 4,
                                 { { 0.0f, d (4.0f), 0.0f, false }, { 1.0f, 0.0f, 0.0f, false } });
        T[(size_t) i++] = stutter ("Stutter 1/16", "The first sixteenth of every beat repeated four times.", 1, 0.25f);
        T[(size_t) i++] = stutter ("Stutter 1/8", "The first eighth of every beat repeated twice.", 1, 0.5f);
        T[(size_t) i++] = stutter ("Stutter 1/32", "The first thirty-second of every beat repeated eight times - a buzz roll.", 1, 0.125f);
        T[(size_t) i++] = stutter ("Stutter 1/4", "The first beat of the bar repeated four times.", 4, 1.0f);
        T[(size_t) i++] = stutter ("Stutter 1/2 bar", "The first two beats of the bar repeated.", 4, 2.0f);
        T[(size_t) i++] = make ("Roll 1 bar", "Beats 1-2 live, beat 3 in eighths, beat 4 in sixteenths - a build-up roll.", 4,
                                 { { 0.0f, 0.0f, 0.0f, true },
                                   { 0.5f, 0.0f, 0.0f, true }, { 0.625f, d (0.5f), 0.0f, true },
                                   { 0.75f, 0.0f, 0.0f, true }, { 0.8125f, d (0.25f), 0.0f, true }, { 0.875f, d (0.5f), 0.0f, true }, { 0.9375f, d (0.75f), 0.0f, true } });
        T[(size_t) i++] = make ("Build 1/16 - 1/32", "Sixteenth stutter on beat 1, thirty-second stutter on beat 2.", 2,
                                 { { 0.0f, 0.0f, 0.0f, true }, { 0.125f, d (0.25f), 0.0f, true }, { 0.25f, d (0.5f), 0.0f, true }, { 0.375f, d (0.75f), 0.0f, true },
                                   { 0.5f, 0.0f, 0.0f, true }, { 0.5625f, d (0.125f), 0.0f, true }, { 0.625f, d (0.25f), 0.0f, true }, { 0.6875f, d (0.375f), 0.0f, true },
                                   { 0.75f, d (0.5f), 0.0f, true }, { 0.8125f, d (0.625f), 0.0f, true }, { 0.875f, d (0.75f), 0.0f, true }, { 0.9375f, d (0.875f), 0.0f, true } });
        T[(size_t) i++] = make ("Tape stop 1 bar", "Slows to a stop over the bar (a curve that steepens to slope 1 = frozen).", 4,
                                 { { 0.0f, 0.0f, 0.3f, false }, { 1.0f, d (2.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Tape stop 2 bars", "A long tape stop across two bars.", 8,
                                 { { 0.0f, 0.0f, 0.3f, false }, { 1.0f, d (4.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Scratch 1/8", "Back and forth every eighth: a vinyl scratch.", 2,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 0.25f, d (1.0f), 0.0f, false }, { 0.5f, 0.0f, 0.0f, false }, { 0.75f, d (1.0f), 0.0f, false }, { 1.0f, 0.0f, 0.0f, false } });
        T[(size_t) i++] = make ("Freeze offbeat", "Live on the beat, frozen (slope 1) on the offbeat eighth.", 1,
                                 { { 0.0f, 0.0f, 0.0f, true }, { 0.5f, 0.0f, 0.0f, false }, { 1.0f, d (0.5f), 0.0f, false } });
        T[(size_t) i++] = make ("Delay 1/8", "Everything one eighth late - a rhythmic offset, no speed change.", 1,
                                 { { 0.0f, d (0.5f), 0.0f, false } });
        T[(size_t) i++] = make ("Delay 1 beat", "Everything one beat late.", 1,
                                 { { 0.0f, d (1.0f), 0.0f, false } });
        T[(size_t) i++] = make ("Delay 1 bar", "Everything one bar late (the previous bar).", 4,
                                 { { 0.0f, d (4.0f), 0.0f, false } });
        jassert (i <= kNumSlots);

        //==========================================================================
        // VOLUME (y = gain)
        auto& V = b.of (EnvKind::volume);
        i = 0;
        V[(size_t) i++] = make ("Off", "Full level all the time. Select it to switch the volume effect off.", 8,
                                 { { 0.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = steps ("Gate 1/8", "On for the first eighth of every beat, silent for the second.", 1, "10");
        V[(size_t) i++] = steps ("Gate 1/16", "Sixteenth gate: on-off-on-off every beat.", 1, "1010");
        V[(size_t) i++] = steps ("Gate 1/8 offbeat", "Silent on the beat, on for the offbeat eighth.", 1, "01");
        V[(size_t) i++] = steps ("Gate 1/4", "One beat on, one beat off.", 2, "10");
        V[(size_t) i++] = steps ("Gate triplet", "Eighth-note triplets: on-off-on every beat.", 1, "101");
        V[(size_t) i++] = steps ("Gate 1/32", "Thirty-second gate - a fast trill.", 1, "10101010");
        V[(size_t) i++] = make ("Pump 1/4", "Side-chain pump every beat: ducked on the beat, swelling back up.", 1,
                                 { { 0.0f, 0.0f, -0.5f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Pump 1/2", "Side-chain pump every two beats.", 2,
                                 { { 0.0f, 0.0f, -0.5f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Pump 1 bar", "A slow pump once per bar.", 4,
                                 { { 0.0f, 0.0f, -0.5f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Pump 1/8", "Two pumps per beat.", 1,
                                 { { 0.0f, 0.1f, -0.5f, false }, { 0.5f, 1.0f, 0.0f, false }, { 0.5f, 0.1f, -0.5f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Duck beat 1", "Ducks the first beat of every bar (kick space), full level for the rest.", 4,
                                 { { 0.0f, 0.2f, -0.5f, false }, { 0.25f, 1.0f, 0.0f, true } });
        V[(size_t) i++] = make ("Fade in 2 bars", "Linear fade-in across two bars.", 8,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Fade out 2 bars", "Linear fade-out across two bars.", 8,
                                 { { 0.0f, 1.0f, 0.0f, false }, { 1.0f, 0.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Swell 1 bar", "Up over two beats, down over two beats.", 4,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 0.5f, 1.0f, 0.0f, false }, { 1.0f, 0.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Ramp 1 beat", "Reverse envelope: every beat swells from silence to full.", 1,
                                 { { 0.0f, 0.0f, 0.0f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Pluck 1 beat", "Every beat starts at full level and decays quickly.", 1,
                                 { { 0.0f, 1.0f, -0.5f, false }, { 1.0f, 0.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Tremolo 1/8", "Smooth tremolo, two cycles per beat.", 1,
                                 { { 0.0f, 1.0f, 0.0f, false }, { 0.25f, 0.3f, 0.0f, false }, { 0.5f, 1.0f, 0.0f, false }, { 0.75f, 0.3f, 0.0f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("Tremolo 1/16", "Smooth tremolo, four cycles per beat.", 1,
                                 { { 0.0f, 1.0f, 0.0f, false }, { 0.125f, 0.3f, 0.0f, false }, { 0.25f, 1.0f, 0.0f, false }, { 0.375f, 0.3f, 0.0f, false },
                                   { 0.5f, 1.0f, 0.0f, false }, { 0.625f, 0.3f, 0.0f, false }, { 0.75f, 1.0f, 0.0f, false }, { 0.875f, 0.3f, 0.0f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = make ("LFO 1 bar", "A smooth dip in the middle of the bar.", 4,
                                 { { 0.0f, 1.0f, 0.3f, false }, { 0.5f, 0.0f, -0.3f, false }, { 1.0f, 1.0f, 0.0f, false } });
        V[(size_t) i++] = steps ("Chop 3-3-2", "The 3-3-2 groove in sixteenths over two beats.", 2, "1101 1011");
        V[(size_t) i++] = steps ("Trance gate 1 bar", "A sixteenth-note gate pattern over one bar.", 4, "1011 0110 1101 0100");
        V[(size_t) i++] = steps ("Half-bar duck", "Full for two beats, -10 dB for two beats.", 4, "13");
        V[(size_t) i++] = steps ("Stutter fade", "Sixteenth chops that die away over the beat.", 1, "1070 4020");
        V[(size_t) i++] = steps ("Random gate 2 bars", "A fixed pseudo-random sixteenth gate over two bars.", 8, "1101 0010 1110 0101 1000 1101 0011 0110");
        jassert (i <= kNumSlots);

        return b;
    }
}

const Bank& factoryBank()
{
    static const Bank bank = build();
    return bank;
}

const Envelope& factoryEnvelope (EnvKind kind, int slot)
{
    return factoryBank().of (kind)[(size_t) juce::jlimit (0, kNumSlots - 1, slot)];
}

int numFactoryShapes (EnvKind kind)
{
    int n = 0;
    const auto& slots = factoryBank().of (kind);
    for (int i = 1; i < kNumSlots; ++i)
        if (slots[(size_t) i].name != "Init") ++n;
    return n;
}

} // namespace dychka
