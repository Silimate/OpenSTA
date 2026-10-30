// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Silimate, Inc.
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

#include <string_view>

#include "NetworkClass.hh"

namespace sta {

class Sta;

// Write the SDC a run with the instances' module as top needs to see the
// clocks, arrivals, requireds, slews and loads its ports see in the linked
// design. All instances merge into one file, keeping the worst value per
// port bit and launching clock edge.
void
writeContextSdc(const InstanceSeq &insts,
                std::string_view filename,
                Sta *sta);

} // namespace sta
