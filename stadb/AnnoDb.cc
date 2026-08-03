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

// Read and write the delays held by the timing graph.
//
// This is a snapshot of every delay in the graph, gate and wire alike, however
// it was arrived at: liberty tables, parasitics, or an sdf file. On load the
// delays go back as annotations, exactly like read_sdf, so report_checks gives
// the same answer without the parasitics or the delay calculation that
// produced them in the first place.
//
// A delay is found again by naming the two ends of the edge it sits on: the
// driver pin and the load pin. Between one pair of pins there can be several
// edges, one per timing arc set (a setup and a hold check between the same two
// pins, for example), so the arc set index within the liberty cell goes in the
// file as well. Everything else is a value: which arc, which delay calc
// analysis point, and the delay itself.

#include "AnnoDb.hh"

#include <string>
#include <tuple>
#include <vector>

#include "Delay.hh"
#include "Graph.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Report.hh"
#include "StaState.hh"
#include "TimingArc.hh"
#include "Transition.hh"
#include "liberty/LibDb.hh"

namespace sta {

// One arc delay at one analysis point.
struct AnnoArcDelay
{
  uint32_t arc_index;
  uint8_t ap_index;
  float delay;
};

// Walks the graph and appends body bytes via DbWriter.
class AnnoWriter
{
public:
  AnnoWriter(StaState *sta) :
    graph_(sta->graph()),
    network_(sta->network()),
    ap_count_(sta->dcalcAnalysisPtCount())
  {
  }

  std::vector<uint8_t> bytes();

private:
  std::vector<AnnoArcDelay> edgeDelays(Edge *edge);
  void writeVertexRef(Vertex *vertex);
  void writeEdges();
  void writeSlews();
  void writePeriodChecks();

  DbWriter w_;            // body bytes + unique string list
  Graph *graph_;
  Network *network_;
  DcalcAPIndex ap_count_;
};

////////////////////////////////////////////////////////////////

void
AnnoWriter::writeVertexRef(Vertex *vertex)
{
  // A bidirect pin has two vertices, one driving and one loading, so the
  // pin path alone does not say which one this is.
  w_.str(network_->pathName(vertex->pin()));
  w_.boolean(vertex->isBidirectDriver());
}

std::vector<AnnoArcDelay>
AnnoWriter::edgeDelays(Edge *edge)
{
  std::vector<AnnoArcDelay> delays;
  for (const TimingArc *arc : edge->timingArcSet()->arcs()) {
    for (DcalcAPIndex ap = 0; ap < ap_count_; ap++)
      delays.push_back({static_cast<uint32_t>(arc->index()),
                        static_cast<uint8_t>(ap),
                        delayAsFloat(graph_->arcDelay(edge, arc, ap))});
  }
  return delays;
}

void
AnnoWriter::writeEdges()
{
  std::vector<Edge *> edges;
  std::vector<std::vector<AnnoArcDelay>> edge_delays;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      std::vector<AnnoArcDelay> delays = edgeDelays(edge);
      if (!delays.empty()) {
        edges.push_back(edge);
        edge_delays.push_back(std::move(delays));
      }
    }
  }

  w_.u32(static_cast<uint32_t>(edges.size()));
  for (size_t i = 0; i < edges.size(); i++) {
    Edge *edge = edges[i];
    writeVertexRef(edge->from(graph_));
    writeVertexRef(edge->to(graph_));
    // Wire arcs come from one shared arc set, so its index means nothing.
    const TimingArcSet *arc_set = edge->timingArcSet();
    w_.boolean(arc_set == TimingArcSet::wireTimingArcSet());
    w_.u32(static_cast<uint32_t>(arc_set->index()));
    w_.u32(static_cast<uint32_t>(edge_delays[i].size()));
    for (const AnnoArcDelay &delay : edge_delays[i]) {
      w_.u32(delay.arc_index);
      w_.u8(delay.ap_index);
      w_.f32(delay.delay);
    }
  }
}

void
AnnoWriter::writeSlews()
{
  // Slews go in for every vertex, not just the annotated ones, for the same
  // reason the delays do: they were computed from parasitics that the file
  // does not carry, so recomputing them would not give the same numbers.
  std::vector<Vertex *> vertices;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext())
    vertices.push_back(vertex_iter.next());

  w_.u32(static_cast<uint32_t>(vertices.size()));
  for (Vertex *vertex : vertices) {
    writeVertexRef(vertex);
    for (const RiseFall *rf : RiseFall::range()) {
      for (DcalcAPIndex ap = 0; ap < ap_count_; ap++)
        w_.f32(delayAsFloat(graph_->slew(vertex, rf, ap)));
    }
  }
}

void
AnnoWriter::writePeriodChecks()
{
  // Period annotations hang off pins rather than edges, and the graph has no
  // iterator for them, so ask every leaf pin.
  std::vector<std::tuple<std::string, uint8_t, float>> checks;
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    const Instance *inst = inst_iter->next();
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      for (DcalcAPIndex ap = 0; ap < ap_count_; ap++) {
        float period = 0.0;
        bool exists = false;
        graph_->periodCheckAnnotation(pin, ap, period, exists);
        if (exists)
          checks.emplace_back(network_->pathName(pin),
                              static_cast<uint8_t>(ap), period);
      }
    }
    delete pin_iter;
  }
  delete inst_iter;

  w_.u32(static_cast<uint32_t>(checks.size()));
  for (const auto &[path, ap, period] : checks) {
    w_.str(path);
    w_.u8(ap);
    w_.f32(period);
  }
}

std::vector<uint8_t>
AnnoWriter::bytes()
{
  // How many analysis points these values were written for; loading into a
  // session with a different count would silently misplace them.
  w_.u8(static_cast<uint8_t>(ap_count_));
  writeEdges();
  writeSlews();
  writePeriodChecks();
  return dbPack(w_);
}

std::vector<uint8_t>
writeAnnoDbBytes(StaState *sta)
{
  AnnoWriter writer(sta);
  return writer.bytes();
}

////////////////////////////////////////////////////////////////

// Inverse of AnnoWriter: read body fields in the same order and put the
// annotations back on the graph.
class AnnoLoader
{
public:
  AnnoLoader(DbReader &r,
             std::string_view label,
             StaState *sta) :
    r_(r),
    label_(label),
    graph_(sta->graph()),
    network_(sta->network()),
    report_(sta->report()),
    ap_count_(sta->dcalcAnalysisPtCount())
  {
  }

  void read();

private:
  Vertex *readVertexRef();
  Edge *findEdge(Vertex *from,
                 Vertex *to,
                 bool is_wire,
                 uint32_t arc_set_index);
  void readEdges();
  void readSlews();
  void readPeriodChecks();

  DbReader &r_;                 // bookmark over body bytes + string list
  std::string label_;
  Graph *graph_;
  Network *network_;
  Report *report_;
  DcalcAPIndex ap_count_;
};

Vertex *
AnnoLoader::readVertexRef()
{
  const std::string &path = r_.str();
  bool bidirect_driver = r_.boolean();
  const Pin *pin = network_->findPin(path);
  if (pin == nullptr)
    report_->error(1430, "{} annotates missing pin {}.", label_, path);
  Vertex *vertex = nullptr;
  Vertex *bidirect_drvr_vertex = nullptr;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  return bidirect_driver ? bidirect_drvr_vertex : vertex;
}

Edge *
AnnoLoader::findEdge(Vertex *from,
                     Vertex *to,
                     bool is_wire,
                     uint32_t arc_set_index)
{
  VertexOutEdgeIterator edge_iter(from, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    const TimingArcSet *arc_set = edge->timingArcSet();
    bool edge_is_wire = arc_set == TimingArcSet::wireTimingArcSet();
    if (edge->to(graph_) == to
        && edge_is_wire == is_wire
        && (is_wire || arc_set->index() == arc_set_index))
      return edge;
  }
  return nullptr;
}

void
AnnoLoader::readEdges()
{
  uint32_t edge_count = r_.u32();
  for (uint32_t i = 0; i < edge_count; i++) {
    Vertex *from = readVertexRef();
    Vertex *to = readVertexRef();
    bool is_wire = r_.boolean();
    uint32_t arc_set_index = r_.u32();
    Edge *edge = (from && to)
      ? findEdge(from, to, is_wire, arc_set_index)
      : nullptr;
    uint32_t delay_count = r_.u32();
    for (uint32_t d = 0; d < delay_count; d++) {
      uint32_t arc_index = r_.u32();
      DcalcAPIndex ap = r_.u8();
      float delay = r_.f32();
      // Keep reading even when the edge is gone so the body stays aligned.
      if (edge) {
        const TimingArc *arc = edge->timingArcSet()->findTimingArc(arc_index);
        if (arc) {
          graph_->setArcDelay(edge, arc, ap, ArcDelay(delay));
          graph_->setArcDelayAnnotated(edge, arc, ap, true);
        }
      }
    }
  }
}

void
AnnoLoader::readSlews()
{
  uint32_t vertex_count = r_.u32();
  for (uint32_t i = 0; i < vertex_count; i++) {
    Vertex *vertex = readVertexRef();
    for (const RiseFall *rf : RiseFall::range()) {
      for (DcalcAPIndex ap = 0; ap < ap_count_; ap++) {
        float slew = r_.f32();
        if (vertex) {
          graph_->setSlew(vertex, rf, ap, Slew(slew));
          // Annotating keeps delay calculation from replacing the value.
          // The vertex tracks this per min/max, which is what the first two
          // analysis point indexes mean.
          vertex->setSlewAnnotated(true, rf, ap);
        }
      }
    }
  }
}

void
AnnoLoader::readPeriodChecks()
{
  uint32_t check_count = r_.u32();
  for (uint32_t i = 0; i < check_count; i++) {
    const std::string &path = r_.str();
    DcalcAPIndex ap = r_.u8();
    float period = r_.f32();
    const Pin *pin = network_->findPin(path);
    if (pin == nullptr)
      report_->error(1431, "{} annotates missing pin {}.", label_, path);
    graph_->setPeriodCheckAnnotation(pin, ap, period);
  }
}

void
AnnoLoader::read()
{
  uint8_t ap_count = r_.u8();
  if (ap_count != ap_count_)
    report_->error(1432, "{} was written with {} delay calc analysis points, "
                   "this session has {}.", label_, ap_count, ap_count_);
  readEdges();
  readSlews();
  readPeriodChecks();

  // DbReader sets this if we tried to read past the end of the body.
  if (r_.failed())
    report_->error(1433, "{} is truncated or corrupt.", label_);
}

void
readAnnoDbBytes(const uint8_t *data,
                size_t size,
                std::string_view label,
                StaState *sta)
{
  std::vector<std::string> strings;
  std::vector<uint8_t> body;
  if (!dbUnpack(data, size, strings, body))
    sta->report()->error(1434, "{} has a corrupt string table.", label);

  DbReader reader(body.data(), body.size(), &strings);
  AnnoLoader loader(reader, label, sta);
  loader.read();
}

} // namespace sta
