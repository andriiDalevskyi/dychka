// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "BankStore.h"

namespace dychka
{

juce::File BankStore::defaultBanksRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Dychka Banks");
}

bool BankStore::saveBank (const Bank& bank, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (bank.toJson(), false));
}

bool BankStore::loadBank (const juce::File& file, Bank& outBank, int* slotsRead)
{
    if (! file.existsAsFile()) return false;
    const int n = outBank.fromJsonString (file.loadFileAsString());
    if (slotsRead != nullptr) *slotsRead = n;
    return n > 0;
}

bool BankStore::saveEnvelope (const Envelope& envelope, EnvKind kind, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (envelope.toJson (kind), false));
}

bool BankStore::loadEnvelope (const juce::File& file, Envelope& outEnvelope, EnvKind* kindOut)
{
    if (! file.existsAsFile()) return false;
    return outEnvelope.fromJsonString (file.loadFileAsString(), kindOut);
}

juce::String BankStore::kindOfFile (const juce::File& file)
{
    const auto name = file.getFileName().toLowerCase();
    if (name.endsWith (kBankExtension)) return "bank";
    if (name.endsWith (kEnvelopeExtension)) return "envelope";
    if (name.endsWith (".json"))
    {
        // a plain .json: peek at the format field
        const auto v = juce::JSON::parse (file);
        const auto fmt = v.getProperty ("format", "").toString();
        if (fmt == "dychka-bank") return "bank";
        if (fmt == "dychka-envelope") return "envelope";
    }
    return {};
}

} // namespace dychka
