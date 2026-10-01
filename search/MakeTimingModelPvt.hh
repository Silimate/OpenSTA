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

#include <array>
#include <map>
#include <string>
#include <string_view>

#include "Delay.hh"
#include "LibertyClass.hh"
#include "MinMax.hh"
#include "NetworkClass.hh"
#include "RiseFallMinMax.hh"
#include "Scene.hh"
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"

namespace sta {

class Sta;
class LibertyBuilder;

class OutputDelays
{
public:
  OutputDelays();
  TimingSense timingSense() const;

  RiseFallMinMax delays;
  // input edge -> output edge path exists for unateness
  bool rf_path_exists[RiseFall::index_count][RiseFall::index_count];
};

using ClockEdgeDelays = std::map<const ClockEdge*, RiseFallMinMax>;
using OutputPinDelays = std::map<const Pin *, OutputDelays>;

// Model clock port and clock edge of an internal path launch or capture.
class InternalClkEdge
{
public:
  LibertyPort *port;
  const RiseFall *rf;
  bool operator<(const InternalClkEdge &clk_edge) const;
};

// Worst internal path between a launch and capture clock edge
// for a check type (min_max index) and endpoint transition.
class InternalPathDelay
{
public:
  // Launch clock port edge to the endpoint.
  float launch_delay{0.0};
  // Check margin relative to the capture clock port edge.
  float check_margin{0.0};
  float slew{0.0};
  bool exists{false};
};

using InternalPathDelays =
  std::array<std::array<InternalPathDelay, RiseFall::index_count>,
             MinMax::index_count>;
// Register -> register paths to an endpoint inside the block.
using InternalEndpointPaths =
  std::map<InternalClkEdge, std::map<InternalClkEdge, InternalPathDelays>>;
// Keyed by internal pin name so the model is written in a stable order.
using InternalEndpointPathsMap = std::map<std::string, InternalEndpointPaths>;

class MakeTimingModel : public StaState
{
public:
  MakeTimingModel(std::string_view lib_name,
                  std::string_view cell_name,
                  std::string_view filename,
                  const Scene *scene,
                  const bool scalar,
                  const bool internal_paths,
                  Sta *sta);
  ~MakeTimingModel() override;
  LibertyLibrary *makeTimingModel();

private:
  void makeLibrary();
  void makeCell();
  float findArea();
  void makePorts();
  void setPortLimits(const Pin *pin,
                     LibertyPort *lib_port);
  void checkClock(Clock *clk);
  void findTimingFromInputs();
  void findTimingFromInput(Port *input_port);
  void findClkedOutputPaths();
  void findInternalPaths();
  void makeInternalPathArcs(const std::string &pin_name,
                            const InternalEndpointPaths &endpoint_paths);
  void findClkTreeDelays();
  void makeClkTreePaths(LibertyPort *lib_port,
                        const MinMax *min_max,
                        TimingSense sense,
                        const ClkDelays &delays);
  void findOutputDelays(const RiseFall *input_rf,
                        OutputPinDelays &output_pin_delays);
  void makeSetupHoldTimingArcs(const Pin *input_pin,
                               const ClockEdgeDelays &clk_margins);
  void makeInputOutputTimingArcs(const Pin *input_pin,
                                 OutputPinDelays &output_pin_delays);
  TimingModel *makeScalarCheckModel(float value,
                                    ScaleFactorType scale_factor_type,
                                    const RiseFall *rf);
  TimingModel *makeGateModelScalar(Delay delay,
                                   Slew slew,
                                   const RiseFall *rf);
  TimingModel *makeGateModelScalar(Delay delay,
                                   const RiseFall *rf);
  TimingModel *makeGateModelTable(const Pin *output_pin,
                                  Delay delay,
                                  const RiseFall *rf,
                                  const MinMax *min_max);
  // Max delay model, or min delay (retaining) model.
  TimingModel *makeOutputGateModel(const Pin *output_pin,
                                   Delay delay,
                                   const RiseFall *rf,
                                   const MinMax *min_max);
  TableTemplate *ensureTableTemplate(const TableTemplate *drvr_template,
                                     const TableAxisPtr &load_axis);
  const TableAxis *loadCapacitanceAxis(const TableModel *table);
  LibertyPort *modelPort(const Pin *pin);

  void saveSdc();
  void restoreSdc();
  void swapSdcWithBackup();

  std::string lib_name_;
  std::string cell_name_;
  std::string filename_;
  const Scene *scene_;
  SceneSet scenes_;
  const bool scalar_;
  const bool internal_paths_;
  LibertyLibrary *library_;
  LibertyCell *cell_{nullptr};
  const MinMax *min_max_;
  LibertyBuilder *lib_builder_;
  // Output driver table model template to model template.
  std::map<const TableTemplate*, TableTemplate*> template_map_;
  int tbl_template_index_{1};
  Sdc *sdc_;
  Sdc *sdc_backup_{nullptr};
  Sta *sta_;
};

} // namespace sta
