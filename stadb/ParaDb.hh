// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

// Binary parasitics database: the RC networks read from spef.
//
// Delays are already snapshotted by AnnoDb, so this is not needed to repeat a
// timing report. It is needed for everything that asks the parasitics a
// question of its own: capacitance limit checks, and any delay recalculation
// after an edit.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace sta {

class Sta;

// Means "no object here" for parasitic set references.
constexpr uint32_t para_db_id_null = 0xFFFFFFFFu;

// How a parasitic node names itself.
enum class ParaDbNodeKind : uint8_t { pin = 0, net = 1 };

// Encode the parasitics of the current scene as one db block.
std::vector<uint8_t> writeParaDbBytes(Sta *sta);

// Rebuild them. The network must already be loaded.
void readParaDbBytes(const uint8_t *data,
                     size_t size,
                     std::string_view label,
                     Sta *sta);

} // namespace sta
