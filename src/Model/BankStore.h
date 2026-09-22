// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include "EnvelopeModel.h"

namespace dychka
{

/** Reads / writes banks and single envelopes on disk. Message thread only. */
class BankStore
{
public:
    static constexpr const char* kBankExtension = ".dychka-bank.json";
    static constexpr const char* kEnvelopeExtension = ".dychka-envelope.json";

    /** Documents\Dychka Banks */
    static juce::File defaultBanksRoot();

    static bool saveBank (const Bank& bank, const juce::File& file);
    /** Replaces every slot listed in the file (an empty file replaces nothing). */
    static bool loadBank (const juce::File& file, Bank& outBank, int* slotsRead = nullptr);

    static bool saveEnvelope (const Envelope& envelope, EnvKind kind, const juce::File& file);
    static bool loadEnvelope (const juce::File& file, Envelope& outEnvelope, EnvKind* kindOut = nullptr);

    /** Which importer handles this file? "bank", "envelope" or "" */
    static juce::String kindOfFile (const juce::File& file);
};

} // namespace dychka
