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

// Read and write the parasitics of the current scene.
//
// A session can hold more than one set of parasitics, one per spef file read,
// with the scene picking which set answers for min and which for max. Each set
// is written as its nets: the nodes of the RC network with their capacitance
// to ground, then the resistors and capacitors between those nodes. Nodes are
// numbered by the order they are written, so a resistor is a pair of numbers.
//
// Spef that was reduced on the way in has pi models and elmore delays per
// driver pin instead of a network, so those are written too.

#include "ParaDb.hh"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "MinMax.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "Report.hh"
#include "Scene.hh"
#include "Sta.hh"
#include "Transition.hh"
#include "liberty/LibDb.hh"

namespace sta {

// Walk the network once; both passes want the same nets and pins.
static void
findNetsAndPins(const Network *network,
                const Instance *inst,
                // Return values.
                std::vector<const Net *> &nets,
                std::vector<const Pin *> &pins)
{
  InstanceNetIterator *net_iter = network->netIterator(inst);
  while (net_iter->hasNext())
    nets.push_back(net_iter->next());
  delete net_iter;

  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext())
    pins.push_back(pin_iter->next());
  delete pin_iter;

  InstanceChildIterator *child_iter = network->childIterator(inst);
  while (child_iter->hasNext())
    findNetsAndPins(network, child_iter->next(), nets, pins);
  delete child_iter;
}

// Walks the parasitics of a scene and appends body bytes via DbWriter.
class ParaWriter
{
public:
  ParaWriter(Sta *sta) :
    sta_(sta),
    network_(sta->network()),
    report_(sta->report())
  {
  }

  std::vector<uint8_t> bytes();

private:
  void writeSet(Parasitics *parasitics);
  void writeNetworks(Parasitics *parasitics);
  void writeNodes(Parasitics *parasitics,
                  const Parasitic *parasitic,
                  // Return value.
                  std::map<const ParasiticNode *, uint32_t> &node_ids);
  uint32_t nodeId(const std::map<const ParasiticNode *, uint32_t> &node_ids,
                  const ParasiticNode *node);
  void writePiModels(Parasitics *parasitics);

  DbWriter w_;            // body bytes + unique string list
  Sta *sta_;
  Network *network_;
  Report *report_;
  std::vector<const Net *> nets_;
  std::vector<const Pin *> pins_;
};

////////////////////////////////////////////////////////////////

void
ParaWriter::writeNodes(Parasitics *parasitics,
                       const Parasitic *parasitic,
                       std::map<const ParasiticNode *, uint32_t> &node_ids)
{
  // A node is either a pin of the netlist or a numbered point inside a net.
  ParasiticNodeSeq nodes = parasitics->nodes(parasitic);
  w_.u32(static_cast<uint32_t>(nodes.size()));
  for (const ParasiticNode *node : nodes) {
    node_ids.emplace(node, static_cast<uint32_t>(node_ids.size()));
    const Pin *pin = parasitics->pin(node);
    if (pin) {
      w_.u8(static_cast<uint8_t>(ParaDbNodeKind::pin));
      w_.str(network_->pathName(pin));
    }
    else {
      w_.u8(static_cast<uint8_t>(ParaDbNodeKind::net));
      w_.str(network_->pathName(parasitics->net(node, network_)));
      w_.u32(parasitics->netId(node));
    }
    w_.f32(parasitics->nodeGndCap(node));
  }
}

uint32_t
ParaWriter::nodeId(const std::map<const ParasiticNode *, uint32_t> &node_ids,
                   const ParasiticNode *node)
{
  auto itr = node_ids.find(node);
  if (itr == node_ids.end())
    report_->error(1440, "parasitic node is not on the net it belongs to.");
  return itr->second;
}

void
ParaWriter::writeNetworks(Parasitics *parasitics)
{
  std::vector<const Net *> nets;
  for (const Net *net : nets_) {
    if (parasitics->findParasiticNetwork(net))
      nets.push_back(net);
  }

  w_.u32(static_cast<uint32_t>(nets.size()));
  for (const Net *net : nets) {
    const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
    w_.str(network_->pathName(net));
    w_.boolean(parasitics->includesPinCaps(parasitic));
    w_.boolean(parasitics->isReducedParasiticNetwork(parasitic));

    std::map<const ParasiticNode *, uint32_t> node_ids;
    writeNodes(parasitics, parasitic, node_ids);

    ParasiticResistorSeq resistors = parasitics->resistors(parasitic);
    w_.u32(static_cast<uint32_t>(resistors.size()));
    for (const ParasiticResistor *resistor : resistors) {
      w_.u32(parasitics->id(resistor));
      w_.f32(parasitics->value(resistor));
      w_.u32(nodeId(node_ids, parasitics->node1(resistor)));
      w_.u32(nodeId(node_ids, parasitics->node2(resistor)));
    }

    // Coupling capacitors only appear when read_spef kept them; otherwise the
    // capacitance is already folded into the node caps above.
    ParasiticCapacitorSeq capacitors = parasitics->capacitors(parasitic);
    w_.u32(static_cast<uint32_t>(capacitors.size()));
    for (const ParasiticCapacitor *capacitor : capacitors) {
      w_.u32(parasitics->id(capacitor));
      w_.f32(parasitics->value(capacitor));
      w_.u32(nodeId(node_ids, parasitics->node1(capacitor)));
      w_.u32(nodeId(node_ids, parasitics->node2(capacitor)));
    }
  }
}

void
ParaWriter::writePiModels(Parasitics *parasitics)
{
  // Pi models hang off a driver pin for one transition and one min/max, so ask
  // every pin for all four combinations.
  struct PiModel
  {
    const Pin *drvr_pin;
    const RiseFall *rf;
    const MinMax *min_max;
    const Parasitic *parasitic;
  };
  std::vector<PiModel> models;
  for (const Pin *pin : pins_) {
    for (const RiseFall *rf : RiseFall::range()) {
      for (const MinMax *min_max : MinMax::range()) {
        const Parasitic *parasitic = parasitics->findPiElmore(pin, rf, min_max);
        if (parasitic)
          models.push_back({pin, rf, min_max, parasitic});
      }
    }
  }

  w_.u32(static_cast<uint32_t>(models.size()));
  for (const PiModel &model : models) {
    w_.str(network_->pathName(model.drvr_pin));
    w_.u8(static_cast<uint8_t>(model.rf->index()));
    w_.u8(static_cast<uint8_t>(model.min_max->index()));
    float c2 = 0.0;
    float rpi = 0.0;
    float c1 = 0.0;
    parasitics->piModel(model.parasitic, c2, rpi, c1);
    w_.f32(c2);
    w_.f32(rpi);
    w_.f32(c1);

    // One elmore delay from the driver to each load it reaches.
    std::vector<std::pair<const Pin *, float>> elmores;
    for (const Pin *load_pin : parasitics->loads(model.drvr_pin)) {
      float elmore = 0.0;
      bool exists = false;
      parasitics->findElmore(model.parasitic, load_pin, elmore, exists);
      if (exists)
        elmores.emplace_back(load_pin, elmore);
    }
    w_.u32(static_cast<uint32_t>(elmores.size()));
    for (const auto &[load_pin, elmore] : elmores) {
      w_.str(network_->pathName(load_pin));
      w_.f32(elmore);
    }
  }
}

void
ParaWriter::writeSet(Parasitics *parasitics)
{
  w_.str(parasitics->name());
  w_.str(parasitics->filename());
  w_.f32(parasitics->couplingCapFactor());
  writeNetworks(parasitics);
  writePiModels(parasitics);
}

std::vector<uint8_t>
ParaWriter::bytes()
{
  const Instance *top = network_->topInstance();
  if (top)
    findNetsAndPins(network_, top, nets_, pins_);

  // The scene can point both min and max at the same set, so write each set
  // once and note which one answers for which.
  const Scene *scene = sta_->cmdScene();
  std::vector<Parasitics *> sets;
  uint32_t min_max_set[MinMax::index_count];
  for (const MinMax *min_max : MinMax::range()) {
    Parasitics *parasitics = scene ? scene->parasitics(min_max) : nullptr;
    uint32_t id = para_db_id_null;
    if (parasitics && parasitics->haveParasitics()) {
      auto itr = std::find(sets.begin(), sets.end(), parasitics);
      if (itr == sets.end()) {
        id = static_cast<uint32_t>(sets.size());
        sets.push_back(parasitics);
      }
      else
        id = static_cast<uint32_t>(itr - sets.begin());
    }
    min_max_set[min_max->index()] = id;
  }

  w_.u32(static_cast<uint32_t>(sets.size()));
  for (Parasitics *parasitics : sets)
    writeSet(parasitics);
  for (const MinMax *min_max : MinMax::range())
    w_.u32(min_max_set[min_max->index()]);
  return dbPack(w_);
}

std::vector<uint8_t>
writeParaDbBytes(Sta *sta)
{
  ParaWriter writer(sta);
  return writer.bytes();
}

////////////////////////////////////////////////////////////////

// Inverse of ParaWriter: read body fields in the same order and rebuild the
// RC networks through the same calls the spef reader uses.
class ParaLoader
{
public:
  ParaLoader(DbReader &r,
             std::string_view label,
             Sta *sta) :
    r_(r),
    label_(label),
    sta_(sta),
    network_(sta->network()),
    report_(sta->report())
  {
  }

  void read();

private:
  Parasitics *readSet();
  void readNetworks(Parasitics *parasitics);
  ParasiticNode *readNode(Parasitics *parasitics,
                          Parasitic *parasitic);
  void readPiModels(Parasitics *parasitics);
  const Pin *findPin(const std::string &path);

  DbReader &r_;                 // bookmark over body bytes + string list
  std::string label_;
  Sta *sta_;
  Network *network_;
  Report *report_;
};

const Pin *
ParaLoader::findPin(const std::string &path)
{
  const Pin *pin = network_->findPin(path);
  if (pin == nullptr)
    report_->error(1441, "{} names missing pin {}.", label_, path);
  return pin;
}

ParasiticNode *
ParaLoader::readNode(Parasitics *parasitics,
                     Parasitic *parasitic)
{
  ParaDbNodeKind kind = static_cast<ParaDbNodeKind>(r_.u8());
  ParasiticNode *node = nullptr;
  if (kind == ParaDbNodeKind::pin)
    node = parasitics->ensureParasiticNode(parasitic, findPin(r_.str()),
                                           network_);
  else {
    const std::string &path = r_.str();
    uint32_t id = r_.u32();
    const Net *net = network_->findNet(path);
    if (net == nullptr)
      report_->error(1442, "{} names missing net {}.", label_, path);
    node = parasitics->ensureParasiticNode(parasitic, net, id, network_);
  }
  // Nodes start with no capacitance, so this sets it rather than adding to it.
  parasitics->incrCap(node, r_.f32());
  return node;
}

void
ParaLoader::readNetworks(Parasitics *parasitics)
{
  uint32_t net_count = r_.u32();
  for (uint32_t i = 0; i < net_count; i++) {
    const std::string &path = r_.str();
    bool includes_pin_caps = r_.boolean();
    bool is_reduced = r_.boolean();
    const Net *net = network_->findNet(path);
    if (net == nullptr)
      report_->error(1442, "{} names missing net {}.", label_, path);
    Parasitic *parasitic = parasitics->makeParasiticNetwork(net,
                                                            includes_pin_caps);
    parasitics->setIsReducedParasiticNetwork(parasitic, is_reduced);

    uint32_t node_count = r_.u32();
    std::vector<ParasiticNode *> nodes;
    nodes.reserve(node_count);
    for (uint32_t n = 0; n < node_count; n++)
      nodes.push_back(readNode(parasitics, parasitic));

    auto node = [&](uint32_t id) -> ParasiticNode * {
      if (id >= nodes.size())
        report_->error(1443, "{} has a bad parasitic node reference.", label_);
      return nodes[id];
    };
    uint32_t resistor_count = r_.u32();
    for (uint32_t res = 0; res < resistor_count; res++) {
      uint32_t id = r_.u32();
      float value = r_.f32();
      ParasiticNode *node1 = node(r_.u32());
      parasitics->makeResistor(parasitic, id, value, node1, node(r_.u32()));
    }
    uint32_t capacitor_count = r_.u32();
    for (uint32_t cap = 0; cap < capacitor_count; cap++) {
      uint32_t id = r_.u32();
      float value = r_.f32();
      ParasiticNode *node1 = node(r_.u32());
      parasitics->makeCapacitor(parasitic, id, value, node1, node(r_.u32()));
    }
  }
}

void
ParaLoader::readPiModels(Parasitics *parasitics)
{
  uint32_t model_count = r_.u32();
  for (uint32_t i = 0; i < model_count; i++) {
    const Pin *drvr_pin = findPin(r_.str());
    const RiseFall *rf = RiseFall::find(r_.u8());
    const MinMax *min_max = MinMax::find(r_.u8());
    float c2 = r_.f32();
    float rpi = r_.f32();
    float c1 = r_.f32();
    Parasitic *parasitic = parasitics->makePiElmore(drvr_pin, rf, min_max,
                                                    c2, rpi, c1);
    uint32_t elmore_count = r_.u32();
    for (uint32_t e = 0; e < elmore_count; e++) {
      const Pin *load_pin = findPin(r_.str());
      parasitics->setElmore(parasitic, load_pin, r_.f32());
    }
  }
}

Parasitics *
ParaLoader::readSet()
{
  const std::string &name = r_.str();
  const std::string &filename = r_.str();
  float coupling_cap_factor = r_.f32();
  // Reuse the set of the same name if the session already has one; its
  // contents were dropped by the clear that precedes loading.
  Parasitics *parasitics = sta_->findParasitics(name);
  if (parasitics == nullptr)
    parasitics = sta_->makeConcreteParasitics(name, filename);
  parasitics->setCouplingCapFactor(coupling_cap_factor);
  readNetworks(parasitics);
  readPiModels(parasitics);
  return parasitics;
}

void
ParaLoader::read()
{
  uint32_t set_count = r_.u32();
  std::vector<Parasitics *> sets;
  for (uint32_t i = 0; i < set_count; i++)
    sets.push_back(readSet());

  Scene *scene = sta_->cmdScene();
  for (const MinMax *min_max : MinMax::range()) {
    uint32_t id = r_.u32();
    if (id != para_db_id_null && scene) {
      if (id >= sets.size())
        report_->error(1444, "{} has a bad parasitics reference.", label_);
      scene->setParasitics(sets[id], min_max == MinMax::min()
                           ? MinMaxAll::min()
                           : MinMaxAll::max());
    }
  }

  // DbReader sets this if we tried to read past the end of the body.
  if (r_.failed())
    report_->error(1445, "{} is truncated or corrupt.", label_);
}

void
readParaDbBytes(const uint8_t *data,
                size_t size,
                std::string_view label,
                Sta *sta)
{
  std::vector<std::string> strings;
  std::vector<uint8_t> body;
  if (!dbUnpack(data, size, strings, body))
    sta->report()->error(1446, "{} has a corrupt string table.", label);

  DbReader reader(body.data(), body.size(), &strings);
  ParaLoader loader(reader, label, sta);
  loader.read();
}

} // namespace sta
