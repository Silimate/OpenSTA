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

#include "MakeTimingModel.hh"
#include "MakeTimingModelPvt.hh"

#include <algorithm>
#include <map>

#include "Debug.hh"
#include "Units.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "liberty/LibertyBuilder.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "Sdc.hh"
#include "StaState.hh"
#include "Graph.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "Sta.hh"
#include "VisitPathEnds.hh"
#include "ArcDelayCalc.hh"
#include "ClkLatency.hh"
#include "PathExpanded.hh"
#include "PathAnalysisPt.hh"
#include "Latches.hh"
#include "ClkInfo.hh"
#include "PathGroup.hh"
#include "Tag.hh"

namespace sta {

using std::string;
using std::min;
using std::max;
using std::make_shared;

LibertyLibrary *
makeTimingModel(const char *lib_name,
                const char *cell_name,
                const char *filename,
                const Corner *corner,
                const bool scalar,
                const bool write_timing_paths,
                const unsigned int internal_path_count,
                Sta *sta)
{
  MakeTimingModel maker(lib_name, cell_name, filename, corner, scalar, write_timing_paths, internal_path_count, sta);
  return maker.makeTimingModel();
}

MakeTimingModel::MakeTimingModel(const char *lib_name,
                                 const char *cell_name,
                                 const char *filename,
                                 const Corner *corner,
                                 const bool scalar,
                                 const bool write_timing_paths,
                                 const unsigned int internal_path_count,
                                 Sta *sta) :
  StaState(sta),
  lib_name_(lib_name),
  cell_name_(cell_name),
  filename_(filename),
  corner_(corner),
  scalar_(scalar),
  write_timing_paths_(write_timing_paths),
  internal_path_count_(internal_path_count),
  cell_(nullptr),
  min_max_(MinMax::max()),
  lib_builder_(new LibertyBuilder),
  tbl_template_index_(1),
  sdc_backup_(nullptr),
  sta_(sta)
{
}

MakeTimingModel::~MakeTimingModel()
{
  delete lib_builder_;
}

LibertyLibrary *
MakeTimingModel::makeTimingModel()
{
  if (!scalar_)
    saveSdc();

  tbl_template_index_ = 1;
  makeLibrary();
  makeCell();
  makePorts();

  sta_->searchPreamble();
  ensurePathGroups();

  findTimingFromInputs();
  findClkedOutputPaths();
  findWorstInternalPaths();
  findClkTreeDelays();

  cell_->finish(false, report_, debug_);

  if (!scalar_)
    restoreSdc();
  
  return library_;
}

// Move sdc commands used by makeTimingModel to the side.
void
MakeTimingModel::saveSdc()
{
  sdc_backup_ = new Sdc(this);
  swapSdcWithBackup();
  sta_->delaysInvalid();
}

void
MakeTimingModel::restoreSdc()
{
  swapSdcWithBackup();
  delete sdc_backup_;
  sta_->delaysInvalid();
}

void
MakeTimingModel::swapSdcWithBackup()
{
  Sdc::swapPortDelays(sdc_, sdc_backup_);
  Sdc::swapPortExtCaps(sdc_, sdc_backup_);
  Sdc::swapDeratingFactors(sdc_, sdc_backup_);
  Sdc::swapClockInsertions(sdc_, sdc_backup_);
}

void
MakeTimingModel::makeLibrary()
{
  library_ = network_->makeLibertyLibrary(lib_name_, filename_);
  LibertyLibrary *default_lib = network_->defaultLibertyLibrary();
  *library_->units() = *default_lib->units();

  for (const RiseFall *rf : RiseFall::range()) {
    library_->setInputThreshold(rf, default_lib->inputThreshold(rf));
    library_->setOutputThreshold(rf, default_lib->outputThreshold(rf));
    library_->setSlewLowerThreshold(rf, default_lib->slewLowerThreshold(rf));
    library_->setSlewUpperThreshold(rf, default_lib->slewUpperThreshold(rf));
  }

  library_->setDelayModelType(default_lib->delayModelType());
  library_->setNominalProcess(default_lib->nominalProcess());
  library_->setNominalVoltage(default_lib->nominalVoltage());
  library_->setNominalTemperature(default_lib->nominalTemperature());
}

void
MakeTimingModel::makeCell()
{
  cell_ = lib_builder_->makeCell(library_, cell_name_, filename_);
  cell_->setIsMacro(true);
  cell_->setArea(findArea());
}

float
MakeTimingModel::findArea()
{
  float area = 0.0;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    const LibertyCell *cell = network_->libertyCell(inst);
    if (cell)
      area += cell->area();
  }
  delete leaf_iter;
  return area;
}

void
MakeTimingModel::makePorts()
{
  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    const char *port_name = network_->name(port);
    if (network_->isBus(port)) {
      int from_index = network_->fromIndex(port);
      int to_index = network_->toIndex(port);
      BusDcl *bus_dcl = new BusDcl(port_name, from_index, to_index);
      library_->addBusDcl(bus_dcl);
      LibertyPort *lib_port = lib_builder_->makeBusPort(cell_, port_name,
                                                        from_index, to_index,
                                                        bus_dcl);
      lib_port->setDirection(network_->direction(port));
      PortMemberIterator *member_iter = network_->memberIterator(port);
      while (member_iter->hasNext()) {
        Port *bit_port = member_iter->next();
        Pin *pin = network_->findPin(top_inst, bit_port);
        LibertyPort *lib_bit_port = modelPort(pin);
        float load_cap = graph_delay_calc_->loadCap(pin, dcalc_ap);
        lib_bit_port->setCapacitance(load_cap);
      }
      delete member_iter;
    }
    else {
      LibertyPort *lib_port = lib_builder_->makePort(cell_, port_name);
      lib_port->setDirection(network_->direction(port));
      Pin *pin = network_->findPin(top_inst, port);
      float load_cap = graph_delay_calc_->loadCap(pin, dcalc_ap);
      lib_port->setCapacitance(load_cap);
    }
  }
  delete port_iter;
}

void
MakeTimingModel::checkClock(Clock *clk)
{
  for (const Pin *pin : clk->leafPins()) {
    if (!network_->isTopLevelPort(pin))
      report_->warn(1355, "clock %s pin %s is inside model block.",
                    clk->name(),
                    network_->pathName(pin));
  }
}

void
MakeTimingModel::ensurePathGroups()
{
  if (sta_->search()->havePathGroups()) {
    return;
  }

  static constexpr int GROUP_PATH_COUNT = std::numeric_limits<int>::max();
  static constexpr int ENDPOINT_PATH_COUNT = std::numeric_limits<int>::max();
  static constexpr bool UNIQUE_PINS = false;
  static constexpr float MIN_SLACK = std::numeric_limits<float>::lowest();
  static constexpr float MAX_SLACK = std::numeric_limits<float>::max();
  static constexpr PathGroupNameSet *PATH_GROUP_NAME_SET = nullptr;
  static constexpr bool SETUP = true;
  static constexpr bool HOLD = true;
  static constexpr bool RECOVERY = true;
  static constexpr bool REMOVAL = true;
  static constexpr bool CLK_GATING_SETUP = true;
  static constexpr bool CLK_GATING_HOLD = true;
  sta_->search()->makePathGroups(
    GROUP_PATH_COUNT,
    ENDPOINT_PATH_COUNT,
    UNIQUE_PINS,
    MIN_SLACK,
    MAX_SLACK,
    PATH_GROUP_NAME_SET,
    SETUP,
    HOLD,
    RECOVERY,
    REMOVAL,
    CLK_GATING_SETUP,
    CLK_GATING_HOLD);
}

////////////////////////////////////////////////////////////////

TimingPathVertex extractTimingPathVertexDescription(StaState *sta_state,
                                                    const RiseFall *rise_fall,
                                                    DcalcAnalysisPt *dcalc_ap,
                                                    const Path *path_element,
                                                    bool is_clock_propagated)
{
  Vertex *vertex = path_element->vertex(sta_state);
  Pin *pin = vertex->pin();

  bool is_clk = path_element->isClock(sta_state->search());

  Instance *inst = sta_state->network()->instance(pin);

  TimingPathVertex timing_path_vertex{};
  
  timing_path_vertex.instance = sta_state->network()->pathName(inst);
  if (auto net = sta_state->network()->net(pin)) {
    timing_path_vertex.net = sta_state->sdcNetwork()->pathName(net);
  }
  timing_path_vertex.pin = sta_state->cmdNetwork()->pathName(pin);
  timing_path_vertex.cell = sta_state->network()->cellName(inst);
  
  timing_path_vertex.arrival = path_element->arrival();
  if (is_clk && !is_clock_propagated) {
    timing_path_vertex.arrival = 0.0f;
  }

  timing_path_vertex.slew = path_element->slew(sta_state);

  timing_path_vertex.capacitance = sta_state->graphDelayCalc()->loadCap(pin, rise_fall, dcalc_ap);

  const RiseFall* vertex_rise_fall = path_element->transition(sta_state);
  timing_path_vertex.transition = vertex_rise_fall->shortName();

  timing_path_vertex.is_driver = sta_state->network()->isDriver(pin);

  return timing_path_vertex;
}

std::vector<TimingPathVertex> extractTimingPathVertices(const Path *path, int starting_index, const RiseFall *rise_fall)
{
  StaState* sta_state = Sta::sta();

  PathExpanded expanded(path, sta_state);
  std::size_t path_first_index = starting_index;
  std::size_t path_last_index = expanded.size() - 1;

  bool is_clock_propagated = path->clkInfo(sta_state->search())->isPropagated();
  DcalcAnalysisPt *dcalc_ap = path->pathAnalysisPt(sta_state)->dcalcAnalysisPt();

  std::vector<TimingPathVertex> vertices;
  vertices.reserve(path_last_index - path_first_index + 1);
  for (std::size_t index = path_first_index; index <= path_last_index; ++index) {
    const Path *path_element = expanded.path(index);

    bool is_clk = path_element->isClock(sta_state->search());
    
    if (is_clk && !is_clock_propagated && index != path_first_index && index != path_last_index) {
      continue;
    }

    TimingPathVertex timing_path_vertex = extractTimingPathVertexDescription(sta_state, rise_fall, dcalc_ap, path_element, is_clock_propagated);
    vertices.emplace_back(timing_path_vertex);
  }

  return vertices;
}

std::vector<TimingPathVertex> extractDataRequiredTimingPathVertices(const Path *path, const RiseFall *rise_fall)
{
  StaState* sta_state = Sta::sta();

  PathExpanded expanded(path, sta_state);
  std::size_t path_last_index = expanded.size() - 1;
  
  bool is_clock_propagated = path->clkInfo(sta_state->search())->isPropagated();
  std::size_t path_first_index = is_clock_propagated ? 1 : path_last_index;

  DcalcAnalysisPt *dcalc_ap = path->pathAnalysisPt(sta_state)->dcalcAnalysisPt();

  std::vector<TimingPathVertex> vertices;
  vertices.reserve(path_last_index - path_first_index + 1);
  for (std::size_t i = path_first_index; i <= path_last_index; ++i) {
    const Path *path_element = expanded.path(i);
    TimingPathVertex timing_path_vertex = extractTimingPathVertexDescription(sta_state, rise_fall, dcalc_ap, path_element, is_clock_propagated);
    vertices.emplace_back(timing_path_vertex);
  }

  return vertices;
}

class MakeEndTimingArcs : public PathEndVisitor
{
public:
  MakeEndTimingArcs(Sta *sta, bool export_paths);
  MakeEndTimingArcs(const MakeEndTimingArcs&) = default;
  virtual ~MakeEndTimingArcs() {}
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  void setInputRf(const RiseFall *input_rf);
  const ClockEdgeDelays &margins() const { return margins_; }
  const InputRegisterTimingPaths& extractedTimingPaths() const { return timing_paths_; }

private:
  const RiseFall *input_rf_;
  ClockEdgeDelays margins_;
  InputRegisterTimingPaths timing_paths_;
  Sta *sta_;
  bool write_timing_paths_;
};

MakeEndTimingArcs::MakeEndTimingArcs(Sta *sta, bool export_paths) :
  input_rf_(nullptr),
  sta_(sta),
  write_timing_paths_(export_paths)
{
}

PathEndVisitor *
MakeEndTimingArcs::copy() const
{
  return new MakeEndTimingArcs(*this);
}

void
MakeEndTimingArcs::setInputRf(const RiseFall *input_rf)
{
  input_rf_ = input_rf;
}

InputRegisterTimingPath
extractInputRegisterTimingPath(PathEnd *path_end, const RiseFall *input_rf)
{
  StaState* sta_state = Sta::sta();

  InputRegisterTimingPath input_register_timing_path{};

  const PathExpanded path_expanded{path_end->path(), sta_state};
  
  const Path *source_clock_path = path_expanded.clkPath();
  int starting_index = 0;
  if (source_clock_path) {
    const RiseFall *source_clock_transition = source_clock_path->transition(sta_state);
    input_register_timing_path.source_clock_path.time = source_clock_path->arrival();
    input_register_timing_path.source_clock_path.vertices = extractDataRequiredTimingPathVertices(source_clock_path, source_clock_transition);
    input_register_timing_path.source_clock_path.name = TimingPath::Names::SOURCE_CLOCK.at(source_clock_transition->index());

    const ClockEdge *clk_edge = source_clock_path->clkEdge(sta_state);
    input_register_timing_path.source_clk_time = clk_edge->time();
    input_register_timing_path.source_clk_arrival = sta_state->search()->clkPathArrival(source_clock_path);

    input_register_timing_path.is_source_clock_propagated = source_clock_path->clkInfo(sta_state->search())->isPropagated();

    starting_index = path_expanded.startIndex();
  }

  input_register_timing_path.data_arrival_path.vertices = extractTimingPathVertices(path_expanded.endPath(), starting_index, input_rf);
  input_register_timing_path.data_arrival_path.time = path_end->dataArrivalTime(sta_state);
  input_register_timing_path.data_arrival_path.rise_fall = input_rf;
  input_register_timing_path.data_arrival_path.name = TimingPath::Names::DATA_ARRIVAL.at(input_rf->index());

  const Path *target_clock_path = path_end->targetClkPath();
  input_register_timing_path.data_required_path.vertices = extractDataRequiredTimingPathVertices(target_clock_path, input_rf);
  input_register_timing_path.data_required_path.time = path_end->requiredTime(sta_state);
  input_register_timing_path.data_required_path.rise_fall = input_rf;
  input_register_timing_path.data_required_path.name = TimingPath::Names::DATA_REQUIRED.at(input_rf->index());

  input_register_timing_path.target_clk_offset = path_end->targetClkOffset(sta_state);
  input_register_timing_path.target_clk_mcp_adjustment = path_end->targetClkMcpAdjustment(sta_state);
  input_register_timing_path.target_clk_insertion_delay = path_end->targetClkInsertionDelay(sta_state);
  input_register_timing_path.target_clk_delay = path_end->targetClkDelay(sta_state);
  input_register_timing_path.target_clk_non_inter_uncertainty = path_end->targetNonInterClkUncertainty(sta_state);
  input_register_timing_path.target_clk_uncertainty = path_end->interClkUncertainty(sta_state);

  const Path *clk_path = path_end->targetClkPath();
  ClkInfo *clk_info = clk_path->clkInfo(sta_state);
  const Pin *src_pin = clk_info->clkSrc();
  const ClockEdge *clk_edge = clk_info->clkEdge();
  const Clock *clk = clk_edge->clock();
  const RiseFall *clk_rf = clk_edge->transition();
  PathAnalysisPt *path_ap = clk_path->pathAnalysisPt(sta_state);
  const MinMax *min_max = path_ap->pathMinMax();
  const TimingRole *check_role = path_end->checkRole(sta_state);
  const EarlyLate *early_late = check_role->tgtClkEarlyLate();
  Arrival path_insertion = sta_state->search()->clockInsertion(clk, src_pin, clk_rf, min_max, min_max, path_ap);
  Arrival tgt_insertion = sta_state->search()->clockInsertion(clk, src_pin, clk_rf, min_max, early_late, path_ap);
  input_register_timing_path.target_clk_insertion_offset = delayAsFloat(tgt_insertion - path_insertion);

  input_register_timing_path.slack = delayAsFloat(path_end->slack(sta_state), early_late, sta_state);
  input_register_timing_path.crpr = delayAsFloat(path_end->checkCrpr(sta_state));

  float target_clk_delay = path_end->targetClkDelay(sta_state);
  input_register_timing_path.target_clk_delay = delayAsFloat(target_clk_delay);

  float src_offset = path_end->sourceClkOffset(sta_state);
  float clk_time = path_end->targetClkTime(sta_state)
    + path_end->targetClkMcpAdjustment(sta_state)
    + src_offset;
  Arrival clk_arrival = clk_time + target_clk_delay;
  input_register_timing_path.target_clk_time = delayAsFloat(clk_time);
  input_register_timing_path.clk_arrival = delayAsFloat(clk_arrival);

  input_register_timing_path.is_target_clock_propagated = target_clock_path->clkInfo(sta_state->search())->isPropagated();

  input_register_timing_path.library_setup_time = path_end->margin(sta_state);
  input_register_timing_path.path_group_name = sta_state->search()->pathGroup(path_end)->name();
  input_register_timing_path.path_type = path_end->minMax(sta_state)->to_string();
  input_register_timing_path.type = path_end->typeName();
  
  const ClockEdge *src_clk_edge = path_end->sourceClkEdge(sta_state);
  input_register_timing_path.source_clock_name = src_clk_edge->clock()->name();
  input_register_timing_path.source_clock_transition = src_clk_edge->transition();

  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_state);
  input_register_timing_path.target_clock_name = tgt_clk_edge->clock()->name();
  input_register_timing_path.target_clock_transition = tgt_clk_edge->transition();

  const PathDelay *path_delay = path_end->pathDelay();
  if (path_delay) {
    input_register_timing_path.path_delay = path_delay->delay();
    input_register_timing_path.has_path_delay = true;
  }

  return input_register_timing_path;
}

void
MakeEndTimingArcs::visit(PathEnd *path_end)
{
  Path *src_path = path_end->path();
  const Clock *src_clk = src_path->clock(sta_);
  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_);
  if (src_clk == sta_->sdc()->defaultArrivalClock()
      && tgt_clk_edge) {
    Network *network = sta_->network();
    Debug *debug = sta_->debug();
    const MinMax *min_max = path_end->minMax(sta_);
    Arrival data_delay = src_path->arrival();
    Delay clk_latency = path_end->targetClkDelay(sta_);
    ArcDelay check_margin = path_end->margin(sta_);
    Delay margin = min_max == MinMax::max()
      ? data_delay - clk_latency + check_margin
      : clk_latency - data_delay + check_margin;
    float delay1 = delayAsFloat(margin, MinMax::max(), sta_);
    debugPrint(debug, "make_timing_model", 2, "%s -> %s clock %s %s %s %s",
               input_rf_->shortName(),
               network->pathName(src_path->pin(sta_)),
               tgt_clk_edge->name(),
               path_end->typeName(),
               min_max->to_string().c_str(),
               delayAsString(margin, sta_));
    if (debug->check("make_timing_model", 3))
      sta_->reportPathEnd(path_end);

    if (write_timing_paths_) {
      InputRegisterTimingPath timing_path = extractInputRegisterTimingPath(path_end, input_rf_);
      if (timing_path.slack < timing_paths_[min_max->index()][input_rf_->index()].slack) {
        timing_paths_[min_max->index()][input_rf_->index()] = std::move(timing_path);
      }
    }

    RiseFallMinMax &margins = margins_[tgt_clk_edge];
    float max_margin;
    bool max_exists;
    margins.value(input_rf_, min_max, max_margin, max_exists);
    // Always max margin, even for min/hold checks.
    margins.setValue(input_rf_, min_max,
                     max_exists ? max(max_margin, delay1) : delay1);
  }
}

// input -> register setup/hold
// input -> output combinational paths
// Use default input arrival (set_input_delay with no clock) from inputs
// to find downstream register checks and output ports.
void
MakeTimingModel::findTimingFromInputs()
{
  search_->deleteFilteredArrivals();

  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortBitIterator *port_iter = network_->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *input_port = port_iter->next();
    if (network_->direction(input_port)->isInput())
      findTimingFromInput(input_port);
  }
  delete port_iter;
}

void
MakeTimingModel::findTimingFromInput(Port *input_port)
{
  Instance *top_inst = network_->topInstance();
  Pin *input_pin = network_->findPin(top_inst, input_port);
  if (!sta_->isClockSrc(input_pin)) {
    MakeEndTimingArcs end_visitor(sta_, write_timing_paths_);
    OutputPinDelays output_delays;
    CombinationalTimingPaths timing_paths;
    for (const RiseFall *input_rf : RiseFall::range()) {
      const RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
      sta_->setInputDelay(input_pin, input_rf1,
                          sdc_->defaultArrivalClock(),
                          sdc_->defaultArrivalClockEdge()->transition(),
                          nullptr, false, false, MinMaxAll::all(), true, 0.0);

      PinSet *from_pins = new PinSet(network_);
      from_pins->insert(input_pin);
      ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                    input_rf1);
      search_->findFilteredArrivals(from, nullptr, nullptr, false, false);

      end_visitor.setInputRf(input_rf);
      VertexSeq endpoints = search_->filteredEndpoints();
      VisitPathEnds visit_ends(sta_);
      for (Vertex *end : endpoints)
        visit_ends.visitPathEnds(end, corner_, MinMaxAll::all(), true, &end_visitor);
      findOutputDelays(input_rf, output_delays, timing_paths);
      search_->deleteFilteredArrivals();

      sta_->removeInputDelay(input_pin, input_rf1,
                             sdc_->defaultArrivalClock(),
                             sdc_->defaultArrivalClockEdge()->transition(),
                             MinMaxAll::all());
    }
    makeSetupHoldTimingArcs(input_pin, end_visitor.margins(), end_visitor.extractedTimingPaths());
    makeInputOutputTimingArcs(input_pin, output_delays, timing_paths);
  }
}

void
MakeTimingModel::findOutputDelays(const RiseFall *input_rf,
                                  OutputPinDelays &output_pin_delays,
                                  CombinationalTimingPaths &combinational_timing_paths)
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();
    if (network_->direction(output_pin)->isOutput()) {
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      VertexPathIterator path_iter(output_vertex, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        if (search_->matchesFilter(path, nullptr)) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival();
          OutputDelays &delays = output_pin_delays[output_pin];
          delays.delays.mergeValue(output_rf, min_max,
                                   delayAsFloat(delay, min_max, sta_));
          delays.rf_path_exists[input_rf->index()][output_rf->index()] = true;
          
          CombinationalTimingPath &timing_path = combinational_timing_paths[output_pin][input_rf->index()];

          Clock* clock = sdc_->clocks()->front();
          float slack = clock->period() - delay;
          if (write_timing_paths_ && slack < timing_path.slack) {
            timing_path.slack = slack;
            timing_path.combinational_delay_path.name = TimingPath::Names::COMBINATIONAL.at(output_rf->index());
            timing_path.combinational_delay_path.rise_fall = input_rf;

            const int starting_index = 0;
            timing_path.combinational_delay_path.vertices = extractTimingPathVertices(path, starting_index, input_rf);
            timing_path.combinational_delay_path.time = path->arrival();
          }
        }
      }
    }
  }
  delete output_iter;
}

void
MakeTimingModel::makeSetupHoldTimingArcs(const Pin *input_pin,
                                         const ClockEdgeDelays &clk_margins,
                                         const InputRegisterTimingPaths& timing_paths)
{
  for (const auto& [clk_edge, margins] : clk_margins) {
    for (const MinMax *min_max : MinMax::range()) {
      bool setup = (min_max == MinMax::max());
      TimingArcAttrsPtr attrs = nullptr;
      for (const RiseFall *input_rf : RiseFall::range()) {
        float margin;
        bool exists;
        margins.value(input_rf, min_max, margin, exists);
        if (exists) {
          debugPrint(debug_, "make_timing_model", 2, "%s %s %s -> clock %s %s",
                     sta_->network()->pathName(input_pin),
                     input_rf->shortName(),
                     min_max == MinMax::max() ? "setup" : "hold",
                     clk_edge->name(),
                     delayAsString(margin, sta_));
          ScaleFactorType scale_type = setup
            ? ScaleFactorType::setup
            : ScaleFactorType::hold;
          TimingModel *check_model = makeScalarCheckModel(margin, scale_type, input_rf);
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          attrs->setModel(input_rf, check_model);

          if (write_timing_paths_) {
            const InputRegisterTimingPath& timing_path = timing_paths[min_max->index()][input_rf->index()];
            attrs->mergeSlack(timing_path.slack);
            attrs->addTimingPath(timing_path.data_arrival_path);
            attrs->addTimingPath(timing_path.data_required_path);
          }
        }
      }
      if (attrs) {
        LibertyPort *input_port = modelPort(input_pin);
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            const TimingRole *role = setup
              ? TimingRole::setup()
              : TimingRole::hold();
            lib_builder_->makeFromTransitionArcs(cell_, clk_port,
                                                 input_port, nullptr,
                                                 clk_rf, role, attrs);
          }
        }
      }
    }
  }
}

void
MakeTimingModel::makeInputOutputTimingArcs(const Pin *input_pin,
                                           OutputPinDelays &output_pin_delays,
                                           CombinationalTimingPaths &combinational_timing_paths)
{
  for (const auto& [output_pin, output_delays] : output_pin_delays) {
    TimingArcAttrsPtr attrs = nullptr;
    for (const RiseFall *output_rf : RiseFall::range()) {
      const MinMax *min_max = MinMax::max();
      float delay;
      bool exists;
      output_delays.delays.value(output_rf, min_max, delay, exists);
      if (exists) {
        debugPrint(debug_, "make_timing_model", 2, "%s -> %s %s delay %s",
                   network_->pathName(input_pin),
                   network_->pathName(output_pin),
                   output_rf->shortName(),
                   delayAsString(delay, sta_));
        TimingModel *gate_model;
        if (scalar_)
          gate_model = makeGateModelScalar(delay, output_rf);
        else
          gate_model = makeGateModelTable(output_pin, delay, output_rf);
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        attrs->setModel(output_rf, gate_model);
        if (write_timing_paths_) {
          attrs->mergeSlack(combinational_timing_paths.at(output_pin)[output_rf->index()].slack);
          attrs->addTimingPath(combinational_timing_paths.at(output_pin)[output_rf->index()].combinational_delay_path);
        }
      }
    }
    if (attrs) {

      LibertyPort *output_port = modelPort(output_pin);
      LibertyPort *input_port = modelPort(input_pin);
      if (output_port && input_port) {
        attrs->setTimingSense(output_delays.timingSense());
        lib_builder_->makeCombinationalArcs(cell_, input_port, output_port,
                                            true, true, attrs);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

// clocked register -> output paths
void
MakeTimingModel::findClkedOutputPaths()
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();
    if (network_->direction(output_pin)->isOutput()) {
      ClockEdgeDelays clk_delays;
      std::unordered_map<const RiseFall*, CombinationalTimingPath> timing_paths;
      LibertyPort *output_port = modelPort(output_pin);
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      VertexPathIterator path_iter(output_vertex, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        const ClockEdge *clk_edge = path->clkEdge(sta_);
        if (clk_edge) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival();
          RiseFallMinMax &delays = clk_delays[clk_edge];
          delays.mergeValue(output_rf, min_max,
                            delayAsFloat(delay, min_max, sta_));

          CombinationalTimingPath timing_path{};

          Arrival arrival = path->arrival();
          Required required = path->required();
          timing_path.slack = arrival - required;
          if (write_timing_paths_ && (timing_paths.count(output_rf) == 0 || timing_path.slack < timing_paths.at(output_rf).slack)) {
            const PathExpanded path_expanded{path, sta_};
            const Path *source_clock_path = path_expanded.clkPath();
            int starting_index = 1;
            if (source_clock_path) {
              const RiseFall *source_clock_transition = source_clock_path->transition(sta_);
              timing_path.source_clock_path.vertices = extractDataRequiredTimingPathVertices(source_clock_path, source_clock_transition);
              timing_path.source_clock_path.name = TimingPath::Names::SOURCE_CLOCK.at(source_clock_transition->index());
              starting_index = path_expanded.startIndex();
            }
            
            timing_path.combinational_delay_path.name = TimingPath::Names::CLOCKED_OUTPUT.at(output_rf->index());
            timing_path.combinational_delay_path.vertices = extractTimingPathVertices(path, starting_index, output_rf);
            timing_path.combinational_delay_path.time = delay;
            timing_path.combinational_delay_path.rise_fall = output_rf;
            timing_paths[output_rf] = std::move(timing_path);
          }
        }
      }
      for (const auto& [clk_edge, delays] : clk_delays) {
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            TimingArcAttrsPtr attrs = nullptr;
            for (const RiseFall *output_rf : RiseFall::range()) {
              float delay = delays.value(output_rf, min_max_) - clk_edge->time();
              TimingModel *gate_model;
              if (scalar_) {
                  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
                  Slew slew = graph_->slew(output_vertex, output_rf, dcalc_ap->index());
                gate_model = makeGateModelScalar(delay, slew, output_rf);
              }
              else
                gate_model = makeGateModelTable(output_pin, delay, output_rf);
              if (attrs == nullptr)
                attrs = std::make_shared<TimingArcAttrs>();
              attrs->setModel(output_rf, gate_model);
              if (write_timing_paths_) {
                attrs->mergeSlack(timing_paths.at(output_rf).slack);
                if (!timing_paths.at(output_rf).source_clock_path.vertices.empty()) {
                  attrs->addTimingPath(timing_paths.at(output_rf).source_clock_path);
                }
                attrs->addTimingPath(timing_paths.at(output_rf).combinational_delay_path);
              }
            }

            if (attrs) {
              lib_builder_->makeFromTransitionArcs(cell_, clk_port,
                                                   output_port, nullptr,
                                                   clk_rf, TimingRole::regClkToQ(),
                                                   attrs);
            }
          }
        }
      }
    }
  }
  delete output_iter;
}

////////////////////////////////////////////////////////////////

class FindRegTimingArcs : public PathEndVisitor
{
public:
  FindRegTimingArcs(Sta *sta, int internal_path_count);
  FindRegTimingArcs(const FindRegTimingArcs&) = default;
  virtual ~FindRegTimingArcs() {}
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  void setInputRf(const RiseFall *input_rf);
  void mergeSlack(const InputRegisterTimingPath &timing_path,
                  const MinMax *min_max,
                  const RiseFall *rise_fall);
  float slack() const { return slack_; }
  const std::set<InputRegisterTimingPath>& timingPaths(const MinMax *min_max,
                                                       const RiseFall *rise_fall);

private:
  const RiseFall *input_rf_;
  Sta *sta_;
  unsigned int internal_path_count_;
  float slack_{std::numeric_limits<float>::max()};
  std::array<std::array<std::set<InputRegisterTimingPath>, 2>, 2> timing_paths_;
};

FindRegTimingArcs::FindRegTimingArcs(Sta *sta, int internal_path_count) :
  input_rf_(nullptr),
  sta_(sta),
  internal_path_count_(internal_path_count)
{
}

PathEndVisitor *
FindRegTimingArcs::copy() const
{
  return new FindRegTimingArcs(*this);
}

void
FindRegTimingArcs::setInputRf(const RiseFall *input_rf)
{
  input_rf_ = input_rf;
}

void
FindRegTimingArcs::visit(PathEnd *path_end)
{
  InputRegisterTimingPath timing_path = extractInputRegisterTimingPath(path_end, input_rf_);
  mergeSlack(timing_path, path_end->minMax(sta_), input_rf_);
}

void
FindRegTimingArcs::mergeSlack(const InputRegisterTimingPath &timing_path,
                              const MinMax *min_max,
                              const RiseFall *rise_fall)
{
  slack_ = std::min(timing_path.slack, slack_);
  auto& paths = timing_paths_.at(min_max->index()).at(rise_fall->index());
  paths.insert(timing_path);
  if (paths.size() > internal_path_count_) {
    paths.erase(std::prev(paths.end()));
  }
}

const std::set<InputRegisterTimingPath>&
FindRegTimingArcs::timingPaths(const MinMax *min_max, const RiseFall *rise_fall)
{
  return timing_paths_.at(min_max->index()).at(rise_fall->index());
}

bool isRegisterInput(const TimingArcSet *timing_arc_set)
{
  return timing_arc_set && timing_arc_set->role() == TimingRole::regClkToQ();
}

void
MakeTimingModel::findWorstInternalPaths()
{
  if (!write_timing_paths_) {
    return;
  }

  search_->deleteFilteredArrivals();

  FindRegTimingArcs end_visitor(sta_, internal_path_count_);
  Instance *top_inst = network_->topInstance();
  LeafInstanceIterator *instance_iterator = network_->leafInstanceIterator(top_inst);
  while (instance_iterator->hasNext()) {
    Instance *instance = instance_iterator->next();
    InstancePinIterator *instance_pin_iterator = network_->pinIterator(instance);

    std::vector<Pin*> input_pins{};
    std::vector<Pin*> output_pins{};

    while (instance_pin_iterator->hasNext()) {
      Pin *instance_pin = instance_pin_iterator->next();
      if (network_->direction(instance_pin)->isInput()) {
        Vertex *vertex = graph_->vertex(network_->vertexId(instance_pin));
        VertexOutEdgeIterator out_edge_iter(vertex, graph_);
        while (out_edge_iter.hasNext()) {
          Edge *out_edge = out_edge_iter.next();
          if (isRegisterInput(out_edge->timingArcSet())) {
            input_pins.emplace_back(instance_pin);
            break;
          }
        }
      } else if (network_->direction(instance_pin)->isOutput()) {
        output_pins.emplace_back(instance_pin);
      }
    }

    if (input_pins.empty()) {
      continue;
    }

    for (auto& register_input_pin : input_pins) {
      for (const RiseFall *input_rf : RiseFall::range()) {
        const RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
        sta_->setInputDelay(register_input_pin, input_rf1,
                            sdc_->defaultArrivalClock(),
                            sdc_->defaultArrivalClockEdge()->transition(),
                            nullptr, false, false, MinMaxAll::all(), true, 0.0);

        PinSet *from_pins = new PinSet(network_);
        from_pins->insert(register_input_pin);
        ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr, RiseFallBoth::rise());

        ExceptionTo *to = sta_->makeExceptionTo(nullptr, nullptr, nullptr, input_rf1, input_rf1);

        search_->findFilteredArrivals(from, nullptr, to, false, false);

        end_visitor.setInputRf(input_rf);
        VertexSeq endpoints = search_->filteredEndpoints();
        VisitPathEnds visit_ends(sta_);
        for (Vertex *end : endpoints) {
          visit_ends.visitPathEnds(end, corner_, MinMaxAll::all(), true, &end_visitor);
        }
        search_->deleteFilteredArrivals();
      }
    }
  }

  for (const MinMax *min_max : MinMax::range()) {
    for (const RiseFall *rise_fall : RiseFall::range()) {
      for (const auto& timing_path : end_visitor.timingPaths(min_max, rise_fall)) {
        cell_->addInternalTimingPath(timing_path, min_max, rise_fall);
      }
    }
  }

  delete instance_iterator;
}

////////////////////////////////////////////////////////////////

void
MakeTimingModel::findClkTreeDelays()
{
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    if (network_->direction(port)->isInput()) {
      const char *port_name = network_->name(port);
      LibertyPort *lib_port = cell_->findLibertyPort(port_name);
      Pin *pin = network_->findPin(top_inst, port);
      if (pin && sdc_->isClock(pin)) {
        lib_port->setIsClock(true);
        ClockSet *clks = sdc_->findClocks(pin);
        if (clks->size() == 1) {
          for (const Clock *clk : *clks) {
            ClkDelays delays = sta_->findClkDelays(clk, true);
            for (const MinMax *min_max : MinMax::range()) {
              makeClkTreePaths(lib_port, min_max, TimingSense::positive_unate, delays);
              makeClkTreePaths(lib_port, min_max, TimingSense::negative_unate, delays);
            }
          }
        }
      }
    }
  }
  delete port_iter;
}

void
MakeTimingModel::makeClkTreePaths(LibertyPort *lib_port,
                                  const MinMax *min_max,
                                  TimingSense sense,
                                  const ClkDelays &delays)
{
  TimingArcAttrsPtr attrs = nullptr;
  for (const RiseFall *clk_rf : RiseFall::range()) {
    const RiseFall *end_rf = (sense == TimingSense::positive_unate)
      ? clk_rf
      : clk_rf->opposite();
    Path clk_path;
    Delay insertion, delay, latency;
    float lib_clk_delay;
    bool exists;
    delays.delay(clk_rf, end_rf, min_max, insertion, delay,
                 lib_clk_delay, latency, clk_path, exists);
    if (exists) {
      TimingModel *model = makeGateModelScalar(delay, end_rf);
      if (attrs == nullptr)
        attrs = std::make_shared<TimingArcAttrs>();
      attrs->setModel(end_rf, model);
    }
  }
  if (attrs) {
    attrs->setTimingSense(sense);
    const TimingRole *role = (min_max == MinMax::min())
      ? TimingRole::clockTreePathMin()
      : TimingRole::clockTreePathMax();
    lib_builder_->makeClockTreePathArcs(cell_, lib_port, role, min_max, attrs);
  }
}

////////////////////////////////////////////////////////////////

LibertyPort *
MakeTimingModel::modelPort(const Pin *pin)
{
  return cell_->findLibertyPort(network_->name(network_->port(pin)));
}

TimingModel *
MakeTimingModel::makeScalarCheckModel(float value,
                                      ScaleFactorType scale_factor_type,
                                      const RiseFall *rf)
{
  TablePtr table = make_shared<Table0>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *table_model = new TableModel(table, tbl_template,
                                           scale_factor_type, rf);
  CheckTableModel *check_model = new CheckTableModel(cell_, table_model, nullptr);
  return check_model;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     Slew slew,
                                     const RiseFall *rf)
{
  TablePtr delay_table = make_shared<Table0>(delayAsFloat(delay));
  TablePtr slew_table = make_shared<Table0>(delayAsFloat(slew));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModel *slew_model = new TableModel(slew_table, tbl_template,
                                          ScaleFactorType::cell, rf);
  GateTableModel *gate_model = new GateTableModel(cell_, delay_model, nullptr,
                                                  slew_model, nullptr,
                                                  nullptr, nullptr);
  return gate_model;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     const RiseFall *rf)
{
  TablePtr delay_table = make_shared<Table0>(delayAsFloat(delay));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  GateTableModel *gate_model = new GateTableModel(cell_, delay_model, nullptr,
                                                  nullptr, nullptr,
                                                  nullptr, nullptr);
  return gate_model;
}

// Eval the driver pin model along its load capacitance
// axis and add the input to output 'delay' to the table values.
TimingModel *
MakeTimingModel::makeGateModelTable(const Pin *output_pin,
                                    Delay delay,
                                    const RiseFall *rf)
{
  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  const Pvt *pvt = dcalc_ap->operatingConditions();
  PinSet *drvrs = network_->drivers(network_->net(network_->term(output_pin)));
  const Pin *drvr_pin = *drvrs->begin();
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  if (drvr_port) {
    const LibertyCell *drvr_cell = drvr_port->libertyCell();
    for (TimingArcSet *arc_set : drvr_cell->timingArcSets(nullptr, drvr_port)) {
      for (TimingArc *drvr_arc : arc_set->arcs()) {
        // Use the first timing arc to simplify life.
        if (drvr_arc->toEdge()->asRiseFall() == rf) {
          const LibertyPort *gate_in_port = drvr_arc->from();
          const Instance *drvr_inst = network_->instance(drvr_pin);
          const Pin *gate_in_pin = network_->findPin(drvr_inst, gate_in_port);
          if (gate_in_pin) {
            Vertex *gate_in_vertex = graph_->pinLoadVertex(gate_in_pin);
            Slew in_slew = graph_->slew(gate_in_vertex,
                                        drvr_arc->fromEdge()->asRiseFall(),
                                        dcalc_ap->index());
            float in_slew1 = delayAsFloat(in_slew);
            GateTableModel *drvr_gate_model = drvr_arc->gateTableModel(dcalc_ap);
            if (drvr_gate_model) {
              float output_load_cap = graph_delay_calc_->loadCap(output_pin, dcalc_ap);
              ArcDelay drvr_self_delay;
              Slew drvr_self_slew;
              drvr_gate_model->gateDelay(pvt, in_slew1, output_load_cap, false,
                                         drvr_self_delay, drvr_self_slew);

              const TableModel *drvr_table = drvr_gate_model->delayModel();
              const TableTemplate *drvr_template = drvr_table->tblTemplate();
              const TableAxis *drvr_load_axis = loadCapacitanceAxis(drvr_table);
              if (drvr_load_axis) {
                const FloatSeq *drvr_axis_values = drvr_load_axis->values();
                FloatSeq *load_values = new FloatSeq;
                FloatSeq *slew_values = new FloatSeq;
                for (size_t i = 0; i < drvr_axis_values->size(); i++) {
                  float load_cap = (*drvr_axis_values)[i];
                  // get slew from driver input pin
                  ArcDelay gate_delay;
                  Slew gate_slew;
                  drvr_gate_model->gateDelay(pvt, in_slew1, load_cap, false,
                                             gate_delay, gate_slew);
                  // Remove the self delay driving the output pin net load cap.
                  load_values->push_back(delayAsFloat(delay + gate_delay
                                                      - drvr_self_delay));
                  slew_values->push_back(delayAsFloat(gate_slew));
                }

                FloatSeq *axis_values = new FloatSeq(*drvr_axis_values);
                TableAxisPtr load_axis =
                  std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance,
                                              axis_values);

                TablePtr delay_table = make_shared<Table1>(load_values, load_axis);
                TablePtr slew_table = make_shared<Table1>(slew_values, load_axis);

                TableTemplate *model_template = ensureTableTemplate(drvr_template,
                                                                    load_axis);
                TableModel *delay_model = new TableModel(delay_table, model_template,
                                                         ScaleFactorType::cell, rf);
                TableModel *slew_model = new TableModel(slew_table, model_template,
                                                        ScaleFactorType::cell, rf);
                GateTableModel *gate_model = new GateTableModel(cell_,
                                                                delay_model, nullptr,
                                                                slew_model, nullptr,
                                                                nullptr, nullptr);
                return gate_model;
              }
            }
          }
        }
      }
    }
  }
  Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
  Slew slew = graph_->slew(output_vertex, rf, dcalc_ap->index());
  return makeGateModelScalar(delay, slew, rf);
}

TableTemplate *
MakeTimingModel::ensureTableTemplate(const TableTemplate *drvr_template,
                                     TableAxisPtr load_axis)
{
  TableTemplate *model_template = template_map_.findKey(drvr_template);
  if (model_template == nullptr) {
    string template_name = "template_";
    template_name += std::to_string(tbl_template_index_++);

    model_template = new TableTemplate(template_name.c_str());
    model_template->setAxis1(load_axis);
    library_->addTableTemplate(model_template, TableTemplateType::delay);
    template_map_[drvr_template] = model_template;
  }
  return model_template;
}

const TableAxis *
MakeTimingModel::loadCapacitanceAxis(const TableModel *table)
{
  if (table->axis1()
      && table->axis1()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis1();
  else if (table->axis2()
           && table->axis2()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis2();
  else if (table->axis3()
           && table->axis3()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis3();
  else
    return nullptr;
}

OutputDelays::OutputDelays()
{
  rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()] = false;
}

TimingSense
OutputDelays::timingSense() const
{
  if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           && rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()]
           && !rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           && !rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()])
    return TimingSense::positive_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           && rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::negative_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::non_unate;
  else
    return TimingSense::none;
}

} // namespace
