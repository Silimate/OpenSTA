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
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sta {

class Sta;

// Serialized STA session database (.stadb).
//
// One file holds a snapshot of the loaded session, each part written as
// objects and values rather than as the text it was originally read from:
//   liberty  - NLDM libraries, LibDb encoding
//   network  - the linked netlist: cells, instances, nets, pins, NetDb encoding
//   sdc      - constraints
//   anno     - every delay and slew in the timing graph, AnnoDb encoding
//
// writeStaDb serializes the live Sta state into that file.
// readStaDb deserializes it back into a session that is ready to report on:
// the network comes back linked and the delays come back annotated, so no
// link_design, delay calculation or parasitics are needed to repeat a report.

constexpr uint32_t sta_db_version = 1;

void writeStaDb(Sta *sta,
                std::string_view filename);
void readStaDb(Sta *sta,
               std::string_view filename);

} // namespace sta
