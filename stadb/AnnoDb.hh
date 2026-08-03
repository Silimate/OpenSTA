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

// Binary delay annotation database: what read_sdf left on the timing graph.
//
// SDF text names gates and ports and has to be matched back onto the graph
// every time it is read. What actually matters is the result of that match:
// the annotated arc delays, slews and period checks sitting on graph edges and
// vertices. Those are written here directly, so loading is a lookup per
// annotation instead of a parse plus a name match.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace sta {

class StaState;

// Encode the graph annotations as one db block.
std::vector<uint8_t> writeAnnoDbBytes(StaState *sta);

// Apply annotations to the graph, which must already be built.
void readAnnoDbBytes(const uint8_t *data,
                     size_t size,
                     std::string_view label,
                     StaState *sta);

} // namespace sta
