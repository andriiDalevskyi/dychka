// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#pragma once
#include "EnvelopeModel.h"

namespace dychka
{

/** The factory bank: slot 1 of each kind is "Off" (flat), the following slots hold the classic
    Gross-Beat-style shapes (half-time, reverse, stutters, tape stop, gates, pumps, fades ...),
    the rest are flat "Init" user slots. Built once on first use (message thread). */
const Bank& factoryBank();

const Envelope& factoryEnvelope (EnvKind kind, int slot);

/** Number of factory shapes of a kind that actually do something (slot 1 "Off" not counted). */
int numFactoryShapes (EnvKind kind);

} // namespace dychka
