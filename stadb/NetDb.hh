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

// Binary netlist database: the linked network graph as bytes.
//
// This is the netlist equivalent of LibDb. Instead of printing Verilog and
// parsing it back, the graph itself is written out: cells and their ports,
// the instance tree, the nets, and the pins that tie instances to nets.
// Reading it rebuilds the same objects through the NetworkReader edit API, so
// the network comes back already linked and no link_design step is needed.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace sta {

class Network;
class NetworkReader;
class Report;

// Means "no object here" for parent/net/term references.
constexpr uint32_t net_db_id_null = 0xFFFFFFFFu;

// Which list a cell reference points at.
enum class NetDbCellKind : uint8_t { liberty = 0, design = 1 };
// How a port was declared.
enum class NetDbPortKind : uint8_t { scalar = 0, bus = 1, bundle = 2 };
// Net tied to a constant, mirroring LogicValue zero/one.
enum class NetDbConstant : uint8_t { none = 0, zero = 1, one = 2 };

// Encode the linked network as one db block.
std::vector<uint8_t> writeNetDbBytes(Network *network,
                                     Report *report);

// Rebuild the network from a block. The network is linked on return.
void readNetDbBytes(const uint8_t *data,
                    size_t size,
                    std::string_view label,
                    NetworkReader *network);

} // namespace sta
