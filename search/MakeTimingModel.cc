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

#include "MakeTimingModel.hh"
#include "MakeTimingModelPvt.hh"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ArcDelayCalc.hh"
#include "ClkDelays.hh"
#include "ClkInfo.hh"
#include "Clock.hh"
#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Delay.hh"
#include "Graph.hh"
#include "GraphClass.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "LibertyClass.hh"
#include "Network.hh"
#include "NetworkClass.hh"
#include "Path.hh"
#include "PathEnd.hh"
#include "PortDirection.hh"
#include "RiseFallMinMax.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "SdcClass.hh"
#include "Search.hh"
#include "Sta.hh"
#include "StaState.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "Units.hh"
#include "VisitPathEnds.hh"
#include "liberty/LibertyBuilder.hh"

namespace sta {

LibertyLibrary *
makeTimingModel(std::string_view lib_name,
                std::string_view cell_name,
                std::string_view filename,
                const Scene *scene,
                const bool scalar,
                const bool internal_paths,
                Sta *sta)
{
  MakeTimingModel maker(lib_name, cell_name, filename, scene, scalar,
                        internal_paths, sta);
  return maker.makeTimingModel();
}

MakeTimingModel::MakeTimingModel(std::string_view lib_name,
                                 std::string_view cell_name,
                                 std::string_view filename,
                                 const Scene *scene,
                                 const bool scalar,
                                 const bool internal_paths,
                                 Sta *sta) :
  StaState(sta),
  lib_name_(lib_name),
  cell_name_(cell_name),
  filename_(filename),
  scene_(scene),
  scalar_(scalar),
  internal_paths_(internal_paths),
  cell_(nullptr),
  min_max_(MinMax::max()),
  lib_builder_(new LibertyBuilder(debug_,
                                  report_)),
  sdc_(scene->sdc()),
  sta_(sta)
{
  scenes_.insert(scene_);
}

MakeTimingModel::~MakeTimingModel() { delete lib_builder_; }

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

  findTimingFromInputs();
  search_->findAllArrivals();
  findClkedOutputPaths();
  if (internal_paths_)
    findInternalPaths();
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
  sdc_backup_ = new Sdc(sdc_->mode(), this);
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
  const LibertySeq &scene_libs = scene_->libertyLibraries(MinMax::max());
  if (!scene_libs.empty()) {
    const LibertyLibrary *scene_lib = scene_libs[0];
    *library_->units() = *scene_lib->units();

    for (const RiseFall *rf : RiseFall::range()) {
      library_->setInputThreshold(rf, scene_lib->inputThreshold(rf));
      library_->setOutputThreshold(rf, scene_lib->outputThreshold(rf));
      library_->setSlewLowerThreshold(rf, scene_lib->slewLowerThreshold(rf));
      library_->setSlewUpperThreshold(rf, scene_lib->slewUpperThreshold(rf));
    }

    library_->setDelayModelType(scene_lib->delayModelType());
    library_->setNominalProcess(scene_lib->nominalProcess());
    library_->setNominalVoltage(scene_lib->nominalVoltage());
    library_->setNominalTemperature(scene_lib->nominalTemperature());
  }
  else
    report_->error(1381, "scene {} has no liberty libraries.", scene_->name());
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
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    std::string port_name(network_->name(port));
    if (network_->isBus(port)) {
      int from_index = network_->fromIndex(port);
      int to_index = network_->toIndex(port);
      BusDcl *bus_dcl = library_->makeBusDcl(port_name, from_index, to_index);
      LibertyPort *lib_port =
          lib_builder_->makeBusPort(cell_, port_name, from_index, to_index, bus_dcl);
      lib_port->setDirection(network_->direction(port));
      PortMemberIterator *member_iter = network_->memberIterator(port);
      while (member_iter->hasNext()) {
        Port *bit_port = member_iter->next();
        Pin *pin = network_->findPin(top_inst, bit_port);
        LibertyPort *lib_bit_port = modelPort(pin);
        float load_cap = graph_delay_calc_->loadCap(pin, scene_, min_max_);
        lib_bit_port->setCapacitance(load_cap);
        setPortLimits(pin, lib_bit_port);
      }
      delete member_iter;
    }
    else {
      LibertyPort *lib_port = lib_builder_->makePort(cell_, port_name);
      lib_port->setDirection(network_->direction(port));
      Pin *pin = network_->findPin(top_inst, port);
      float load_cap = graph_delay_calc_->loadCap(pin, scene_, min_max_);
      lib_port->setCapacitance(load_cap);
      setPortLimits(pin, lib_port);
    }
  }
  delete port_iter;
}

// Find port max_cap/max_slew limits.
void
MakeTimingModel::setPortLimits(const Pin *pin,
                               LibertyPort *lib_port)
{
  const PortDirection *dir = network_->direction(pin);
  float slew_limit = std::numeric_limits<float>::max();
  float cap_limit = std::numeric_limits<float>::max();
  bool slew_exists = false;
  bool cap_exists = false;
  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(pin);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      const LibertyLibrary *lib = port->libertyCell()->libertyLibrary();
      float limit;
      bool exists;
      port->slewLimit(min_max_, limit, exists);
      if (!exists)
        lib->defaultMaxSlew(limit, exists);
      if (exists && limit < slew_limit) {
        slew_limit = limit;
        slew_exists = true;
      }

      if (dir->isAnyOutput()) {
        port->capacitanceLimit(min_max_, limit, exists);
        if (!exists)
          lib->defaultMaxCapacitance(limit, exists);
        if (exists && limit < cap_limit) {
          cap_limit = limit;
          cap_exists = true;
        }
      }
    }
  }
  delete pin_iter;
  if (slew_exists)
    lib_port->setSlewLimit(slew_limit, min_max_);
  if (cap_exists)
    lib_port->setCapacitanceLimit(cap_limit, min_max_);
}

void
MakeTimingModel::checkClock(Clock *clk)
{
  for (const Pin *pin : clk->leafPins()) {
    if (!network_->isTopLevelPort(pin))
      report_->warn(1380, "clock {} pin {} is inside model block.", clk->name(),
                    network_->pathName(pin));
  }
}

////////////////////////////////////////////////////////////////

class MakeEndTimingArcs : public PathEndVisitor
{
public:
  MakeEndTimingArcs(LibertyPort *input_port,
                    MakeTimingModel *internal_model,
                    Sta *sta);
  MakeEndTimingArcs(const MakeEndTimingArcs &) = default;
  PathEndVisitor *copy() const override;
  void visit(PathEnd *path_end) override;
  void setInputRf(const RiseFall *input_rf);
  const ClockEdgeDelays &margins() const { return margins_; }

private:
  LibertyPort *input_port_;
  MakeTimingModel *internal_model_;
  const RiseFall *input_rf_{nullptr};
  ClockEdgeDelays margins_;
  Sta *sta_;
};

MakeEndTimingArcs::MakeEndTimingArcs(LibertyPort *input_port,
                                     MakeTimingModel *internal_model,
                                     Sta *sta) :
  input_port_(input_port),
  internal_model_(internal_model),
  sta_(sta)
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

void
MakeEndTimingArcs::visit(PathEnd *path_end)
{
  Path *src_path = path_end->path();
  const Sdc *sdc = src_path->sdc(sta_);
  const Clock *src_clk = src_path->clock(sta_);
  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_);
  if (src_clk == sdc->defaultArrivalClock() && tgt_clk_edge) {
    if (internal_model_
        && internal_model_->recordInternalPath(path_end,
                                               {input_port_, input_rf_, true}))
      return;
    Network *network = sta_->network();
    Debug *debug = sta_->debug();
    const MinMax *min_max = path_end->minMax(sta_);
    Arrival data_delay = src_path->arrival();
    Delay clk_latency = path_end->targetClkDelay(sta_);
    ArcDelay check_margin = path_end->margin(sta_);
    Delay margin = (min_max == MinMax::max())
      ? delaySum(delayDiff(data_delay, clk_latency, sta_), check_margin, sta_)
      : delaySum(delayDiff(clk_latency, data_delay, sta_), check_margin, sta_);
    float delay1 = delayAsFloat(margin, MinMax::max(), sta_);
    debugPrint(debug, "make_timing_model", 2, "{} -> {} clock {} {} {} {}",
               input_rf_->shortName(), network->pathName(src_path->pin(sta_)),
               tgt_clk_edge->name(), path_end->typeName(),
               min_max->to_string(), delayAsString(margin, sta_));
    if (debug->check("make_timing_model", 3))
      sta_->reportPathEnd(path_end);

    RiseFallMinMax &margins = margins_[tgt_clk_edge];
    float max_margin;
    bool max_exists;
    margins.value(input_rf_, min_max, max_margin, max_exists);
    // Always max margin, even for min/hold checks.
    margins.setValue(input_rf_, min_max,
                     max_exists ? std::max(max_margin, delay1) : delay1);
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
  if (!sdc_->isClock(input_pin)) {
    MakeEndTimingArcs end_visitor(modelPort(input_pin),
                                  internal_paths_ ? this : nullptr, sta_);
    OutputPinDelays output_delays;
    for (const RiseFall *input_rf : RiseFall::range()) {
      const RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
      sta_->setInputDelay(input_pin, input_rf1, sdc_->defaultArrivalClock(),
                          sdc_->defaultArrivalClockEdge()->transition(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0, sdc_);

      PinSet *from_pins = new PinSet(network_);
      from_pins->insert(input_pin);
      ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr,
                                                    nullptr, input_rf1, sdc_);
      search_->findFilteredArrivals(from, nullptr, nullptr, false, false);

      end_visitor.setInputRf(input_rf);
      VertexSeq endpoints = search_->filteredEndpoints();
      VisitPathEnds visit_ends(sta_);
      for (Vertex *end : endpoints)
        visit_ends.visitPathEnds(end, scenes_, MinMaxAll::all(), true, &end_visitor);
      findOutputDelays(input_rf, output_delays);
      search_->deleteFilteredArrivals();

      sta_->removeInputDelay(input_pin, input_rf1, sdc_->defaultArrivalClock(),
                             sdc_->defaultArrivalClockEdge()->transition(),
                             MinMaxAll::all(), sdc_);
    }
    makeSetupHoldTimingArcs(input_pin, end_visitor.margins());
    makeInputOutputTimingArcs(input_pin, output_delays);
  }
}

void
MakeTimingModel::findOutputDelays(const RiseFall *input_rf,
                                  OutputPinDelays &output_pin_delays)
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
        }
      }
    }
  }
  delete output_iter;
}

void
MakeTimingModel::makeSetupHoldTimingArcs(const Pin *input_pin,
                                         const ClockEdgeDelays &clk_margins)
{
  for (const auto &[clk_edge, margins] : clk_margins) {
    for (const MinMax *min_max : MinMax::range()) {
      bool setup = (min_max == MinMax::max());
      TimingArcAttrsPtr attrs = nullptr;
      for (const RiseFall *input_rf : RiseFall::range()) {
        float margin;
        bool exists;
        margins.value(input_rf, min_max, margin, exists);
        if (exists) {
          debugPrint(debug_, "make_timing_model", 2, "{} {} {} -> clock {} {}",
                     sta_->network()->pathName(input_pin), input_rf->shortName(),
                     min_max == MinMax::max() ? "setup" : "hold", clk_edge->name(),
                     delayAsString(margin, sta_));
          ScaleFactorType scale_type =
              setup ? ScaleFactorType::setup : ScaleFactorType::hold;
          TimingModel *check_model =
              makeScalarCheckModel(margin, scale_type, input_rf);
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          attrs->setModel(input_rf, check_model);
        }
      }
      if (attrs) {
        LibertyPort *input_port = modelPort(input_pin);
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            const TimingRole *role =
                setup ? TimingRole::setup() : TimingRole::hold();
            lib_builder_->makeFromTransitionArcs(cell_, clk_port, input_port,
                                                 nullptr, clk_rf, role, attrs);
          }
        }
      }
    }
  }
}

static bool
sameTableModel(const TableModel *model1,
               const TableModel *model2)
{
  if (model1 == nullptr || model2 == nullptr)
    return model1 == model2;
  const Table *table1 = model1->table().get();
  const Table *table2 = model2->table().get();
  if (table1->order() != table2->order())
    return false;
  const TableAxis *axes1[] = {table1->axis1(), table1->axis2(), table1->axis3()};
  const TableAxis *axes2[] = {table2->axis1(), table2->axis2(), table2->axis3()};
  size_t sizes[] = {1, 1, 1};
  for (int i = 0; i < table1->order(); i++) {
    if (axes1[i]->variable() != axes2[i]->variable()
        || axes1[i]->values() != axes2[i]->values())
      return false;
    sizes[i] = axes1[i]->size();
  }
  for (size_t i1 = 0; i1 < sizes[0]; i1++) {
    for (size_t i2 = 0; i2 < sizes[1]; i2++) {
      for (size_t i3 = 0; i3 < sizes[2]; i3++) {
        if (table1->value(i1, i2, i3) != table2->value(i1, i2, i3))
          return false;
      }
    }
  }
  return true;
}

// Retaining (min) model unless it is the same as the max model.
static void
setDistinctRetainModel(TimingArcAttrs *attrs,
                       const RiseFall *rf,
                       TimingModel *min_model)
{
  auto max_gate = dynamic_cast<const GateTableModel*>(attrs->model(rf));
  auto min_gate = dynamic_cast<const GateTableModel*>(min_model);
  if (max_gate && min_gate
      && sameTableModel(max_gate->delayModel(), min_gate->delayModel())
      && sameTableModel(max_gate->slewModel(), min_gate->slewModel()))
    delete min_model;
  else
    attrs->setRetainModel(rf, min_model);
}

void
MakeTimingModel::makeInputOutputTimingArcs(const Pin *input_pin,
                                           OutputPinDelays &output_pin_delays)
{
  for (const auto &[output_pin, output_delays] : output_pin_delays) {
    TimingArcAttrsPtr attrs = nullptr;
    for (const RiseFall *output_rf : RiseFall::range()) {
      float delay;
      bool exists;
      output_delays.delays.value(output_rf, MinMax::max(), delay, exists);
      if (exists) {
        debugPrint(debug_, "make_timing_model", 2, "{} -> {} {} delay {}",
                   network_->pathName(input_pin), network_->pathName(output_pin),
                   output_rf->shortName(), delayAsString(delay, sta_));
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        attrs->setModel(output_rf, makeOutputGateModel(output_pin, delay, output_rf,
                                                       MinMax::max()));
        // Shortest path delay for min analysis.
        float min_delay;
        output_delays.delays.value(output_rf, MinMax::min(), min_delay, exists);
        if (exists)
          setDistinctRetainModel(attrs.get(), output_rf,
                                 makeOutputGateModel(output_pin, min_delay,
                                                     output_rf, MinMax::min()));
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
      LibertyPort *output_port = modelPort(output_pin);
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      VertexPathIterator path_iter(output_vertex, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        const ClockEdge *clk_edge = path->clkEdge(sta_);
        if (clk_edge) {
          if (internal_paths_ && recordInternalOutputPath(output_pin, path))
            continue;
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival();
          RiseFallMinMax &delays = clk_delays[clk_edge];
          delays.mergeValue(output_rf, min_max, delayAsFloat(delay, min_max, sta_));
        }
      }
      for (const auto &[clk_edge, delays] : clk_delays) {
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            TimingArcAttrsPtr attrs = nullptr;
            for (const RiseFall *output_rf : RiseFall::range()) {
              float max_arrival, min_arrival;
              bool max_exists, min_exists;
              delays.value(output_rf, min_max_, max_arrival, max_exists);
              delays.value(output_rf, MinMax::min(), min_arrival, min_exists);
              if (!max_exists && !min_exists)
                continue;
              float delay = (max_exists ? max_arrival : min_arrival) - clk_edge->time();
              if (attrs == nullptr)
                attrs = std::make_shared<TimingArcAttrs>();
              attrs->setModel(output_rf, makeOutputGateModel(output_pin, delay,
                                                             output_rf, min_max_));
              // Shortest path delay for min analysis.
              if (min_exists) {
                float min_delay = min_arrival - clk_edge->time();
                setDistinctRetainModel(attrs.get(), output_rf,
                                       makeOutputGateModel(output_pin, min_delay,
                                                           output_rf,
                                                           MinMax::min()));
              }
            }
            if (attrs) {
              lib_builder_->makeFromTransitionArcs(cell_, clk_port, output_port,
                                                   nullptr, clk_rf,
                                                   TimingRole::regClkToQ(), attrs);
            }
          }
        }
      }
    }
  }
  delete output_iter;
}

////////////////////////////////////////////////////////////////

bool
InternalClkEdge::operator<(const InternalClkEdge &clk_edge) const
{
  int cmp = port->name().compare(clk_edge.port->name());
  return cmp < 0
    || (cmp == 0 && rf->index() < clk_edge.rf->index());
}

bool
InternalLaunch::operator<(const InternalLaunch &launch) const
{
  if (from_input != launch.from_input)
    return !from_input;
  int cmp = port->name().compare(launch.port->name());
  return cmp < 0
    || (cmp == 0 && rf->index() < launch.rf->index());
}

// Register -> register paths inside the block.
class MakeInternalPathArcs : public PathEndVisitor
{
public:
  MakeInternalPathArcs(MakeTimingModel *model,
                       Sta *sta);
  MakeInternalPathArcs(const MakeInternalPathArcs &) = default;
  PathEndVisitor *copy() const override;
  void visit(PathEnd *path_end) override;

private:
  MakeTimingModel *model_;
  Sta *sta_;
};

MakeInternalPathArcs::MakeInternalPathArcs(MakeTimingModel *model,
                                           Sta *sta) :
  model_(model),
  sta_(sta)
{
}

PathEndVisitor *
MakeInternalPathArcs::copy() const
{
  return new MakeInternalPathArcs(*this);
}

void
MakeInternalPathArcs::visit(PathEnd *path_end)
{
  Path *path = path_end->path();
  const Sdc *sdc = path->sdc(sta_);
  const Clock *src_clk = path->clock(sta_);
  if (src_clk == nullptr
      || src_clk == sdc->defaultArrivalClock())
    return;

  // Paths from data inputs are found by the input searches.
  const Path *start = path;
  while (start->prevPath())
    start = start->prevPath();
  const Pin *start_pin = start->pin(sta_);
  if (sta_->network()->isTopLevelPort(start_pin)
      && !sdc->isClock(start_pin))
    return;

  LibertyPort *launch_clk_port = model_->clkModelPort(path->clkInfo(sta_)->clkSrc());
  const RiseFall *launch_clk_rf = path_end->sourceClkEdge(sta_)->transition();
  model_->recordInternalPath(path_end, {launch_clk_port, launch_clk_rf, false});
}

// Endpoint path in the block, so the internal pin has the same
// path as the endpoint in the flat design.
static std::string
internalPinName(const Pin *pin,
                const StaState *sta)
{
  const Network *sdc_network = sta->sdcNetwork();
  std::string path_name = sdc_network->pathName(pin);
  std::string name;
  for (char ch : path_name) {
    if (ch != sdc_network->pathEscape())
      name += ch;
  }
  return portLibertyToSta(name);
}

// Register output of the clock -> Q arc that launches path.
static const Path *
launcherPath(const Path *path,
             const StaState *sta)
{
  for (const Path *p = path; p; p = p->prevPath()) {
    const TimingArc *prev_arc = p->prevArc(sta);
    if (prev_arc
        && prev_arc->role()->genericRole() == TimingRole::regClkToQ())
      return p;
  }
  return nullptr;
}

// Model port for a clock defined on a block input port.
LibertyPort *
MakeTimingModel::clkModelPort(const Pin *clk_src)
{
  if (clk_src && network_->isTopLevelPort(clk_src))
    return modelPort(clk_src);
  return nullptr;
}

bool
MakeTimingModel::recordInternalPath(const PathEnd *path_end,
                                    const InternalLaunch &launch)
{
  PathEnd::Type type = path_end->type();
  if (type == PathEnd::Type::latch_check) {
    if (!warned_internal_latch_)
      report_->warn(1382, "latch paths inside the model block are ignored.");
    warned_internal_latch_ = true;
    return false;
  }
  if (!(type == PathEnd::Type::check
        || type == PathEnd::Type::gated_clk))
    return false;

  const Path *path = path_end->path();
  const Path *tgt_clk_path = path_end->targetClkPath();
  LibertyPort *capture_clk_port = tgt_clk_path
    ? clkModelPort(tgt_clk_path->clkInfo(this)->clkSrc())
    : nullptr;
  if (launch.port == nullptr || capture_clk_port == nullptr) {
    if (!warned_internal_clk_)
      report_->warn(1383, "paths clocked by clocks defined inside the model block are ignored.");
    warned_internal_clk_ = true;
    return false;
  }
  if (path_end->multiCyclePath() && !warned_internal_mcp_) {
    report_->warn(1384, "multicycle paths inside the model block are ignored.");
    warned_internal_mcp_ = true;
  }

  const MinMax *min_max = path_end->minMax(this);
  const ClockEdge *src_clk_edge = path_end->sourceClkEdge(this);
  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(this);
  bool setup = (path_end->checkGenericRole(this) == TimingRole::setup());
  // Launch clock port edge or input port to the endpoint.
  float launch_delay = delayAsFloat(path->arrival(), min_max, this)
    - src_clk_edge->time();
  // Required time relative to the capture clock port edge without the
  // block's cycle accounting, uncertainty and exceptions.
  Required required = path_end->requiredTime(this);
  float required_offset = delayAsFloat(required, min_max, this)
    - path_end->targetClkTime(this)
    - path_end->targetClkUncertainty(this)
    - path_end->targetClkMcpAdjustment(this)
    - path_end->targetClkPathMargin(this);
  float check_margin = setup ? -required_offset : required_offset;

  InternalClkEdge capture{capture_clk_port, tgt_clk_edge->transition()};
  std::string pin_name = internalPinName(path->pin(this), this);
  int rf_index = path_end->transition(this)->index();
  InternalPathDelay &delay =
    internal_endpoint_paths_[pin_name][launch][capture][min_max->index()][rf_index];
  // Keep the path with the worst slack.
  float violation = setup
    ? launch_delay + check_margin
    : check_margin - launch_delay;
  float worst_violation = setup
    ? delay.launch_delay + delay.check_margin
    : delay.check_margin - delay.launch_delay;
  if (!delay.exists || violation > worst_violation) {
    delay.launch_delay = launch_delay;
    delay.check_margin = check_margin;
    delay.slew = delayAsFloat(path->slew(this), min_max, this);
    delay.check_role = path_end->checkRole(this);
    delay.launcher.clear();
    if (!launch.from_input) {
      const Path *launcher = launcherPath(path, this);
      if (launcher) {
        delay.launcher = internalPinName(launcher->pin(this), this);
        delay.launcher_delay = delayAsFloat(launcher->arrival(), min_max, this)
          - src_clk_edge->time();
        delay.launcher_slew = delayAsFloat(launcher->slew(this), min_max, this);
        delay.launcher_rf = launcher->transition(this);
      }
    }
    delay.exists = true;
  }
  debugPrint(debug_, "make_timing_model", 2,
             "internal {} -> {} {} launch {} check {}",
             launch.port->name(), pin_name, setup ? "setup" : "hold",
             delayAsString(launch_delay, this),
             delayAsString(check_margin, this));
  return true;
}

// Each endpoint inside the block is modeled with an internal pin that
// has an arc from the register output pin that launches its worst path
// for each launch clock edge, an arc from each input with a path to it
// and the checks of the endpoint, so the paths to it are timed with the
// clocks of the context the model is used in. The register output pins
// are internal pins with a clock -> pin arc.
void
MakeTimingModel::findInternalPaths()
{
  MakeInternalPathArcs end_visitor(this, sta_);
  VisitPathEnds visit_ends(sta_);
  for (Vertex *end : search_->endpoints())
    visit_ends.visitPathEnds(end, scenes_, MinMaxAll::all(), false, &end_visitor);

  for (const auto &[pin_name, endpoint_paths] : internal_endpoint_paths_) {
    for (const auto &[launch, captures] : endpoint_paths) {
      for (const auto &[capture, delays] : captures) {
        for (const MinMax *min_max : MinMax::range()) {
          for (const RiseFall *rf : RiseFall::range())
            mergeInternalLauncher(launch, min_max,
                                  delays[min_max->index()][rf->index()]);
        }
      }
    }
  }
  for (const auto &[port_name, output_paths] : internal_output_paths_) {
    for (const auto &[launch, delays] : output_paths.paths) {
      for (const MinMax *min_max : MinMax::range()) {
        for (const RiseFall *rf : RiseFall::range())
          mergeInternalLauncher(launch, min_max,
                                delays[min_max->index()][rf->index()]);
      }
    }
  }
  for (const auto &[pin_name, clk_edge_delays] : internal_launchers_)
    makeInternalLauncherArcs(pin_name, clk_edge_delays);
  for (const auto &[pin_name, endpoint_paths] : internal_endpoint_paths_)
    makeInternalPathArcs(pin_name, endpoint_paths);
  for (const auto &[port_name, output_paths] : internal_output_paths_)
    makeInternalOutputArcs(output_paths);
}

void
MakeTimingModel::mergeInternalLauncher(const InternalLaunch &launch,
                                       const MinMax *min_max,
                                       const InternalPathDelay &delay)
{
  if (launch.from_input || !delay.exists || delay.launcher.empty())
    return;
  InternalLauncherDelays &launcher =
    internal_launchers_[delay.launcher][{launch.port, launch.rf}];
  int mm_index = min_max->index();
  int launcher_rf_index = delay.launcher_rf->index();
  float &launcher_delay = launcher.delay[mm_index][launcher_rf_index];
  bool &exists = launcher.exists[mm_index][launcher_rf_index];
  if (!exists
      || (min_max == MinMax::max()
          ? delay.launcher_delay > launcher_delay
          : delay.launcher_delay < launcher_delay)) {
    launcher_delay = delay.launcher_delay;
    launcher.slew[mm_index][launcher_rf_index] = delay.launcher_slew;
    exists = true;
  }
}

float
MakeTimingModel::internalLauncherDelay(const InternalLaunch &launch,
                                       const MinMax *min_max,
                                       const InternalPathDelay &delay)
{
  const InternalLauncherDelays &launcher =
    internal_launchers_[delay.launcher][{launch.port, launch.rf}];
  return launcher.delay[min_max->index()][delay.launcher_rf->index()];
}

bool
MakeTimingModel::recordInternalOutputPath(const Pin *output_pin,
                                          const Path *path)
{
  const Path *launcher = launcherPath(path, this);
  LibertyPort *clk_port = clkModelPort(path->clkInfo(this)->clkSrc());
  LibertyPort *output_port = modelPort(output_pin);
  if (launcher == nullptr || clk_port == nullptr || output_port == nullptr)
    return false;
  const ClockEdge *clk_edge = path->clkEdge(this);
  const MinMax *min_max = path->minMax(this);
  float launch_delay = delayAsFloat(path->arrival(), min_max, this)
    - clk_edge->time();
  InternalOutputPaths &output_paths = internal_output_paths_[output_port->name()];
  output_paths.output_pin = output_pin;
  InternalLaunch launch{clk_port, clk_edge->transition(), false};
  InternalPathDelay &delay =
    output_paths.paths[launch][min_max->index()][path->transition(this)->index()];
  if (!delay.exists
      || (min_max == MinMax::max()
          ? launch_delay > delay.launch_delay
          : launch_delay < delay.launch_delay)) {
    delay.launch_delay = launch_delay;
    delay.slew = delayAsFloat(path->slew(this), min_max, this);
    delay.launcher = internalPinName(launcher->pin(this), this);
    delay.launcher_delay = delayAsFloat(launcher->arrival(), min_max, this)
      - clk_edge->time();
    delay.launcher_slew = delayAsFloat(launcher->slew(this), min_max, this);
    delay.launcher_rf = launcher->transition(this);
    delay.exists = true;
  }
  return true;
}

// Launching register -> output arcs.
void
MakeTimingModel::makeInternalOutputArcs(const InternalOutputPaths &output_paths)
{
  InternalArcDelaysMap arc_delays;
  for (const auto &[launch, delays] : output_paths.paths) {
    for (const MinMax *min_max : MinMax::range()) {
      for (const RiseFall *rf : RiseFall::range()) {
        const InternalPathDelay &delay = delays[min_max->index()][rf->index()];
        auto launcher_itr = internal_launcher_ports_.find(delay.launcher);
        if (delay.exists && launcher_itr != internal_launcher_ports_.end()) {
          InternalArcDelays &arc = arc_delays[delay.launcher];
          arc.from_port = launcher_itr->second;
          arc.merge(delay.launcher_rf, rf, min_max,
                    delay.launch_delay - internalLauncherDelay(launch, min_max, delay),
                    delay.slew);
        }
      }
    }
  }
  makeInternalLaunchArcs(arc_delays, modelPort(output_paths.output_pin),
                         output_paths.output_pin);
}

void
MakeTimingModel::makeInternalLauncherArcs(const std::string &pin_name,
                                          const InternalLauncherClkEdges &clk_edge_delays)
{
  if (cell_->findLibertyPort(pin_name)) {
    report_->warn(1385, "internal path pin {} conflicts with a block port.",
                  pin_name);
    return;
  }
  LibertyPort *launcher_port = lib_builder_->makePort(cell_, pin_name);
  launcher_port->setDirection(PortDirection::internal());
  internal_launcher_ports_[pin_name] = launcher_port;
  for (const auto &[clk_edge, delays] : clk_edge_delays) {
    TimingArcAttrsPtr attrs = nullptr;
    int max_index = MinMax::max()->index();
    int min_index = MinMax::min()->index();
    for (const RiseFall *rf : RiseFall::range()) {
      int rf_index = rf->index();
      bool max_exists = delays.exists[max_index][rf_index];
      bool min_exists = delays.exists[min_index][rf_index];
      if (max_exists || min_exists) {
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        int index = max_exists ? max_index : min_index;
        attrs->setModel(rf, makeGateModelScalar(delays.delay[index][rf_index],
                                                delays.slew[index][rf_index], rf));
        if (max_exists && min_exists)
          setDistinctRetainModel(attrs.get(), rf,
                                 makeGateModelScalar(delays.delay[min_index][rf_index],
                                                     delays.slew[min_index][rf_index],
                                                     rf));
      }
    }
    if (attrs)
      lib_builder_->makeFromTransitionArcs(cell_, clk_edge.port, launcher_port,
                                           nullptr, clk_edge.rf,
                                           TimingRole::regClkToQ(), attrs);
  }
}

// Launch delays for one launch and endpoint transition.
class InternalLaunchDelays
{
public:
  float max_delay{0.0};
  float max_slew{0.0};
  bool max_exists{false};
  float min_delay{0.0};
  float min_slew{0.0};
  bool min_exists{false};
};

void
InternalArcDelays::merge(const RiseFall *from_rf,
                         const RiseFall *to_rf,
                         const MinMax *min_max,
                         float delay,
                         float slew)
{
  int from_index = from_rf->index();
  int to_index = to_rf->index();
  int mm_index = min_max->index();
  float &value = delays[from_index][to_index][mm_index];
  bool &value_exists = exists[from_index][to_index][mm_index];
  if (!value_exists
      || (min_max == MinMax::max() ? delay > value : delay < value)) {
    value = delay;
    slews[from_index][to_index][mm_index] = slew;
    value_exists = true;
  }
}

// The check margins are those of the worst path to the endpoint from any
// launch. The launch arc delays absorb the margin differences of the
// other launches so each one keeps its worst slack. Launch arcs use the
// max delays for setup and the min (retaining) delays for hold.
void
MakeTimingModel::makeInternalPathArcs(const std::string &pin_name,
                                      const InternalEndpointPaths &endpoint_paths)
{
  if (cell_->findLibertyPort(pin_name)) {
    report_->warn(1385, "internal path pin {} conflicts with a block port.",
                  pin_name);
    return;
  }
  LibertyPort *int_port = lib_builder_->makePort(cell_, pin_name);
  int_port->setDirection(PortDirection::internal());

  std::map<InternalClkEdge, InternalPathDelays> check_margins;
  for (const auto &[launch, captures] : endpoint_paths) {
    for (const auto &[capture, delays] : captures) {
      InternalPathDelays &margins = check_margins[capture];
      for (const MinMax *min_max : MinMax::range()) {
        bool setup = (min_max == MinMax::max());
        for (const RiseFall *rf : RiseFall::range()) {
          const InternalPathDelay &delay = delays[min_max->index()][rf->index()];
          InternalPathDelay &margin = margins[min_max->index()][rf->index()];
          if (delay.exists) {
            float violation = setup
              ? delay.launch_delay + delay.check_margin
              : delay.check_margin - delay.launch_delay;
            float worst_violation = setup
              ? margin.launch_delay + margin.check_margin
              : margin.check_margin - margin.launch_delay;
            if (!margin.exists || violation > worst_violation)
              margin = delay;
          }
        }
      }
    }
  }

  for (const auto &[capture, margins] : check_margins) {
    for (const MinMax *min_max : MinMax::range()) {
      bool setup = (min_max == MinMax::max());
      TimingArcAttrsPtr attrs = nullptr;
      const TimingRole *role = setup ? TimingRole::setup() : TimingRole::hold();
      for (const RiseFall *rf : RiseFall::range()) {
        const InternalPathDelay &margin = margins[min_max->index()][rf->index()];
        if (margin.exists) {
          if (margin.check_role == TimingRole::recovery())
            role = TimingRole::recovery();
          else if (margin.check_role == TimingRole::removal())
            role = TimingRole::removal();
          ScaleFactorType scale_type = setup
            ? ScaleFactorType::setup
            : ScaleFactorType::hold;
          if (role == TimingRole::recovery())
            scale_type = ScaleFactorType::recovery;
          else if (role == TimingRole::removal())
            scale_type = ScaleFactorType::removal;
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          attrs->setModel(rf, makeScalarCheckModel(margin.check_margin,
                                                   scale_type, rf));
        }
      }
      if (attrs)
        lib_builder_->makeFromTransitionArcs(cell_, capture.port, int_port,
                                             nullptr, capture.rf, role, attrs);
    }
  }

  // Arcs from inputs and launching registers.
  InternalArcDelaysMap arc_delays;
  for (const auto &[launch, captures] : endpoint_paths) {
    TimingArcAttrsPtr attrs = nullptr;
    for (const RiseFall *rf : RiseFall::range()) {
      InternalLaunchDelays launch_delays;
      for (const auto &[capture, delays] : captures) {
        const InternalPathDelays &margins = check_margins[capture];
        for (const MinMax *min_max : MinMax::range()) {
          bool setup = (min_max == MinMax::max());
          const InternalPathDelay &delay = delays[min_max->index()][rf->index()];
          if (!delay.exists)
            continue;
          const InternalPathDelay &margin = margins[min_max->index()][rf->index()];
          float launch_delay = setup
            ? delay.launch_delay + delay.check_margin - margin.check_margin
            : delay.launch_delay - delay.check_margin + margin.check_margin;
          auto launcher_itr = internal_launcher_ports_.find(delay.launcher);
          if (launch.from_input) {
            InternalArcDelays &arc = arc_delays[launch.port->name()];
            arc.from_port = launch.port;
            arc.merge(launch.rf, rf, min_max, launch_delay, delay.slew);
          }
          else if (!delay.launcher.empty()
                   && launcher_itr != internal_launcher_ports_.end()) {
            float launcher_delay = internalLauncherDelay(launch, min_max, delay);
            InternalArcDelays &arc = arc_delays[delay.launcher];
            arc.from_port = launcher_itr->second;
            arc.merge(delay.launcher_rf, rf, min_max,
                      launch_delay - launcher_delay, delay.slew);
          }
          else if (setup) {
            if (!launch_delays.max_exists || launch_delay > launch_delays.max_delay) {
              launch_delays.max_delay = launch_delay;
              launch_delays.max_slew = delay.slew;
              launch_delays.max_exists = true;
            }
          }
          else {
            if (!launch_delays.min_exists || launch_delay < launch_delays.min_delay) {
              launch_delays.min_delay = launch_delay;
              launch_delays.min_slew = delay.slew;
              launch_delays.min_exists = true;
            }
          }
        }
      }
      // Clock port -> endpoint paths without a launching register.
      // Without a setup path the min delay is also the max delay.
      if (!launch_delays.max_exists && launch_delays.min_exists) {
        launch_delays.max_delay = launch_delays.min_delay;
        launch_delays.max_slew = launch_delays.min_slew;
        launch_delays.max_exists = true;
      }
      if (launch_delays.max_exists) {
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        attrs->setModel(rf, makeGateModelScalar(launch_delays.max_delay,
                                                launch_delays.max_slew, rf));
        if (launch_delays.min_exists)
          setDistinctRetainModel(attrs.get(), rf,
                                 makeGateModelScalar(launch_delays.min_delay,
                                                     launch_delays.min_slew, rf));
      }
    }
    if (attrs)
      lib_builder_->makeFromTransitionArcs(cell_, launch.port, int_port,
                                           nullptr, launch.rf,
                                           TimingRole::regClkToQ(), attrs);
  }

  makeInternalLaunchArcs(arc_delays, int_port, nullptr);
}

// Positive and negative unate arcs so each from/to transition pair keeps
// its own delay. Arcs to output ports use the output pin load models.
void
MakeTimingModel::makeInternalLaunchArcs(const InternalArcDelaysMap &arc_delays,
                                        LibertyPort *to_port,
                                        const Pin *output_pin)
{
  int max_index = MinMax::max()->index();
  int min_index = MinMax::min()->index();
  for (const auto &[from_name, arc] : arc_delays) {
    for (TimingSense sense : {TimingSense::positive_unate,
                              TimingSense::negative_unate}) {
      TimingArcAttrsPtr attrs = nullptr;
      for (const RiseFall *to_rf : RiseFall::range()) {
        const RiseFall *from_rf = (sense == TimingSense::positive_unate)
          ? to_rf
          : to_rf->opposite();
        int from_index = from_rf->index();
        int to_index = to_rf->index();
        bool max_exists = arc.exists[from_index][to_index][max_index];
        bool min_exists = arc.exists[from_index][to_index][min_index];
        // Without a setup path the min delay is also the max delay.
        if (max_exists || min_exists) {
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          int index = max_exists ? max_index : min_index;
          float delay = arc.delays[from_index][to_index][index];
          attrs->setModel(to_rf, output_pin
                          ? makeOutputGateModel(output_pin, delay, to_rf,
                                                max_exists ? MinMax::max()
                                                           : MinMax::min())
                          : makeGateModelScalar(delay,
                                                arc.slews[from_index][to_index][index],
                                                to_rf));
          float min_delay = arc.delays[from_index][to_index][min_index];
          if (max_exists && min_exists)
            setDistinctRetainModel(attrs.get(), to_rf, output_pin
                                   ? makeOutputGateModel(output_pin, min_delay, to_rf,
                                                         MinMax::min())
                                   : makeGateModelScalar(min_delay,
                                                         arc.slews[from_index][to_index][min_index],
                                                         to_rf));
        }
      }
      if (attrs) {
        attrs->setTimingSense(sense);
        lib_builder_->makeCombinationalArcs(cell_, arc.from_port, to_port,
                                            true, true, attrs);
      }
    }
  }
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
      std::string port_name = network_->name(port);
      LibertyPort *lib_port = cell_->findLibertyPort(port_name);
      Pin *pin = network_->findPin(top_inst, port);
      if (pin && sdc_->isClock(pin)) {
        lib_port->setIsClock(true);
        ClockSet *clks = sdc_->findClocks(pin);
        if (clks->size() == 1) {
          for (const Clock *clk : *clks) {
            // Clock tree delays are removed from the arcs of macros
            // created with propagated clocks that are used with ideal
            // clocks. Arcs found with ideal clocks do not include them.
            if (clk->isIdeal())
              continue;
            ClkDelays delays = sta_->findClkDelays(clk, scene_, true);
            for (const MinMax *min_max : MinMax::range()) {
              makeClkTreePaths(lib_port, min_max, TimingSense::positive_unate,
                               delays);
              makeClkTreePaths(lib_port, min_max, TimingSense::negative_unate,
                               delays);
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
    const RiseFall *end_rf =
        (sense == TimingSense::positive_unate) ? clk_rf : clk_rf->opposite();
    Path clk_path;
    Delay insertion, delay, latency;
    float lib_clk_delay;
    bool exists;
    delays.delay(clk_rf, end_rf, min_max, insertion, delay, lib_clk_delay, latency,
                 clk_path, exists);
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
    lib_builder_->makeClockTreePathArcs(cell_, lib_port, role, attrs);
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
  TablePtr table = std::make_shared<Table>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *check_table = new TableModel(table, tbl_template, scale_factor_type, rf);
  TableModels *check_tables = new TableModels(check_table);
  CheckTableModel *check = new CheckTableModel(cell_, check_tables);
  return check;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     Slew slew,
                                     const RiseFall *rf)
{
  TablePtr delay_table = std::make_shared<Table>(delayAsFloat(delay));
  TablePtr slew_table = std::make_shared<Table>(delayAsFloat(slew));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModels *delay_models = new TableModels(delay_model);
  TableModel *slew_model = new TableModel(slew_table, tbl_template,
                                          ScaleFactorType::cell, rf);
  TableModels *slew_models = new TableModels(slew_model);
  GateTableModel *gate_model = new GateTableModel(cell_, delay_models, slew_models,
                                                  nullptr, nullptr);
  return gate_model;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     const RiseFall *rf)
{
  TablePtr delay_table = std::make_shared<Table>(delayAsFloat(delay));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModels *models = new TableModels(delay_model);
  GateTableModel *gate_model = new GateTableModel(cell_, models, nullptr,
                                                  nullptr, nullptr);
  return gate_model;
}

TimingModel *
MakeTimingModel::makeOutputGateModel(const Pin *output_pin,
                                     Delay delay,
                                     const RiseFall *rf,
                                     const MinMax *min_max)
{
  if (scalar_) {
    Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
    DcalcAPIndex dcalc_ap_index = scene_->dcalcAnalysisPtIndex(min_max);
    Slew slew = graph_->slew(output_vertex, rf, dcalc_ap_index);
    return makeGateModelScalar(delay, slew, rf);
  }
  else
    return makeGateModelTable(output_pin, delay, rf, min_max);
}

// Eval the driver pin model along its load capacitance
// axis and add the input to output 'delay' to the table values.
TimingModel *
MakeTimingModel::makeGateModelTable(const Pin *output_pin,
                                    Delay delay,
                                    const RiseFall *rf,
                                    const MinMax *min_max)
{
  const Pvt *pvt = sdc_->operatingConditions(min_max);
  PinSet *drvrs = network_->drivers(network_->net(network_->term(output_pin)));
  DcalcAPIndex ap_index = scene_->dcalcAnalysisPtIndex(min_max);
  const Pin *drvr_pin = *drvrs->begin();
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  if (drvr_port) {
    const LibertyCell *drvr_cell = drvr_port->libertyCell();
    for (TimingArcSet *arc_set : drvr_cell->timingArcSetsTo(drvr_port)) {
      for (TimingArc *drvr_arc : arc_set->arcs()) {
        // Use the first timing arc to simplify life.
        if (drvr_arc->toEdge()->asRiseFall() == rf) {
          const LibertyPort *gate_in_port = drvr_arc->from();
          const Instance *drvr_inst = network_->instance(drvr_pin);
          const Pin *gate_in_pin = network_->findPin(drvr_inst, gate_in_port);
          if (gate_in_pin) {
            Vertex *gate_in_vertex = graph_->pinLoadVertex(gate_in_pin);
            Slew in_slew = graph_->slew(
                gate_in_vertex, drvr_arc->fromEdge()->asRiseFall(), ap_index);
            float in_slew1 = delayAsFloat(in_slew);
            GateTableModel *drvr_gate_model =
                drvr_arc->gateTableModel(scene_, min_max);
            if (drvr_gate_model) {
              float output_load_cap = graph_delay_calc_->loadCap(output_pin,
                                                                 scene_,
                                                                 min_max);
              float drvr_self_delay, drvr_self_slew;
              drvr_gate_model->gateDelay(pvt, in_slew1, output_load_cap,
                                         drvr_self_delay, drvr_self_slew);

              const TableModel *drvr_table = drvr_gate_model->delayModels()->model();
              const TableTemplate *drvr_template = drvr_table->tblTemplate();
              const TableAxis *drvr_load_axis = loadCapacitanceAxis(drvr_table);
              if (drvr_load_axis) {
                const FloatSeq &drvr_axis_values = drvr_load_axis->values();
                FloatSeq *load_values = new FloatSeq;
                FloatSeq *slew_values = new FloatSeq;
                for (float load_cap : drvr_axis_values) {
                  // get slew from driver input pin
                  float gate_delay, gate_slew;
                  drvr_gate_model->gateDelay(pvt, in_slew1, load_cap,
                                             gate_delay, gate_slew);
                  // Remove the self delay driving the output pin net load cap.
                  load_values->push_back(delayAsFloat(delay)
                                         + gate_delay
                                         - drvr_self_delay);
                  slew_values->push_back(delayAsFloat(gate_slew));
                }

                FloatSeq axis_values = drvr_axis_values;
                TableAxisPtr load_axis = std::make_shared<TableAxis>(
                    TableAxisVariable::total_output_net_capacitance,
                    std::move(axis_values));

                TablePtr delay_table =
                    std::make_shared<Table>(load_values, load_axis);
                TablePtr slew_table =
                    std::make_shared<Table>(slew_values, load_axis);

                TableTemplate *model_template =
                    ensureTableTemplate(drvr_template, load_axis);
                TableModel *delay_model = new TableModel(delay_table, model_template,
                                                         ScaleFactorType::cell, rf);
                TableModels *delay_models = new TableModels(delay_model);
                TableModel *slew_model = new TableModel(slew_table, model_template,
                                                        ScaleFactorType::cell, rf);
                TableModels *slew_models = new TableModels(slew_model);
                GateTableModel *gate_model = new GateTableModel(cell_,
                                                                delay_models,
                                                                slew_models,
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
  Slew slew = graph_->slew(output_vertex, rf, ap_index);
  return makeGateModelScalar(delay, slew, rf);
}

TableTemplate *
MakeTimingModel::ensureTableTemplate(const TableTemplate *drvr_template,
                                     const TableAxisPtr &load_axis)
{
  TableTemplate *model_template = findKey(template_map_, drvr_template);
  if (model_template == nullptr) {
    std::string template_name = "template_";
    template_name += std::to_string(tbl_template_index_++);

    model_template =
        library_->makeTableTemplate(template_name, TableTemplateType::delay);
    model_template->setAxis1(load_axis);
    template_map_[drvr_template] = model_template;
  }
  return model_template;
}

const TableAxis *
MakeTimingModel::loadCapacitanceAxis(const TableModel *table)
{
  if (table->axis1()
      && table->axis1()->variable()
          == TableAxisVariable::total_output_net_capacitance)
    return table->axis1();
  else if (table->axis2()
           && table->axis2()->variable()
               == TableAxisVariable::total_output_net_capacitance)
    return table->axis2();
  else if (table->axis3()
           && table->axis3()->variable()
               == TableAxisVariable::total_output_net_capacitance)
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

}  // namespace sta
