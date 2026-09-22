// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

/** SHARED CONTRACT between the model, the engine, the UI and the file formats. See ARCHITECTURE.md.

    Dychka has two banks of 36 envelope slots: TIME envelopes (how far back in the 2-bar audio
    buffer the read head sits, i.e. re-arranging / slowing / reversing / stuttering time) and
    VOLUME envelopes (a gain curve). An envelope spans `lengthBeats` quarter notes and loops with
    the song position. It is a list of points sorted by x; every point owns the segment that
    starts at it (its tension curve, or a hold that keeps the point's value until the next point).
    After the last point the value is held until the loop wraps. */
namespace dychka
{

constexpr int kNumSlots       = 36;    // per kind (parameters "timeSlot" / "volSlot" 1..36)
constexpr int kMaxPoints      = 512;
constexpr float kMaxDelayBeats = 8.0f; // TIME envelope: y = 1 -> 2 bars (8 quarter notes) back
constexpr int kLengthChoices[] = { 1, 2, 4, 8, 16 }; // envelope length in beats
constexpr int kNumLengthChoices = 5;
constexpr int kDefaultLengthBeats = 8;

inline bool isValidLength (int beats) noexcept
{
    for (int c : kLengthChoices) if (c == beats) return true;
    return false;
}

inline juce::String lengthName (int beats)
{
    switch (beats)
    {
        case 1:  return "1 beat";
        case 2:  return "2 beats";
        case 4:  return "1 bar";
        case 8:  return "2 bars";
        case 16: return "4 bars";
    }
    return juce::String (beats) + " beats";
}

enum class EnvKind : int { time = 0, volume = 1 };
constexpr int kNumKinds = 2;

inline const char* kindName (EnvKind k) noexcept { return k == EnvKind::volume ? "volume" : "time"; }
inline EnvKind kindFromName (const juce::String& s) noexcept { return s.trim().equalsIgnoreCase ("volume") ? EnvKind::volume : EnvKind::time; }
inline char kindLetter (EnvKind k) noexcept { return k == EnvKind::volume ? 'V' : 'T'; }
/** The value of a flat "Off" envelope: no delay for TIME, full level for VOLUME. */
inline float flatValue (EnvKind k) noexcept { return k == EnvKind::volume ? 1.0f : 0.0f; }

/** Segment shape: local position t 0..1 -> 0..1. tension 0 = linear; > 0 stays near the start
    value and rushes to the end (power curve), < 0 the mirror image. */
inline float curveShape (float t, float tension) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (std::abs (tension) < 1.0e-4f) return t;
    const float e = std::pow (10.0f, juce::jlimit (0.0f, 1.0f, std::abs (tension)));   // 1 .. 10
    return tension > 0.0f ? std::pow (t, e) : 1.0f - std::pow (1.0f - t, e);
}

struct Point
{
    float x = 0.0f;        // 0..1 of the envelope length
    float y = 0.0f;        // 0..1 (TIME: fraction of kMaxDelayBeats back; VOLUME: gain)
    float tension = 0.0f;  // -1..1 curve of the segment that starts here
    bool hold = false;     // the segment keeps this point's value until the next point (step)

    bool operator== (const Point& o) const noexcept
    {
        return juce::approximatelyEqual (x, o.x) && juce::approximatelyEqual (y, o.y)
            && juce::approximatelyEqual (tension, o.tension) && hold == o.hold;
    }
    bool operator!= (const Point& o) const noexcept { return ! (*this == o); }
};

struct Envelope
{
    juce::String name;             // shown in the slot grid / LCD; "" = never used
    juce::String info;             // free text: what the shape does
    int lengthBeats = kDefaultLengthBeats;
    bool retrigger = false;        // the envelope restarts when this slot becomes active (else it follows the song grid)
    bool hold = false;             // MIDI: the slot stays selected after the note is released (latch)
    std::vector<Point> points;     // sorted by x, points[0].x == 0, 1..kMaxPoints entries

    /** A flat envelope: TIME = no delay, VOLUME = full level. */
    static Envelope flat (EnvKind kind, const juce::String& name = "Init")
    {
        Envelope e;
        e.name = name;
        e.points.push_back ({ 0.0f, flatValue (kind), 0.0f, false });
        return e;
    }

    bool isEmpty() const noexcept { return name.isEmpty(); }
    int numPoints() const noexcept { return (int) points.size(); }

    /** True when every point sits at the kind's flat value (the slot does nothing). */
    bool isFlat (EnvKind kind) const noexcept
    {
        const float v = flatValue (kind);
        for (const auto& p : points) if (std::abs (p.y - v) > 1.0e-4f) return false;
        return true;
    }

    void sortPoints()
    {
        std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
    }

    /** Sorts, clamps every value, forces the first point to x = 0 and keeps 1..kMaxPoints points. */
    void clampAll()
    {
        if (! isValidLength (lengthBeats)) lengthBeats = kDefaultLengthBeats;
        for (auto& p : points)
        {
            p.x = juce::jlimit (0.0f, 1.0f, std::isfinite (p.x) ? p.x : 0.0f);
            p.y = juce::jlimit (0.0f, 1.0f, std::isfinite (p.y) ? p.y : 0.0f);
            p.tension = juce::jlimit (-1.0f, 1.0f, std::isfinite (p.tension) ? p.tension : 0.0f);
        }
        if (points.size() > (size_t) kMaxPoints) points.resize ((size_t) kMaxPoints);
        sortPoints();
        if (points.empty()) points.push_back ({ 0.0f, 0.0f, 0.0f, false });
        points.front().x = 0.0f;
    }

    /** Index of the point whose segment contains `phase` (the last point with x <= phase). */
    int segmentAt (float phase) const noexcept
    {
        int i = 0;
        const int n = (int) points.size();
        while (i + 1 < n && points[(size_t) (i + 1)].x <= phase) ++i;
        return i;
    }

    /** Envelope value at phase 0..1 (the same maths as the engine). */
    float valueAt (float phase) const noexcept
    {
        if (points.empty()) return 0.0f;
        phase = juce::jlimit (0.0f, 1.0f, phase);
        const int i = segmentAt (phase);
        const auto& p0 = points[(size_t) i];
        if (i + 1 >= (int) points.size() || p0.hold) return p0.y;
        const auto& p1 = points[(size_t) (i + 1)];
        const float dx = p1.x - p0.x;
        if (dx <= 1.0e-6f) return p1.y;
        return p0.y + (p1.y - p0.y) * curveShape ((phase - p0.x) / dx, p0.tension);
    }

    //==============================================================================
    // Editing helpers (the UI calls these, then the processor records an undo step).

    /** Rotates the shape by dx (fraction of the length) with wrap-around; keeps a point at x = 0. */
    void shift (float dx)
    {
        if (points.empty()) return;
        dx -= std::floor (dx);
        if (dx < 1.0e-6f) return;
        const float wrapPhase = 1.0f - dx;
        const int seg = segmentAt (juce::jmin (0.999999f, wrapPhase));
        const float cutValue = valueAt (wrapPhase);
        Point cut { 0.0f, cutValue, points[(size_t) seg].tension, points[(size_t) seg].hold };
        // points that wrap past the end come first (they now sit at x < dx), the rest follow
        std::vector<Point> wrapped, rest;
        for (auto p : points)
        {
            const float nx = p.x + dx;
            if (nx >= 1.0f - 1.0e-6f) { p.x = nx - 1.0f; wrapped.push_back (p); }
            else { p.x = nx; rest.push_back (p); }
        }
        points = std::move (wrapped);
        points.insert (points.end(), rest.begin(), rest.end());
        if (points.empty() || points.front().x > 1.0e-4f) points.insert (points.begin(), cut);
        else points.front().x = 0.0f;
        // the segment that was cut continues to the cut value at the end of the loop
        if (points.back().x < 1.0f - 1.0e-4f && (int) points.size() < kMaxPoints) points.push_back ({ 1.0f, cutValue, 0.0f, false });
        clampAll();
    }

    /** Mirrors the shape in time (x -> 1 - x). */
    void flipX()
    {
        if (points.empty()) return;
        std::vector<Point> out;
        const int n = (int) points.size();
        // the tail (after the last point) is a hold of the last value: it becomes the head
        out.push_back ({ 0.0f, points.back().y, 0.0f, true });
        for (int i = n - 1; i >= 0; --i)
        {
            const auto& p = points[(size_t) i];
            Point q { 1.0f - p.x, p.y, 0.0f, false };
            if (i > 0)
            {
                const auto& prev = points[(size_t) (i - 1)]; // the old segment prev -> p becomes q -> q(prev)
                q.tension = -prev.tension;
                q.hold = prev.hold;
            }
            out.push_back (q);
        }
        if (out.size() > 1 && out[1].x <= 1.0e-4f) out.erase (out.begin());
        points = std::move (out);
        clampAll();
    }

    /** Mirrors the values (y -> 1 - y). */
    void flipY()
    {
        for (auto& p : points) p.y = 1.0f - p.y;
    }

    bool operator== (const Envelope& o) const noexcept
    {
        if (name != o.name || lengthBeats != o.lengthBeats || retrigger != o.retrigger || hold != o.hold || points.size() != o.points.size()) return false;
        for (size_t i = 0; i < points.size(); ++i) if (points[i] != o.points[i]) return false;
        return true;
    }
    bool operator!= (const Envelope& o) const noexcept { return ! (*this == o); }

    //==============================================================================
    // JSON: { "format": "dychka-envelope", "version": 1, "kind": "time"|"volume", "name", "info",
    //         "length": 8, "retrigger": false, "hold": false, "points": [ [x, y, tension, hold], ... ] }
    juce::var toJson (EnvKind kind) const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "dychka-envelope");
        root->setProperty ("version", 1);
        root->setProperty ("kind", kindName (kind));
        root->setProperty ("name", name);
        if (info.isNotEmpty()) root->setProperty ("info", info);
        root->setProperty ("length", lengthBeats);
        root->setProperty ("retrigger", retrigger);
        root->setProperty ("hold", hold);
        juce::Array<juce::var> arr;
        for (const auto& p : points)
        {
            juce::Array<juce::var> pt;
            pt.add ((double) p.x);
            pt.add ((double) p.y);
            pt.add ((double) p.tension);
            pt.add (p.hold ? 1 : 0);
            arr.add (juce::var (pt));
        }
        root->setProperty ("points", arr);
        return juce::var (root);
    }

    /** Reads an envelope; `kindOut` (optional) receives the kind stored in the file. */
    bool fromJson (const juce::var& root, EnvKind* kindOut = nullptr)
    {
        if (! root.isObject()) return false;
        if (root.getProperty ("format", "dychka-envelope").toString() != "dychka-envelope") return false;

        Envelope e;
        e.name = root.getProperty ("name", "").toString();
        e.info = root.getProperty ("info", "").toString();
        e.lengthBeats = (int) root.getProperty ("length", kDefaultLengthBeats);
        e.retrigger = (bool) root.getProperty ("retrigger", false);
        e.hold = (bool) root.getProperty ("hold", false);
        if (auto* arr = root.getProperty ("points", juce::var()).getArray())
        {
            for (const auto& v : *arr)
            {
                Point p;
                if (auto* a = v.getArray())
                {
                    if (a->size() > 0) p.x = (float) (double) a->getReference (0);
                    if (a->size() > 1) p.y = (float) (double) a->getReference (1);
                    if (a->size() > 2) p.tension = (float) (double) a->getReference (2);
                    if (a->size() > 3) p.hold = (int) a->getReference (3) != 0;
                }
                else if (v.isObject())
                {
                    p.x = (float) (double) v.getProperty ("x", 0.0);
                    p.y = (float) (double) v.getProperty ("y", 0.0);
                    p.tension = (float) (double) v.getProperty ("t", v.getProperty ("tension", 0.0));
                    p.hold = (bool) v.getProperty ("h", v.getProperty ("hold", false));
                }
                else continue;
                e.points.push_back (p);
                if (e.points.size() >= (size_t) kMaxPoints) break;
            }
        }
        e.clampAll();
        if (kindOut != nullptr) *kindOut = kindFromName (root.getProperty ("kind", "time").toString());
        *this = std::move (e);
        return true;
    }

    juce::String toJsonString (EnvKind kind) const { return juce::JSON::toString (toJson (kind), false); }
    bool fromJsonString (const juce::String& text, EnvKind* kindOut = nullptr) { return fromJson (juce::JSON::parse (text), kindOut); }
};

/** 36 TIME + 36 VOLUME slots. Slot i (0-based) is slot i+1 in the UI / parameters / MIDI. */
struct Bank
{
    juce::String name;
    std::array<std::array<Envelope, kNumSlots>, kNumKinds> slots {};

    std::array<Envelope, kNumSlots>& of (EnvKind k) noexcept { return slots[(size_t) k]; }
    const std::array<Envelope, kNumSlots>& of (EnvKind k) const noexcept { return slots[(size_t) k]; }

    // { "format": "dychka-bank", "version": 1, "name", "time": [ { "index": 0, ...envelope } ], "volume": [ ... ] }
    // `onlyThese` (nullptr = all): restrict to flagged slots - used by the DAW state to store only
    // the slots that differ from the factory bank.
    juce::var toJson (const std::array<std::array<bool, kNumSlots>, kNumKinds>* onlyThese = nullptr) const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "dychka-bank");
        root->setProperty ("version", 1);
        root->setProperty ("name", name);
        for (int k = 0; k < kNumKinds; ++k)
        {
            juce::Array<juce::var> arr;
            for (int i = 0; i < kNumSlots; ++i)
            {
                if (onlyThese != nullptr && ! (*onlyThese)[(size_t) k][(size_t) i]) continue;
                auto v = slots[(size_t) k][(size_t) i].toJson ((EnvKind) k);
                v.getDynamicObject()->setProperty ("index", i);
                arr.add (v);
            }
            root->setProperty (kindName ((EnvKind) k), arr);
        }
        return juce::var (root);
    }

    /** Fills the slots listed in the JSON; others are left untouched. Returns the number of slots
        read (0 on a malformed file). */
    int fromJson (const juce::var& root)
    {
        if (! root.isObject()) return 0;
        if (root.getProperty ("format", "").toString() != "dychka-bank") return 0;
        name = root.getProperty ("name", name).toString();
        int count = 0;
        for (int k = 0; k < kNumKinds; ++k)
        {
            if (auto* arr = root.getProperty (kindName ((EnvKind) k), juce::var()).getArray())
            {
                for (const auto& v : *arr)
                {
                    const int index = (int) v.getProperty ("index", -1);
                    if (! juce::isPositiveAndBelow (index, kNumSlots)) continue;
                    Envelope e;
                    if (e.fromJson (v)) { slots[(size_t) k][(size_t) index] = std::move (e); ++count; }
                }
            }
        }
        return count;
    }

    juce::String toJsonString (const std::array<std::array<bool, kNumSlots>, kNumKinds>* onlyThese = nullptr) const
    {
        return juce::JSON::toString (toJson (onlyThese), false);
    }
    int fromJsonString (const juce::String& text) { return fromJson (juce::JSON::parse (text)); }
};

} // namespace dychka
