// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "EnvelopeEditor.h"
#include "DychkaLookAndFeel.h"
#include "../Model/FactoryEnvelopes.h"
#include "../PluginProcessor.h"

namespace dychka
{

namespace
{
    constexpr float kPointRadius = 4.5f;
    constexpr float kHitRadius = 7.0f;
    constexpr float kCurveHitDistance = 8.0f;
    constexpr float kTensionPixels = 150.0f;   // mouse travel for the full -1..1 tension range

    struct SnapChoice { const char* name; int div; };
    const SnapChoice kSnapChoices[] = { { "1/4", 1 }, { "1/8", 2 }, { "1/16", 4 }, { "1/32", 8 }, { "1/12", 3 }, { "1/24", 6 }, { "Snap off", 0 } };
    constexpr int kNumSnapChoices = 7;

    juce::String beatText (float phase, int lengthBeats)
    {
        const float beats = phase * (float) lengthBeats;
        const int bar = (int) (beats / 4.0f);
        const float inBar = beats - bar * 4.0f;
        return juce::String (bar + 1) + "." + juce::String (inBar + 1.0f, 2);
    }
}

//==============================================================================
EnvelopeCanvas::EnvelopeCanvas()
{
    setPaintingIsUnclipped (true);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
}

void EnvelopeCanvas::setEnvelope (Envelope* e, EnvKind k)
{
    env = e;
    kind = k;
    if (env == nullptr || selected >= env->numPoints()) selected = -1;
    hoverPoint = hoverSeg = -1;
    repaint();
}

void EnvelopeCanvas::setPlayhead (float phase, float value, bool visible)
{
    if (juce::approximatelyEqual (phase, playPhase) && juce::approximatelyEqual (value, playValue) && visible == playVisible) return;
    playPhase = phase; playValue = value; playVisible = visible;
    repaint();
}

juce::Rectangle<float> EnvelopeCanvas::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) kMarginLeft).withTrimmedRight ((float) kMarginRight)
                                     .withTrimmedTop ((float) kMarginTop).withTrimmedBottom ((float) kMarginBottom);
}

float EnvelopeCanvas::phaseToX (float ph) const { const auto r = plot(); return r.getX() + ph * r.getWidth(); }
float EnvelopeCanvas::xToPhase (float x) const { const auto r = plot(); return juce::jlimit (0.0f, 1.0f, (x - r.getX()) / juce::jmax (1.0f, r.getWidth())); }
float EnvelopeCanvas::valueToY (float v) const
{
    const auto r = plot();
    return kind == EnvKind::time ? r.getY() + v * r.getHeight() : r.getBottom() - v * r.getHeight();
}
float EnvelopeCanvas::yToValue (float y) const
{
    const auto r = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (y - r.getY()) / juce::jmax (1.0f, r.getHeight()));
    return kind == EnvKind::time ? t : 1.0f - t;
}

float EnvelopeCanvas::snapPhase (float ph, bool fine) const
{
    if (env == nullptr || snapDiv <= 0 || fine) return ph;
    const float step = 1.0f / (float) (env->lengthBeats * snapDiv);
    return juce::jlimit (0.0f, 1.0f, std::round (ph / step) * step);
}

float EnvelopeCanvas::snapValue (float v, bool fine) const
{
    if (snapDiv <= 0 || fine) return v;
    const float step = kind == EnvKind::time ? 1.0f / (kMaxDelayBeats * (float) snapDiv) : 0.1f;
    return juce::jlimit (0.0f, 1.0f, std::round (v / step) * step);
}

int EnvelopeCanvas::pointAt (juce::Point<float> p) const
{
    if (env == nullptr) return -1;
    int best = -1;
    float bestD = kHitRadius;
    for (int i = 0; i < env->numPoints(); ++i)
    {
        const auto& pt = env->points[(size_t) i];
        const float dist = p.getDistanceFrom ({ phaseToX (pt.x), valueToY (pt.y) });
        if (dist <= bestD) { bestD = dist; best = i; }
    }
    return best;
}

int EnvelopeCanvas::segmentNear (juce::Point<float> p) const
{
    if (env == nullptr || ! plot().expanded (0.0f, kCurveHitDistance).contains (p)) return -1;
    const float ph = xToPhase (p.x);
    const float y = valueToY (env->valueAt (ph));
    if (std::abs (y - p.y) > kCurveHitDistance) return -1;
    return env->segmentAt (ph);
}

juce::String EnvelopeCanvas::valueText (float v) const
{
    if (kind == EnvKind::time)
    {
        const float beats = v * kMaxDelayBeats;
        return juce::String (-beats, 2) + " beats";
    }
    return juce::String ((int) std::round (v * 100.0f)) + " %";
}

juce::String EnvelopeCanvas::describeAt (juce::Point<float> p) const
{
    if (env == nullptr) return {};
    const float ph = xToPhase (p.x);
    juce::String s = "beat " + beatText (ph, env->lengthBeats);

    if (hoverPoint >= 0)
    {
        const auto& pt = env->points[(size_t) hoverPoint];
        s = "point " + juce::String (hoverPoint + 1) + juce::String::fromUTF8 (" \xC2\xB7 beat ") + beatText (pt.x, env->lengthBeats)
          + juce::String::fromUTF8 (" \xC2\xB7 ") + valueText (pt.y);
        if (pt.hold) s += juce::String::fromUTF8 (" \xC2\xB7 HOLD");
        else if (std::abs (pt.tension) > 0.005f) s += juce::String::fromUTF8 (" \xC2\xB7 tension ") + juce::String (pt.tension, 2);
        return s;
    }

    s += juce::String::fromUTF8 (" \xC2\xB7 ") + valueText (yToValue (p.y));
    if (kind == EnvKind::time)
    {
        // local playback speed: 1 - d(delay)/dt, from the numerical slope of the curve
        const int seg = env->segmentAt (ph);
        const auto& p0 = env->points[(size_t) seg];
        juce::String speed;
        if (p0.hold || seg + 1 >= env->numPoints()) speed = "step, speed 1x";
        else
        {
            const float eps = 0.002f;
            const float dv = env->valueAt (juce::jmin (1.0f, ph + eps)) - env->valueAt (juce::jmax (0.0f, ph - eps));
            const float slope = dv * kMaxDelayBeats / (2.0f * eps * (float) env->lengthBeats); // beats of delay per beat
            const float sp = 1.0f - slope;
            if (std::abs (sp) < 0.02f) speed = "frozen";
            else if (sp < 0.0f) speed = "reverse " + juce::String (-sp, 2) + "x";
            else speed = "speed " + juce::String (sp, 2) + "x";
        }
        s += juce::String::fromUTF8 (" \xC2\xB7 ") + speed;
    }
    return s;
}

//==============================================================================
void EnvelopeCanvas::paint (juce::Graphics& g)
{
    const auto r = plot();
    g.setColour (DychkaLookAndFeel::lcdBg);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    if (env == nullptr) return;

    const int L = env->lengthBeats;
    const auto colour = kind == EnvKind::time ? DychkaLookAndFeel::amber : DychkaLookAndFeel::teal;
    const auto gridFaint = juce::Colour (0xff1c1f23), gridBeat = juce::Colour (0xff2c3036), gridBar = juce::Colour (0xff3d424a);
    const float pxPerBeat = r.getWidth() / (float) L;

    // vertical grid: snap divisions (if not too dense), beats, bars
    if (snapDiv > 1 && pxPerBeat / snapDiv >= 6.0f)
    {
        g.setColour (gridFaint);
        for (int i = 1; i < L * snapDiv; ++i)
            if (i % snapDiv != 0)
            {
                const float x = r.getX() + (float) i / (float) (L * snapDiv) * r.getWidth();
                g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
            }
    }
    for (int b = 0; b <= L; ++b)
    {
        const float x = r.getX() + (float) b / (float) L * r.getWidth();
        g.setColour (b % 4 == 0 ? gridBar : gridBeat);
        g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
        if (b < L && (pxPerBeat >= 28.0f || b % 4 == 0))
        {
            g.setColour (juce::Colour (0xff6b6f77));
            g.setFont (DychkaLookAndFeel::monoFont (9.0f));
            g.drawText (juce::String (b / 4 + 1) + "." + juce::String (b % 4 + 1), juce::Rectangle<float> (x + 3.0f, r.getBottom() + 1.0f, 40.0f, 13.0f), juce::Justification::topLeft);
        }
    }

    // horizontal grid + labels
    g.setFont (DychkaLookAndFeel::monoFont (9.0f));
    if (kind == EnvKind::time)
    {
        const float pyPerBeat = r.getHeight() / kMaxDelayBeats;
        for (int j = 0; j <= (int) kMaxDelayBeats; ++j)
        {
            const float y = r.getY() + j * pyPerBeat;
            g.setColour (j % 4 == 0 ? gridBar : gridBeat);
            g.fillRect (r.getX(), y - 0.5f, r.getWidth(), 1.0f);
            g.setColour (juce::Colour (0xff6b6f77));
            juce::String label = j == 0 ? "live" : (j == 4 ? "-1 bar" : (j == 8 ? "-2 bars" : "-" + juce::String (j)));
            g.drawText (label, juce::Rectangle<float> (2.0f, y - 6.0f, (float) kMarginLeft - 6.0f, 12.0f), juce::Justification::centredRight);
        }
        // freeze guides: slope 1 (one beat back per beat) - a line along a guide is a frozen read head
        {
            juce::Path guides;
            const float dx = r.getWidth() + r.getHeight() * pxPerBeat / pyPerBeat;
            for (int b = -(int) kMaxDelayBeats; b < L; ++b)
            {
                const float x0 = r.getX() + b * pxPerBeat;
                guides.startNewSubPath (x0, r.getY());
                guides.lineTo (x0 + dx, r.getY() + dx / pxPerBeat * pyPerBeat);
            }
            g.saveState();
            g.reduceClipRegion (r.toNearestInt());
            juce::Path dashed;
            const float dashes[] = { 3.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, guides, dashes, 2);
            g.setColour (DychkaLookAndFeel::amber.withAlpha (0.10f));
            g.fillPath (dashed);
            g.restoreState();
        }
    }
    else
    {
        for (int j = 0; j <= 4; ++j)
        {
            const float y = r.getY() + j * r.getHeight() / 4.0f;
            g.setColour (j == 0 || j == 4 ? gridBar : gridBeat);
            g.fillRect (r.getX(), y - 0.5f, r.getWidth(), 1.0f);
            g.setColour (juce::Colour (0xff6b6f77));
            g.drawText (juce::String (100 - j * 25) + " %", juce::Rectangle<float> (2.0f, y - 6.0f, (float) kMarginLeft - 6.0f, 12.0f), juce::Justification::centredRight);
        }
    }

    // curve (sampled per pixel; jumps / holds come out as vertical steps)
    juce::Path curve;
    const int w = juce::jmax (2, (int) r.getWidth());
    for (int i = 0; i <= w; ++i)
    {
        const float ph = juce::jmin (0.999999f, (float) i / (float) w);
        const float v = env->valueAt (ph);
        const float x = r.getX() + (float) i;
        const float y = valueToY (v);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    const float baseY = kind == EnvKind::time ? r.getY() : r.getBottom();
    fill.lineTo (r.getRight(), baseY);
    fill.lineTo (r.getX(), baseY);
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.14f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // hovered segment highlight
    if (hoverSeg >= 0 && hoverPoint < 0 && hoverSeg < env->numPoints())
    {
        const auto& p0 = env->points[(size_t) hoverSeg];
        const float x0 = phaseToX (p0.x);
        const float x1 = hoverSeg + 1 < env->numPoints() ? phaseToX (env->points[(size_t) (hoverSeg + 1)].x) : r.getRight();
        g.setColour (DychkaLookAndFeel::text.withAlpha (0.06f));
        g.fillRect (x0, r.getY(), juce::jmax (1.0f, x1 - x0), r.getHeight());
    }

    // play head
    if (playVisible)
    {
        const float x = phaseToX (playPhase);
        g.setColour (DychkaLookAndFeel::amber.withAlpha (0.55f));
        g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
        const float y = valueToY (playValue);
        g.setColour (juce::Colour (0xffe9e6df));
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }

    // points
    for (int i = 0; i < env->numPoints(); ++i)
    {
        const auto& pt = env->points[(size_t) i];
        const float x = phaseToX (pt.x), y = valueToY (pt.y);
        const float rad = i == hoverPoint ? kPointRadius + 1.5f : kPointRadius;
        g.setColour (DychkaLookAndFeel::lcdBg);
        g.fillEllipse (x - rad - 1.0f, y - rad - 1.0f, (rad + 1.0f) * 2.0f, (rad + 1.0f) * 2.0f);
        g.setColour (pt.hold ? juce::Colour (0xffe9e6df) : colour);
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        if (i == selected)
        {
            g.setColour (juce::Colour (0xffe9e6df));
            g.drawEllipse (x - rad - 2.0f, y - rad - 2.0f, (rad + 2.0f) * 2.0f, (rad + 2.0f) * 2.0f, 1.2f);
        }
    }

    g.setColour (DychkaLookAndFeel::lcdBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

//==============================================================================
void EnvelopeCanvas::mouseMove (const juce::MouseEvent& e)
{
    if (env == nullptr) return;
    const int p = pointAt (e.position);
    const int s = p < 0 ? segmentNear (e.position) : -1;
    if (p != hoverPoint || s != hoverSeg) { hoverPoint = p; hoverSeg = s; repaint(); }
    setMouseCursor (p >= 0 ? juce::MouseCursor::DraggingHandCursor : (s >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor));
    if (onHoverText) onHoverText (describeAt (e.position));
}

void EnvelopeCanvas::mouseExit (const juce::MouseEvent&)
{
    if (hoverPoint != -1 || hoverSeg != -1) { hoverPoint = hoverSeg = -1; repaint(); }
    if (onHoverText) onHoverText ({});
}

void EnvelopeCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (env == nullptr) return;
    grabKeyboardFocus();
    const int p = pointAt (e.position);
    const int s = p < 0 ? segmentNear (e.position) : -1;

    if (e.mods.isPopupMenu())
    {
        selected = p;
        repaint();
        showMenu (e.getPosition(), p, s);
        return;
    }

    if (onEditBegin) onEditBegin();
    if (p >= 0)
    {
        selected = dragIndex = p;
        drag = Drag::point;
    }
    else if (s >= 0)
    {
        drag = Drag::tension;
        dragIndex = s;
        dragStartMouseY = e.position.y;
        dragStartTension = env->points[(size_t) s].tension;
        const float y0 = valueToY (env->points[(size_t) s].y);
        const float y1 = s + 1 < env->numPoints() ? valueToY (env->points[(size_t) (s + 1)].y) : y0;
        dragTensionSign = y1 > y0 ? 1.0f : -1.0f;
        selected = -1;
    }
    else
    {
        // add a point at the (snapped) mouse position and start dragging it
        const bool fine = e.mods.isShiftDown();
        Point np { snapPhase (xToPhase (e.position.x), fine), snapValue (yToValue (e.position.y), fine), 0.0f, false };
        const int seg = env->segmentAt (np.x);
        np.hold = env->points[(size_t) seg].hold;
        if (env->numPoints() >= kMaxPoints) { if (onEditEnd) onEditEnd(); return; }
        env->points.push_back (np);
        env->sortPoints();
        // find it again after the sort (the last point with this x)
        dragIndex = -1;
        for (int i = env->numPoints() - 1; i >= 0; --i)
            if (juce::approximatelyEqual (env->points[(size_t) i].x, np.x) && juce::approximatelyEqual (env->points[(size_t) i].y, np.y)) { dragIndex = i; break; }
        selected = dragIndex;
        drag = dragIndex >= 0 ? Drag::point : Drag::none;
        if (onEdit) onEdit();
    }
    hoverPoint = selected;
    repaint();
}

void EnvelopeCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (env == nullptr || drag == Drag::none || ! juce::isPositiveAndBelow (dragIndex, env->numPoints())) return;
    const bool fine = e.mods.isShiftDown();
    auto& pt = env->points[(size_t) dragIndex];

    if (drag == Drag::point)
    {
        float x = snapPhase (xToPhase (e.position.x), fine);
        if (dragIndex == 0) x = 0.0f;
        else
        {
            const float lo = env->points[(size_t) (dragIndex - 1)].x;
            const float hi = dragIndex + 1 < env->numPoints() ? env->points[(size_t) (dragIndex + 1)].x : 1.0f;
            x = juce::jlimit (lo, hi, x);
        }
        pt.x = x;
        pt.y = snapValue (yToValue (e.position.y), fine);
    }
    else
    {
        pt.tension = juce::jlimit (-1.0f, 1.0f, dragStartTension + dragTensionSign * (dragStartMouseY - e.position.y) / kTensionPixels);
    }
    if (onEdit) onEdit();
    if (onHoverText) onHoverText (describeAt (e.position));
    repaint();
}

void EnvelopeCanvas::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::none) return;
    drag = Drag::none;
    dragIndex = -1;
    if (onEditEnd) onEditEnd();
}

void EnvelopeCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (env == nullptr) return;
    const int p = pointAt (e.position);
    if (p > 0)
    {
        if (onEditBegin) onEditBegin();
        deletePoint (p);
        if (onEdit) onEdit();
        if (onEditEnd) onEditEnd();
        return;
    }
    const int s = p < 0 ? segmentNear (e.position) : -1;
    if (s >= 0)
    {
        if (onEditBegin) onEditBegin();
        env->points[(size_t) s].hold = ! env->points[(size_t) s].hold;
        if (onEdit) onEdit();
        if (onEditEnd) onEditEnd();
        repaint();
    }
}

void EnvelopeCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (env == nullptr) return;
    const int s = pointAt (e.position) >= 0 ? -1 : segmentNear (e.position);
    if (s < 0 || w.deltaY == 0.0f) return;
    if (onEditBegin) onEditBegin();
    auto& pt = env->points[(size_t) s];
    pt.tension = juce::jlimit (-1.0f, 1.0f, pt.tension + (w.deltaY > 0 ? 0.05f : -0.05f));
    if (onEdit) onEdit();
    if (onEditEnd) onEditEnd();
    if (onHoverText) onHoverText (describeAt (e.position));
    repaint();
}

bool EnvelopeCanvas::keyPressed (const juce::KeyPress& key)
{
    if (env == nullptr) return false;
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && selected > 0 && selected < env->numPoints())
    {
        if (onEditBegin) onEditBegin();
        deletePoint (selected);
        if (onEdit) onEdit();
        if (onEditEnd) onEditEnd();
        return true;
    }
    return false;
}

void EnvelopeCanvas::deletePoint (int index)
{
    if (env == nullptr || index <= 0 || index >= env->numPoints()) return; // the first point stays
    env->points.erase (env->points.begin() + index);
    selected = -1;
    hoverPoint = hoverSeg = -1;
    repaint();
}

void EnvelopeCanvas::showMenu (juce::Point<int> where, int point, int segment)
{
    juce::PopupMenu menu;
    const int seg = point >= 0 ? point : segment;
    if (point > 0) menu.addItem (1, "Delete point");
    if (point >= 0) menu.addItem (2, "Reset value to " + juce::String (kind == EnvKind::time ? "live" : "100 %"));
    if (seg >= 0 && seg < env->numPoints())
    {
        menu.addItem (3, env->points[(size_t) seg].hold ? "Curve segment (unhold)" : "Hold segment (step)");
        menu.addItem (4, "Straight segment (tension 0)");
    }
    if (point < 0)
        menu.addItem (5, "Add point here");
    const auto snapped = juce::Point<float> ((float) where.x, (float) where.y);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<int> (where.x, where.y, 1, 1))),
        [this, point, seg, snapped] (int result)
        {
            if (result == 0 || env == nullptr) return;
            if (onEditBegin) onEditBegin();
            if (result == 1) deletePoint (point);
            else if (result == 2 && point >= 0 && point < env->numPoints()) env->points[(size_t) point].y = flatValue (kind);
            else if (result == 3 && seg >= 0 && seg < env->numPoints()) env->points[(size_t) seg].hold = ! env->points[(size_t) seg].hold;
            else if (result == 4 && seg >= 0 && seg < env->numPoints()) env->points[(size_t) seg].tension = 0.0f;
            else if (result == 5 && env->numPoints() < kMaxPoints)
            {
                Point np { snapPhase (xToPhase (snapped.x), false), snapValue (yToValue (snapped.y), false), 0.0f, false };
                np.hold = env->points[(size_t) env->segmentAt (np.x)].hold;
                env->points.push_back (np);
                env->sortPoints();
            }
            if (onEdit) onEdit();
            if (onEditEnd) onEditEnd();
            repaint();
        });
}

//==============================================================================
EnvelopeEditor::EnvelopeEditor (DychkaAudioProcessor& processorIn) : processor (processorIn)
{
    setPaintingIsUnclipped (true);

    timeTab.setTooltip ("Edit the TIME envelope of the selected time slot");
    volTab.setTooltip ("Edit the VOLUME envelope of the selected volume slot");
    timeTab.onClick = [this] { processor.setEditKind (EnvKind::time); };
    volTab.onClick = [this] { processor.setEditKind (EnvKind::volume); };
    addAndMakeVisible (timeTab);
    addAndMakeVisible (volTab);

    nameField.setFont (DychkaLookAndFeel::monoFont (13.0f));
    nameField.setSelectAllWhenFocused (true);
    nameField.setMultiLine (false);
    nameField.setReturnKeyStartsNewLine (false);
    nameField.setTooltip ("Envelope name (shown in the slot LCD)");
    auto commitName = [this]
    {
        if (updating) return;
        auto& e = envelope();
        const auto text = nameField.getText().substring (0, 32).trim();
        if (text != e.name) { e.name = text.isEmpty() ? juce::String ("Init") : text; processor.envelopeEdited (kind()); }
    };
    nameField.onReturnKey = [this, commitName] { commitName(); giveAwayKeyboardFocus(); };
    nameField.onFocusLost = commitName;
    nameField.onEscapeKey = [this] { refresh(); giveAwayKeyboardFocus(); };
    addAndMakeVisible (nameField);

    for (int i = 0; i < kNumLengthChoices; ++i) lengthCombo.addItem (lengthName (kLengthChoices[i]), i + 1);
    lengthCombo.setTooltip ("Length of the envelope loop. The TIME axis always spans two bars of delay");
    lengthCombo.onChange = [this]
    {
        if (updating || lengthCombo.getSelectedId() <= 0) return;
        beginEdit();
        envelope().lengthBeats = kLengthChoices[lengthCombo.getSelectedId() - 1];
        edited(); endEdit();
    };
    addAndMakeVisible (lengthCombo);

    for (int i = 0; i < kNumSnapChoices; ++i) snapCombo.addItem (kSnapChoices[i].name, i + 1);
    snapCombo.setSelectedId (3, juce::dontSendNotification);
    snapCombo.setTooltip ("Snap grid for points (time, and delay in beats for TIME envelopes). Hold Shift while dragging for no snap");
    snapCombo.onChange = [this]
    {
        if (snapCombo.getSelectedId() <= 0) return;
        snapDivisions = kSnapChoices[snapCombo.getSelectedId() - 1].div;
        canvas.setSnap (snapDivisions);
    };
    addAndMakeVisible (snapCombo);

    shiftLeftButton.setButtonText (juce::String::fromUTF8 ("\xE2\x97\x80"));
    shiftRightButton.setButtonText (juce::String::fromUTF8 ("\xE2\x96\xB6"));
    shiftLeftButton.setTooltip ("Rotate the envelope one snap step earlier");
    shiftRightButton.setTooltip ("Rotate the envelope one snap step later");
    auto shiftBy = [this] (float dir)
    {
        const int div = snapDivisions > 0 ? snapDivisions : 4;
        beginEdit();
        envelope().shift (dir / (float) (envelope().lengthBeats * div));
        edited(); endEdit();
    };
    shiftLeftButton.onClick = [shiftBy] { shiftBy (-1.0f); };
    shiftRightButton.onClick = [shiftBy] { shiftBy (1.0f); };
    addAndMakeVisible (shiftLeftButton);
    addAndMakeVisible (shiftRightButton);

    flipButton.setTooltip ("Mirror the envelope in time");
    flipButton.onClick = [this] { beginEdit(); envelope().flipX(); edited(); endEdit(); };
    invertButton.setTooltip ("Invert the values (TIME: mirror the delay, VOLUME: invert the gain)");
    invertButton.onClick = [this] { beginEdit(); envelope().flipY(); edited(); endEdit(); };
    clearButton.setTooltip ("Flat envelope (no effect)");
    clearButton.onClick = [this]
    {
        beginEdit();
        auto& e = envelope();
        const auto name = e.name;
        e = Envelope::flat (kind(), name);
        edited(); endEdit();
    };
    initButton.setTooltip ("Reset this slot to its factory shape (flat for a user slot)");
    initButton.onClick = [this] { processor.resetSlotToFactory (kind(), processor.getCurrentSlot (kind())); };
    for (auto* b : { &flipButton, &invertButton, &clearButton, &initButton }) addAndMakeVisible (*b);

    retrigButton.setClickingTogglesState (true);
    retrigButton.setTooltip ("RETRIG: the envelope restarts from its beginning when this slot is selected (click, automation or MIDI note). Off: it follows the song grid");
    retrigButton.onClick = [this] { if (updating) return; beginEdit(); envelope().retrigger = retrigButton.getToggleState(); edited(); endEdit(); };
    holdButton.setClickingTogglesState (true);
    holdButton.setTooltip ("HOLD: a MIDI note keeps this slot selected after the key is released (latch). Off: the note is momentary and the previous slot returns");
    holdButton.onClick = [this] { if (updating) return; beginEdit(); envelope().hold = holdButton.getToggleState(); edited(); endEdit(); };
    addAndMakeVisible (retrigButton);
    addAndMakeVisible (holdButton);

    libraryButton.setButtonText (juce::String::fromUTF8 ("Library \xE2\x96\xBE"));
    libraryButton.setTooltip ("Load a factory shape into this slot");
    libraryButton.onClick = [this] { showLibrary(); };
    addAndMakeVisible (libraryButton);

    canvas.onEditBegin = [this] { beginEdit(); };
    canvas.onEdit = [this] { edited(); };
    canvas.onEditEnd = [this] { endEdit(); };
    canvas.onHoverText = [this] (const juce::String& t) { if (t != hoverText) { hoverText = t; repaint (infoBounds); } };
    canvas.setSnap (snapDivisions);
    addAndMakeVisible (canvas);

    refresh();
}

EnvelopeEditor::~EnvelopeEditor() = default;

Envelope& EnvelopeEditor::envelope() { return processor.getCurrentEnvelope (kind()); }
EnvKind EnvelopeEditor::kind() const { return processor.getEditKind(); }

void EnvelopeEditor::beginEdit() { processor.beginUndoGesture(); }
void EnvelopeEditor::edited() { processor.envelopeEdited (kind()); }
void EnvelopeEditor::endEdit() { processor.endUndoGesture(); }

void EnvelopeEditor::showLibrary()
{
    const auto k = kind();
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String (k == EnvKind::time ? "TIME" : "VOLUME") + " shapes");
    const auto& factory = factoryBank().of (k);
    for (int i = 0; i < kNumSlots; ++i)
        if (i == 0 || factory[(size_t) i].name != "Init")
            menu.addItem (100 + i, factory[(size_t) i].name);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libraryButton), [this, k] (int result)
    {
        if (result < 100) return;
        const auto& shape = factoryEnvelope (k, result - 100);
        processor.replaceSlot (k, processor.getCurrentSlot (k), shape);
        if (onMessage) onMessage ("Loaded \"" + shape.name + "\": " + shape.info);
    });
}

void EnvelopeEditor::refresh()
{
    updating = true;
    const auto k = kind();
    auto& e = envelope();
    timeTab.setToggleState (k == EnvKind::time, juce::dontSendNotification);
    volTab.setToggleState (k == EnvKind::volume, juce::dontSendNotification);
    nameField.setText (e.name, juce::dontSendNotification);
    for (int i = 0; i < kNumLengthChoices; ++i)
        if (kLengthChoices[i] == e.lengthBeats) lengthCombo.setSelectedId (i + 1, juce::dontSendNotification);
    retrigButton.setToggleState (e.retrigger, juce::dontSendNotification);
    holdButton.setToggleState (e.hold, juce::dontSendNotification);
    canvas.setEnvelope (&e, k);
    updating = false;
    repaint();
}

void EnvelopeEditor::tick()
{
    const auto& eng = processor.getEngine();
    const bool running = processor.apvts.getRawParameterValue (ParamIDs::on)->load() >= 0.5f;
    const auto k = kind();
    // the play head is meaningful only when the edited slot is the one playing
    const bool playing = running && eng.getActiveSlot (k) == processor.getCurrentSlot (k);
    canvas.setPlayhead (eng.getPhase (k), eng.getValue (k), playing);
}

void EnvelopeEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (28);
    timeTab.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (2);
    volTab.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (8);
    snapCombo.setBounds (top.removeFromRight (78));
    top.removeFromRight (8);
    lengthCombo.setBounds (top.removeFromRight (88));
    top.removeFromRight (8);
    nameField.setBounds (top);

    area.removeFromTop (6);
    auto tools = area.removeFromTop (24);
    shiftLeftButton.setBounds (tools.removeFromLeft (30));
    tools.removeFromLeft (2);
    shiftRightButton.setBounds (tools.removeFromLeft (30));
    tools.removeFromLeft (6);
    flipButton.setBounds (tools.removeFromLeft (46));
    tools.removeFromLeft (4);
    invertButton.setBounds (tools.removeFromLeft (40));
    tools.removeFromLeft (4);
    clearButton.setBounds (tools.removeFromLeft (50));
    tools.removeFromLeft (4);
    initButton.setBounds (tools.removeFromLeft (42));
    tools.removeFromLeft (8);
    retrigButton.setBounds (tools.removeFromLeft (56));
    tools.removeFromLeft (4);
    holdButton.setBounds (tools.removeFromLeft (46));
    tools.removeFromLeft (8);
    libraryButton.setBounds (tools);

    area.removeFromTop (8);
    infoBounds = area.removeFromBottom (14);
    area.removeFromBottom (4);
    canvas.setBounds (area);
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (DychkaLookAndFeel::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (DychkaLookAndFeel::panelBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto info = infoBounds;
    if (hoverText.isNotEmpty())
    {
        g.setColour (DychkaLookAndFeel::amber);
        g.setFont (DychkaLookAndFeel::monoFont (10.0f));
        g.drawText (hoverText, info, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (juce::Colour (0xff6b6f77));
        g.setFont (DychkaLookAndFeel::sansFont (10.0f));
        const auto& e = processor.getEnvelope (kind(), processor.getCurrentSlot (kind()));
        g.drawText (e.info.isNotEmpty() ? e.info : juce::String ("Click to add a point, drag to move, drag a segment to bend it, double-click a segment for a hold step, Shift = no snap."),
                    info, juce::Justification::centredLeft, true);
    }
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent&)
{
    giveAwayKeyboardFocus();
}

} // namespace dychka
