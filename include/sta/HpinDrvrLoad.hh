// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "Set.hh"
#include "NetworkClass.hh"

namespace sta {

class HpinDrvrLoad;
class HpinDrvrLoadVisitor;

void
visitHpinDrvrLoads(const Pin *pin,
		   const Network *network,
		   HpinDrvrLoadVisitor *visitor);

class HpinDrvrLoadLess
{ 
public:
  bool operator()(const HpinDrvrLoad *drvr_load1,
		  const HpinDrvrLoad *drvr_load2) const;
};

// Abstract base class for visitDrvrLoadsThruHierPin visitor.
class HpinDrvrLoadVisitor
{
public:
  HpinDrvrLoadVisitor() {}
  virtual ~HpinDrvrLoadVisitor() {}
  virtual void visit(HpinDrvrLoad *drvr_load) = 0;
};

class HpinDrvrLoad
{
public:
  HpinDrvrLoad(const Pin *drvr,
	       const Pin *load,
	       PinSet *hpins_from_drvr,
	       PinSet *hpins_to_load);
  ~HpinDrvrLoad();
  void report(const Network *network);
  HpinDrvrLoad(const Pin *drvr,
	       const Pin *load);
  const Pin *drvr() const { return drvr_; }
  const Pin *load() const { return load_; }
  PinSet *hpinsFromDrvr() { return hpins_from_drvr_; }
  PinSet *hpinsToLoad() { return hpins_to_load_; }
  void setDrvr(const Pin *drvr);
 
private:
  const Pin *drvr_;
  const Pin *load_;
  PinSet *hpins_from_drvr_;
  PinSet *hpins_to_load_;
};

} // namespace
