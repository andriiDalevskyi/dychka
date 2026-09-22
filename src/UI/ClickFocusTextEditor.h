// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace dychka
{

/** A TextEditor that only becomes active when the user clicks it (or tabs into it).

    Hosts re-activate the plug-in window constantly (REAPER does it on mouse-over) and JUCE then
    hands keyboard focus back to the last focused component or the first focusable child, which
    used to be a text field: a stray Space or letter would then rename the pad. Focus handed over
    programmatically is refused here and redirected to the editor root instead. */
class ClickFocusTextEditor : public juce::TextEditor
{
public:
    using juce::TextEditor::TextEditor;

    void focusGained (FocusChangeType cause) override
    {
        if (cause != juce::Component::focusChangedByMouseClick && cause != juce::Component::focusChangedByTabKey)
        {
            juce::Component::SafePointer<juce::Component> self (this);
            juce::MessageManager::callAsync ([self]
            {
                if (self == nullptr || ! self->hasKeyboardFocus (false))
                    return;
                if (auto* root = self->findParentComponentOfClass<juce::AudioProcessorEditor>())
                    root->grabKeyboardFocus();
                else
                    self->giveAwayKeyboardFocus();
            });
            return; // do not show the caret / select-all for a focus we are about to refuse
        }

        juce::TextEditor::focusGained (cause);
    }
};

} // namespace dychka
