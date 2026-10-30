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

#include "WriteContextSdc.hh"

#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Clock.hh"
#include "Delay.hh"
#include "Error.hh"
#include "Format.hh"
#include "Fuzzy.hh"
#include "Graph.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Path.hh"
#include "PortDirection.hh"
#include "Report.hh"
#include "RiseFallMinMax.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Search.hh"
#include "Sta.hh"
#include "Units.hh"

namespace sta {

using ClockEdgeValues = std::map<const ClockEdge*, RiseFallMinMax>;

// Worst context of one block port bit over every instance.
class PortContext
{
public:
  ClockSet clocks;
  ClockEdgeValues input_delays;
  ClockEdgeValues output_delays;
  RiseFallMinMax slews;
  RiseFallMinMax pin_caps;
  RiseFallMinMax wire_caps;
};

// Leaf pins and top level ports on a net outside an instance, and the drivers
// inside it.
class BoundaryPins : public PinVisitor
{
public:
  BoundaryPins(const Instance *inst,
               const Network *network);
  void operator()(const Pin *pin) override;

  PinSeq outside;
  PinSeq inside_drvrs;

private:
  const Instance *inst_;
  const Network *network_;
};

BoundaryPins::BoundaryPins(const Instance *inst,
                           const Network *network) :
  inst_(inst),
  network_(network)
{
}

void
BoundaryPins::operator()(const Pin *pin)
{
  if (network_->isLeaf(pin) || network_->isTopLevelPort(pin)) {
    if (!network_->isInside(pin, inst_))
      outside.push_back(pin);
    else if (network_->isDriver(pin))
      inside_drvrs.push_back(pin);
  }
}

// Port names of the instance's module, so instances of different modules
// are not merged.
static std::string
portNames(const Instance *inst,
          const Network *network)
{
  std::string names;
  std::unique_ptr<CellPortIterator> port_iter(network->portIterator(network->cell(inst)));
  while (port_iter->hasNext()) {
    names += ' ';
    names += network->name(port_iter->next());
  }
  return names;
}

// Merge the path delays at vertex into values by launching clock edge.
static void
mergeDelays(Vertex *vertex,
            const PathDelayFunc &path_delay,
            const Scene *scene,
            const Sta *sta,
            ClockEdgeValues &values)
{
  DelaysWrtClks delays = sta->search()->delaysWrtClks(vertex, scene, path_delay);
  for (const auto &[clk_edge, edge_delays] : delays) {
    for (const RiseFall *rf : RiseFall::range()) {
      for (const MinMax *min_max : MinMax::range()) {
        Delay delay;
        bool exists;
        edge_delays.value(rf, min_max, delay, exists);
        if (exists)
          values[clk_edge].mergeValue(rf, min_max, delayAsFloat(delay));
      }
    }
  }
}

// One line when the four rise/fall min/max values agree, else one per value.
static void
writeValues(std::ofstream &stream,
            std::string_view cmd,
            const RiseFallMinMax &values,
            float scale,
            std::string_view args,
            std::string_view object)
{
  bool all_equal = values.hasValue(RiseFall::rise(), MinMax::max());
  float rise_max = all_equal ? values.value(RiseFall::rise(), MinMax::max()) : 0.0;
  for (const RiseFall *rf : RiseFall::range()) {
    for (const MinMax *min_max : MinMax::range())
      all_equal &= values.hasValue(rf, min_max)
        && values.value(rf, min_max) == rise_max;
  }
  if (all_equal)
    stream << sta::format("{} {:.6f}{} {}\n", cmd, rise_max / scale, args, object);
  else {
    for (const RiseFall *rf : RiseFall::range()) {
      for (const MinMax *min_max : MinMax::range()) {
        if (values.hasValue(rf, min_max))
          stream << sta::format("{} -{} -{} {:.6f}{} {}\n", cmd, rf->name(),
                                min_max->to_string(), values.value(rf, min_max) / scale,
                                args, object);
      }
    }
  }
}

// Port delays by clock in name order, so the file is deterministic.
static void
writeDelays(std::ofstream &stream,
            std::string_view cmd,
            const ClockEdgeValues &delays,
            const ClockSeq &clks,
            const ClockEdge *default_edge,
            float scale,
            std::string_view object)
{
  // Arrivals from inputs with no clock (sta_input_port_default_clock) keep none.
  auto default_iter = delays.find(default_edge);
  if (default_iter != delays.end())
    writeValues(stream, cmd, default_iter->second, scale, " -add_delay", object);
  for (const Clock *clk : clks) {
    for (const RiseFall *clk_rf : RiseFall::range()) {
      auto delay_iter = delays.find(clk->edge(clk_rf));
      if (delay_iter != delays.end()) {
        std::string args = sta::format(" -clock [get_clocks {{{}}}]{} -add_delay", clk->name(),
                                       clk_rf == RiseFall::fall() ? " -clock_fall" : "");
        writeValues(stream, cmd, delay_iter->second, scale, args, object);
      }
    }
  }
}

static bool
allZero(const RiseFallMinMax &values)
{
  for (const RiseFall *rf : RiseFall::range()) {
    for (const MinMax *min_max : MinMax::range()) {
      if (values.hasValue(rf, min_max) && values.value(rf, min_max) != 0.0)
        return false;
    }
  }
  return true;
}

void
writeContextSdc(const InstanceSeq &insts,
                std::string_view filename,
                Sta *sta)
{
  sta->ensureLibLinked();
  sta->findRequireds();
  const Network *network = sta->network();
  const Network *sdc_network = sta->sdcNetwork();
  Report *report = sta->report();
  Scene *scene = sta->cmdScene();
  const Sdc *sdc = scene->sdc();
  const Clock *default_clk = sdc->defaultArrivalClock();
  const ClockEdge *default_edge = sdc->defaultArrivalClockEdge();
  SceneSeq scenes{scene};

  if (insts.empty())
    report->error(1387, "write_context_sdc needs at least one instance.");
  std::string first_ports = portNames(insts[0], network);
  for (const Instance *inst : insts) {
    if (network->isLeaf(inst))
      report->error(1388, "{} is not a hierarchical instance.", network->pathName(inst));
    std::string ports = portNames(inst, network);
    if (ports != first_ports)
      report->error(1389, "{} ports{} do not match {} ports{}.", network->pathName(inst),
                    ports, network->pathName(insts[0]), first_ports);
  }

  // Input delay of a path is its arrival after its launching edge.
  PathDelayFunc input_delay = [=](const Path *path) -> Delay {
    const ClockEdge *clk_edge = path->clkEdge(sta);
    if (clk_edge == nullptr)
      return INF;
    return delayAsFloat(path->arrival(), path->minMax(sta), sta) - clk_edge->time();
  };
  // Output delay that gives the same required time against the launching
  // clock's next edge (setup) or the launching edge itself (hold).
  PathDelayFunc output_delay = [=](const Path *path) -> Delay {
    const ClockEdge *clk_edge = path->clkEdge(sta);
    float required = delayAsFloat(path->required(), path->minMax(sta), sta);
    if (clk_edge == nullptr || clk_edge->clock() == default_clk || fuzzyInf(required))
      return INF;
    float check_time = clk_edge->time();
    if (path->minMax(sta) == MinMax::max())
      check_time += clk_edge->clock()->period();
    return check_time - required;
  };

  std::vector<std::string> port_names;
  std::map<std::string, PortContext> contexts;
  for (const Instance *inst : insts) {
    std::unique_ptr<InstancePinIterator> pin_iter(network->pinIterator(inst));
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      const Net *net = network->net(pin);
      if (net == nullptr)
        continue;
      std::string port_name = sdc_network->name(network->port(pin));
      auto [context_iter, inserted] = contexts.try_emplace(port_name);
      if (inserted)
        port_names.push_back(port_name);
      PortContext &context = context_iter->second;
      BoundaryPins boundary(inst, network);
      network->visitConnectedPins(net, boundary);
      const PortDirection *dir = network->direction(pin);

      if (dir->isAnyInput()) {
        for (const Pin *drvr : boundary.outside) {
          if (!network->isDriver(drvr))
            continue;
          ClockSet clks = sta->clocks(drvr, scene->mode());
          if (!clks.empty()) {
            context.clocks.insert(clks.begin(), clks.end());
            continue;
          }
          Vertex *vertex = sta->graph()->pinDrvrVertex(drvr);
          mergeDelays(vertex, input_delay, scene, sta, context.input_delays);
          for (const RiseFall *rf : RiseFall::range()) {
            for (const MinMax *min_max : MinMax::range()) {
              Slew slew = sta->slew(vertex, rf->asRiseFallBoth(), scenes, min_max);
              context.slews.mergeValue(rf, min_max, delayAsFloat(slew));
            }
          }
        }
      }

      if (dir->isAnyOutput()) {
        PinSeq loads;
        for (const Pin *load : boundary.outside) {
          if (network->isLoad(load)) {
            loads.push_back(load);
            Vertex *vertex = sta->graph()->pinLoadVertex(load);
            mergeDelays(vertex, output_delay, scene, sta, context.output_delays);
          }
        }
        // Pin loads outside the block by OpenSTA's own pin rule, and the net's wire
        // load (parasitics, set_load) as the block's driver sees it.
        if (!loads.empty()) {
          for (const RiseFall *rf : RiseFall::range()) {
            for (const MinMax *min_max : MinMax::range()) {
              float pin_cap = 0.0;
              float port_wire_cap = 0.0;
              float fanout = 0.0;
              for (const Pin *load : loads)
                sdc->pinCaps(load, rf, scene, min_max, pin_cap, port_wire_cap, fanout);
              context.pin_caps.mergeValue(rf, min_max, pin_cap);
              for (const Pin *drvr : boundary.inside_drvrs) {
                float drvr_pin_cap, wire_cap;
                sta->connectedCap(drvr, rf, scene, min_max, drvr_pin_cap, wire_cap);
                context.wire_caps.mergeValue(rf, min_max, wire_cap);
              }
            }
          }
        }
      }
    }
  }

  // Clocks the file needs: those on block ports and those the delays refer to.
  std::set<const Clock*> used_clks;
  for (const auto &[port_name, context] : contexts) {
    used_clks.insert(context.clocks.begin(), context.clocks.end());
    for (const auto &[clk_edge, delays] : context.input_delays)
      used_clks.insert(clk_edge->clock());
    for (const auto &[clk_edge, delays] : context.output_delays)
      used_clks.insert(clk_edge->clock());
  }
  ClockSeq clks;
  for (Clock *clk : sdc->sortedClocks()) {
    if (used_clks.count(clk) && clk != default_clk)
      clks.push_back(clk);
  }

  std::string filename_str(filename);
  std::ofstream stream(filename_str);
  if (!stream.is_open())
    throw FileNotWritable(filename);
  const Units *units = sta->units();
  float time_scale = units->timeUnit()->scale();
  float cap_scale = units->capacitanceUnit()->scale();

  std::set<std::string> clocked_ports;
  for (Clock *clk : clks) {
    std::string ports;
    bool add = false;
    for (const std::string &port_name : port_names) {
      if (contexts[port_name].clocks.count(clk)) {
        ports += (ports.empty() ? "" : " ") + port_name;
        add |= !clocked_ports.insert(port_name).second;
      }
    }
    std::string waveform;
    for (float edge_time : clk->waveform())
      waveform += sta::format("{}{:.6f}", waveform.empty() ? "" : " ", edge_time / time_scale);
    stream << sta::format("create_clock -name {{{}}}{} -period {:.6f} -waveform {{{}}}",
                          clk->name(), add ? " -add" : "", clk->period() / time_scale,
                          waveform);
    if (!ports.empty())
      stream << sta::format(" [get_ports {{{}}}]", ports);
    stream << "\n";
  }

  for (const std::string &port_name : port_names) {
    std::string object = sta::format("[get_ports {{{}}}]", port_name);
    writeDelays(stream, "set_input_delay", contexts[port_name].input_delays, clks,
                default_edge, time_scale, object);
  }
  for (const std::string &port_name : port_names) {
    std::string object = sta::format("[get_ports {{{}}}]", port_name);
    writeDelays(stream, "set_output_delay", contexts[port_name].output_delays, clks,
                default_edge, time_scale, object);
  }
  for (const std::string &port_name : port_names) {
    const PortContext &context = contexts[port_name];
    std::string object = sta::format("[get_ports {{{}}}]", port_name);
    if (!allZero(context.slews))
      writeValues(stream, "set_input_transition", context.slews, time_scale, "", object);
    if (!allZero(context.pin_caps))
      writeValues(stream, "set_load -pin_load", context.pin_caps, cap_scale, "", object);
    if (!allZero(context.wire_caps))
      writeValues(stream, "set_load -wire_load", context.wire_caps, cap_scale, "", object);
  }
}

} // namespace sta
