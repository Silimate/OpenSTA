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

// Rebuild the network graph from bytes.
//
// Body field order must match NetWriter in NetDbWriter.cc. Objects are made in
// the order they were written, so the position of an object in the file is its
// position in the lists here, and a reference is just a lookup.
// The network is linked when this finishes: the instance tree, nets and pins
// are all in place and the top instance is set, so link_design is not needed.

#include "NetDb.hh"

#include <string>
#include <utility>
#include <vector>

#include "ConcreteNetwork.hh"
#include "Format.hh"
#include "GeneratedClock.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Report.hh"
#include "liberty/LibDb.hh"

namespace sta {

using AttributePairs = std::vector<std::pair<std::string, std::string>>;

static PortDirection *
directionFromCode(uint8_t code)
{
  // Inverse of directionCode() in the writer.
  switch (code) {
  case 0: return PortDirection::input();
  case 1: return PortDirection::output();
  case 2: return PortDirection::tristate();
  case 3: return PortDirection::bidirect();
  case 4: return PortDirection::internal();
  case 5: return PortDirection::ground();
  case 6: return PortDirection::power();
  case 7: return PortDirection::well();
  default: return PortDirection::unknown();
  }
}

// Inverse of NetWriter: read body fields in the same order and build objects.
class NetLoader
{
public:
  NetLoader(DbReader &r,
            std::string_view label,
            NetworkReader *network) :
    r_(r),
    label_(label),
    network_(network),
    report_(network->report())
  {
  }

  void read();

private:
  void readDesignCells();
  void readPorts(Cell *cell);
  AttributePairs readAttributes();
  void readInstances();
  void readNets();
  void readPins();
  void makeGeneratedClocks(const LibertyCell *cell,
                           Instance *inst);
  Cell *designCell(uint32_t id);
  Instance *instance(uint32_t id);
  Net *net(uint32_t id);

  DbReader &r_;                 // bookmark over body bytes + string list
  std::string label_;
  NetworkReader *network_;
  Report *report_;

  // Objects made so far (index = the number written in the file).
  std::vector<Cell *> design_cells_;
  std::vector<Instance *> instances_;
  std::vector<Net *> nets_;
  // Net alias pairs, applied once everything else exists.
  std::vector<std::pair<uint32_t, uint32_t>> merges_;
};

////////////////////////////////////////////////////////////////
// References are positions in the lists above.

Cell *
NetLoader::designCell(uint32_t id)
{
  if (id >= design_cells_.size())
    report_->error(1413, "{} has a bad cell reference.", label_);
  return design_cells_[id];
}

Instance *
NetLoader::instance(uint32_t id)
{
  if (id >= instances_.size())
    report_->error(1414, "{} has a bad instance reference.", label_);
  return instances_[id];
}

Net *
NetLoader::net(uint32_t id)
{
  // A null id is a real answer here: unconnected pin, or a pin with no term.
  if (id == net_db_id_null)
    return nullptr;
  if (id >= nets_.size())
    report_->error(1415, "{} has a bad net reference.", label_);
  return nets_[id];
}

////////////////////////////////////////////////////////////////

AttributePairs
NetLoader::readAttributes()
{
  AttributePairs attributes;
  uint32_t count = r_.u32();
  for (uint32_t i = 0; i < count; i++) {
    const std::string &key = r_.str();
    const std::string &value = r_.str();
    attributes.emplace_back(key, value);
  }
  return attributes;
}

void
NetLoader::readPorts(Cell *cell)
{
  uint32_t count = r_.u32();
  for (uint32_t i = 0; i < count; i++) {
    NetDbPortKind kind = static_cast<NetDbPortKind>(r_.u8());
    const std::string &name = r_.str();
    Port *port = nullptr;
    if (kind == NetDbPortKind::bus) {
      // The bus port makes its own bits, matching what the writer walked.
      int32_t from_index = r_.i32();
      int32_t to_index = r_.i32();
      port = network_->makeBusPort(cell, name, from_index, to_index);
    }
    else if (kind == NetDbPortKind::bundle) {
      // The cell owns the member ports; the bundle owns the list holding them.
      uint32_t member_count = r_.u32();
      PortSeq *members = new PortSeq;
      for (uint32_t m = 0; m < member_count; m++) {
        const std::string &member_name = r_.str();
        Port *member = network_->findPort(cell, member_name);
        if (member == nullptr)
          member = network_->makePort(cell, member_name);
        members->push_back(member);
      }
      port = network_->makeBundlePort(cell, name, members);
    }
    else
      port = network_->makePort(cell, name);
    // Setting the direction of a bus or bundle also sets its members.
    network_->setDirection(port, directionFromCode(r_.u8()));
  }
}

void
NetLoader::readDesignCells()
{
  uint32_t count = r_.u32();
  design_cells_.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    const std::string &library_name = r_.str();
    const std::string &cell_name = r_.str();
    bool is_leaf = r_.boolean();
    const std::string &filename = r_.str();

    // Reuse the library if the session already has one by that name.
    Library *library = network_->findLibrary(library_name);
    if (library == nullptr)
      library = network_->makeLibrary(library_name, filename);
    Cell *cell = network_->makeCell(library, cell_name, is_leaf, filename);
    for (const auto &[key, value] : readAttributes())
      network_->setAttribute(cell, key, value);
    readPorts(cell);
    design_cells_.push_back(cell);
  }
}

void
NetLoader::makeGeneratedClocks(const LibertyCell *cell,
                               Instance *inst)
{
  // Same map the verilog reader fills in: source clock pin path -> cell that
  // declares the generated clock. Derived from liberty, so it is not in the file.
  if (cell->generatedClocks().empty())
    return;
  std::string inst_path = network_->pathName(inst);
  std::string_view path = inst_path;
  // Drop the top instance name, which is not part of a pin path.
  size_t divider = path.find(network_->pathDivider());
  if (divider != std::string_view::npos)
    path = path.substr(divider + 1);
  for (GeneratedClock *generated_clock : cell->generatedClocks()) {
    std::string pin_path = sta::format("{}/{}", path,
                                       generated_clock->masterPin());
    network_->addGeneratedClockPinToCell(pin_path.c_str(),
                                         const_cast<LibertyCell *>(cell));
  }
}

void
NetLoader::readInstances()
{
  uint32_t count = r_.u32();
  instances_.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    const std::string &name = r_.str();
    uint32_t parent_id = r_.u32();
    // No parent means this is the top instance, which is always written first.
    Instance *parent = parent_id == net_db_id_null
      ? nullptr
      : instance(parent_id);

    NetDbCellKind kind = static_cast<NetDbCellKind>(r_.u8());
    Cell *cell = nullptr;
    LibertyCell *liberty_cell = nullptr;
    if (kind == NetDbCellKind::liberty) {
      // Liberty cells came in from the liberty sections, so look them up.
      const std::string &library_name = r_.str();
      const std::string &cell_name = r_.str();
      LibertyLibrary *library = network_->findLiberty(library_name);
      liberty_cell = library
        ? library->findLibertyCell(cell_name)
        : network_->findLibertyCell(cell_name);
      if (liberty_cell == nullptr)
        report_->error(1416, "{} instance {} uses missing liberty cell {}.",
                       label_, name, cell_name);
      cell = network_->cell(liberty_cell);
    }
    else
      cell = designCell(r_.u32());

    Instance *inst = network_->makeInstance(cell, name, parent);
    for (const auto &[key, value] : readAttributes())
      network_->setAttribute(inst, key, value);
    if (liberty_cell)
      makeGeneratedClocks(liberty_cell, inst);
    instances_.push_back(inst);
  }
}

void
NetLoader::readNets()
{
  uint32_t count = r_.u32();
  nets_.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    Instance *inst = instance(r_.u32());
    const std::string &name = r_.str();
    NetDbConstant constant = static_cast<NetDbConstant>(r_.u8());
    uint32_t merged_into = r_.u32();
    Net *net = network_->makeNet(name, inst);
    if (constant != NetDbConstant::none)
      network_->addConstantNet(net, constant == NetDbConstant::zero
                               ? LogicValue::zero
                               : LogicValue::one);
    // Merging moves pins, so wait until every net and pin is in place.
    if (merged_into != net_db_id_null)
      merges_.emplace_back(static_cast<uint32_t>(nets_.size()), merged_into);
    nets_.push_back(net);
  }
}

void
NetLoader::readPins()
{
  uint32_t count = r_.u32();
  for (uint32_t i = 0; i < count; i++) {
    Instance *inst = instance(r_.u32());
    const std::string &port_name = r_.str();
    Net *pin_net = net(r_.u32());
    Net *term_net = net(r_.u32());
    Port *port = network_->findPort(network_->cell(inst), port_name);
    if (port == nullptr)
      report_->error(1417, "{} instance {} has no port {}.",
                     label_, network_->name(inst), port_name);
    Pin *pin = network_->makePin(inst, port, pin_net);
    // A term is the inside end of a pin on a hierarchical instance, and how
    // a top level port pin reaches its net.
    if (term_net)
      network_->makeTerm(pin, term_net);
  }
}

void
NetLoader::read()
{
  // Drop whatever netlist was loaded before so nothing here collides with it.
  network_->readNetlistBefore();

  network_->setPathDivider(static_cast<char>(r_.u8()));
  network_->setPathEscape(static_cast<char>(r_.u8()));
  readDesignCells();
  readInstances();
  readNets();
  readPins();
  for (const auto &[net_id, into_id] : merges_)
    network_->mergeInto(net(net_id), net(into_id));

  // DbReader sets this if we tried to read past the end of the body.
  if (r_.failed())
    report_->error(1418, "{} is truncated or corrupt.", label_);
  if (instances_.empty())
    report_->error(1419, "{} has no top instance.", label_);

  // Setting the top instance is what makes the network count as linked.
  ConcreteNetwork *concrete = dynamic_cast<ConcreteNetwork *>(network_);
  if (concrete == nullptr)
    report_->error(1420, "{} needs a concrete network to load into.", label_);
  concrete->setTopInstance(instances_[0]);
  network_->checkNetworkLibertyScenes();
}

void
readNetDbBytes(const uint8_t *data,
               size_t size,
               std::string_view label,
               NetworkReader *network)
{
  // Split the block into the string list and body, then rebuild the network.
  std::vector<std::string> strings;
  std::vector<uint8_t> body;
  if (!dbUnpack(data, size, strings, body))
    network->report()->error(1421, "{} has a corrupt string table.", label);

  DbReader reader(body.data(), body.size(), &strings);
  NetLoader loader(reader, label, network);
  loader.read();
}

} // namespace sta
