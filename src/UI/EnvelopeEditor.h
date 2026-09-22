// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/EnvelopeModel.h"
#include "ClickFocusTextEditor.h"
#include <functional>
#include <memory>

class DychkaAudioProcessor;

namespace dychka
{

/** The envelope canvas. Beat grid, the curve with its points, hold steps, tension handles and the
    play head. Click adds a point (snapped), drag moves it, drag on a segment bends it (tension),
    double-click a segment toggles hold, double-click / Delete removes a point, right-click opens
    the point / segment menu. Shift while dragging disables the snap. Edits go straight into the
    Envelope; the owner records undo steps through the callbacks. */
class EnvelopeCanvas : public juce::Component
{
public:
    EnvelopeCanvas();

    /** The envelope to display / edit (owned by the bank; must stay valid until the next call). */
    void setEnvelope (Envelope* envelope, EnvKind kind);
    /** Snap grid: divisions per beat (1 = quarters, 2 = eighths, 4 = sixteenths ...), 0 = off. */
    void setSnap (int divisionsPerBeat) { snapDiv = divisionsPerBeat; repaint(); }
    void setPlayhead (float phase, float value, bool visible);

    std::function<void()> onEditBegin, onEdit, onEditEnd;
    std::function<void (const juce::String&)> onHoverText;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kMarginLeft = 44, kMarginRight = 8, kMarginTop = 8, kMarginBottom = 16;

private:
    Envelope* env = nullptr;
    EnvKind kind = EnvKind::time;
    int snapDiv = 4;
    int selected = -1, hoverPoint = -1, hoverSeg = -1;
    enum class Drag { none, point, tension } drag = Drag::none;
    int dragIndex = -1;
    float dragStartMouseY = 0.0f, dragStartTension = 0.0f, dragTensionSign = 1.0f;
    float playPhase = 0.0f, playValue = 0.0f;
    bool playVisible = false;

    juce::Rectangle<float> plot() const;
    float phaseToX (float ph) const;
    float xToPhase (float x) const;
    float valueToY (float v) const;
    float yToValue (float y) const;
    float snapPhase (float ph, bool fine) const;
    float snapValue (float v, bool fine) const;
    int pointAt (juce::Point<float> p) const;
    int segmentNear (juce::Point<float> p) const;
    juce::String describeAt (juce::Point<float> p) const;
    juce::String valueText (float v) const;
    void deletePoint (int index);
    void showMenu (juce::Point<int> where, int point, int segment);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeCanvas)
};

/** Centre column: the envelope editor. TIME / VOLUME tabs, name, LENGTH, SNAP, tools
    (shift, flip, invert, clear, init, RETRIG, HOLD, LIBRARY) and the canvas. */
class EnvelopeEditor : public juce::Component
{
public:
    explicit EnvelopeEditor (DychkaAudioProcessor& processor);
    ~EnvelopeEditor() override;

    /** Re-reads the edited envelope. */
    void refresh();
    /** ~30 Hz: play head. */
    void tick();

    std::function<void (const juce::String& message)> onMessage;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    DychkaAudioProcessor& processor;

    juce::TextButton timeTab { "TIME" }, volTab { "VOLUME" };
    ClickFocusTextEditor nameField;
    juce::ComboBox lengthCombo, snapCombo;
    juce::TextButton shiftLeftButton, shiftRightButton;
    juce::TextButton flipButton { "Flip" }, invertButton { "Inv" }, clearButton { "Clear" }, initButton { "Init" };
    juce::TextButton retrigButton { "Retrig" }, holdButton { "Hold" }, libraryButton;
    EnvelopeCanvas canvas;
    juce::Rectangle<int> infoBounds;
    juce::String hoverText;
    bool updating = false;
    int snapDivisions = 4;

    Envelope& envelope();
    EnvKind kind() const;
    void beginEdit();
    void edited();
    void endEdit();
    void showLibrary();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
};

} // namespace dychka
