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

// Write the linked network graph as bytes.
//
// Body field order must match NetLoader in NetDbReader.cc.
// Objects are numbered by the order they are written, and every reference
// (parent instance, net on a pin, cell of an instance) is one of those
// numbers. Liberty cells are the exception: they are named, because the
// liberty sections of the .stadb are loaded before the netlist section.
// Strings: body stores an index; the string table holds the text once.

#include "NetDb.hh"

#include <map>
#include <string>
#include <vector>

#include "Liberty.hh"
#include "Network.hh"
#include "PatternMatch.hh"
#include "PortDirection.hh"
#include "Report.hh"
#include "liberty/LibDb.hh"

namespace sta {

static uint8_t
directionCode(const PortDirection *dir)
{
  // PortDirection is a pointer in RAM; store a small stable number instead.
  if (dir == PortDirection::input()) return 0;
  if (dir == PortDirection::output()) return 1;
  if (dir == PortDirection::tristate()) return 2;
  if (dir == PortDirection::bidirect()) return 3;
  if (dir == PortDirection::internal()) return 4;
  if (dir == PortDirection::ground()) return 5;
  if (dir == PortDirection::power()) return 6;
  if (dir == PortDirection::well()) return 7;
  return 8;
}

// Walks the linked network and appends body bytes via DbWriter.
class NetWriter
{
public:
  NetWriter(Network *network,
            Report *report) :
    network_(network),
    edit_(dynamic_cast<NetworkEdit *>(network)),
    report_(report)
  {
  }

  std::vector<uint8_t> bytes();

private:
  void collect();
  void collectInstance(const Instance *inst);
  void collectConstantNets();
  void writeDesignCells();
  void writePorts(const Cell *cell);
  void writeAttributes(const AttributeMap &attributes);
  void writeInstances();
  void writeNets();
  void writePins();
  uint32_t instanceId(const Instance *inst) const;
  uint32_t netId(const Net *net) const;

  DbWriter w_;            // body bytes + unique string list
  Network *network_;
  NetworkEdit *edit_;     // null if this network cannot merge nets
  Report *report_;
  // Everything is referenced by position in these lists.
  std::vector<const Cell *> design_cells_;
  std::vector<const Instance *> instances_;
  std::vector<const Net *> nets_;
  std::vector<const Pin *> pins_;
  std::map<const Cell *, uint32_t> design_cell_ids_;
  std::map<const Instance *, uint32_t> instance_ids_;
  std::map<const Net *, uint32_t> net_ids_;
  std::map<const Net *, NetDbConstant> net_constants_;
};

////////////////////////////////////////////////////////////////
// Pass 1: number every object before writing, so a reference is just an index.

void
NetWriter::collect()
{
  const Instance *top = network_->topInstance();
  if (top == nullptr)
    report_->error(1410, "design is not linked; cannot write the netlist.");
  collectInstance(top);

  // Nets belong to the instance they were declared in. Matching by name rather
  // than using the net iterator also picks up nets merged into another one by
  // a verilog assign, whose names constraints can still refer to.
  PatternMatch any_name("*");
  for (const Instance *inst : instances_) {
    NetSeq inst_nets;
    network_->findInstNetsMatching(inst, &any_name, inst_nets);
    for (const Net *net : inst_nets) {
      net_ids_.emplace(net, static_cast<uint32_t>(nets_.size()));
      nets_.push_back(net);
    }
  }
  collectConstantNets();
}

void
NetWriter::collectInstance(const Instance *inst)
{
  // Parent before child, so the reader always has the parent in hand.
  instance_ids_.emplace(inst, static_cast<uint32_t>(instances_.size()));
  instances_.push_back(inst);

  // Cells without a liberty model (modules, black boxes) are written out in
  // full; liberty cells already came in from the liberty sections.
  const Cell *cell = network_->cell(inst);
  if (network_->libertyCell(cell) == nullptr
      && !design_cell_ids_.contains(cell)) {
    design_cell_ids_.emplace(cell, static_cast<uint32_t>(design_cells_.size()));
    design_cells_.push_back(cell);
  }

  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext())
    pins_.push_back(pin_iter->next());
  delete pin_iter;

  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext())
    collectInstance(child_iter->next());
  delete child_iter;
}

void
NetWriter::collectConstantNets()
{
  // The network knows which pins are tied high/low; record it on their nets.
  ConstantPinIterator *const_iter = network_->constantPinIterator();
  while (const_iter->hasNext()) {
    const Pin *pin = nullptr;
    LogicValue value = LogicValue::unknown;
    const_iter->next(pin, value);
    const Net *net = network_->net(pin);
    if (net == nullptr) {
      const Term *term = network_->term(pin);
      net = term ? network_->net(term) : nullptr;
    }
    if (net && net_ids_.contains(net))
      net_constants_[net] = value == LogicValue::zero
        ? NetDbConstant::zero
        : NetDbConstant::one;
  }
  delete const_iter;
}

uint32_t
NetWriter::instanceId(const Instance *inst) const
{
  auto itr = instance_ids_.find(inst);
  if (itr == instance_ids_.end())
    report_->error(1411, "instance {} is not in the netlist.",
                   network_->pathName(inst));
  return itr->second;
}

uint32_t
NetWriter::netId(const Net *net) const
{
  auto itr = net_ids_.find(net);
  if (itr == net_ids_.end())
    report_->error(1412, "net {} is not in the netlist.",
                   network_->pathName(net));
  return itr->second;
}

////////////////////////////////////////////////////////////////
// Pass 2: write the numbered objects.

void
NetWriter::writeAttributes(const AttributeMap &attributes)
{
  w_.u32(static_cast<uint32_t>(attributes.size()));
  for (const auto &[key, value] : attributes) {
    w_.str(key);
    w_.str(value);
  }
}

void
NetWriter::writePorts(const Cell *cell)
{
  std::vector<Port *> ports;
  CellPortIterator *port_iter = network_->portIterator(cell);
  while (port_iter->hasNext())
    ports.push_back(port_iter->next());
  delete port_iter;

  w_.u32(static_cast<uint32_t>(ports.size()));
  for (Port *port : ports) {
    // Buses and bundles are one port here; their bits are rebuilt from the
    // range or the member names.
    if (network_->isBus(port)) {
      w_.u8(static_cast<uint8_t>(NetDbPortKind::bus));
      w_.str(network_->name(port));
      w_.i32(network_->fromIndex(port));
      w_.i32(network_->toIndex(port));
    }
    else if (network_->isBundle(port)) {
      w_.u8(static_cast<uint8_t>(NetDbPortKind::bundle));
      w_.str(network_->name(port));
      std::vector<std::string> members;
      PortMemberIterator *member_iter = network_->memberIterator(port);
      while (member_iter->hasNext())
        members.push_back(network_->name(member_iter->next()));
      delete member_iter;
      w_.u32(static_cast<uint32_t>(members.size()));
      for (const std::string &member : members)
        w_.str(member);
    }
    else {
      w_.u8(static_cast<uint8_t>(NetDbPortKind::scalar));
      w_.str(network_->name(port));
    }
    w_.u8(directionCode(network_->direction(port)));
  }
}

void
NetWriter::writeDesignCells()
{
  w_.u32(static_cast<uint32_t>(design_cells_.size()));
  for (const Cell *cell : design_cells_) {
    const Library *library = network_->library(cell);
    w_.str(library ? network_->name(library) : "");
    w_.str(network_->name(cell));
    w_.boolean(network_->isLeaf(cell));
    w_.str(network_->filename(cell));
    writeAttributes(network_->attributeMap(cell));
    writePorts(cell);
  }
}

void
NetWriter::writeInstances()
{
  w_.u32(static_cast<uint32_t>(instances_.size()));
  for (const Instance *inst : instances_) {
    w_.str(network_->name(inst));
    const Instance *parent = network_->parent(inst);
    // The top instance has no parent, which is how the reader spots it.
    w_.u32(parent ? instanceId(parent) : net_db_id_null);

    const Cell *cell = network_->cell(inst);
    const LibertyCell *liberty_cell = network_->libertyCell(cell);
    if (liberty_cell) {
      w_.u8(static_cast<uint8_t>(NetDbCellKind::liberty));
      w_.str(liberty_cell->libertyLibrary()->name());
      w_.str(liberty_cell->name());
    }
    else {
      w_.u8(static_cast<uint8_t>(NetDbCellKind::design));
      w_.u32(design_cell_ids_.at(cell));
    }
    writeAttributes(network_->attributeMap(inst));
  }
}

void
NetWriter::writeNets()
{
  w_.u32(static_cast<uint32_t>(nets_.size()));
  for (const Net *net : nets_) {
    w_.u32(instanceId(network_->instance(net)));
    w_.str(network_->name(net));
    auto itr = net_constants_.find(net);
    w_.u8(static_cast<uint8_t>(itr == net_constants_.end()
                               ? NetDbConstant::none
                               : itr->second));
    // A merged net is an alias: it keeps its name but its pins live on the
    // net it was merged into.
    const Net *merged_into = edit_
      ? edit_->mergedInto(const_cast<Net *>(net))
      : nullptr;
    w_.u32(merged_into ? netId(merged_into) : net_db_id_null);
  }
}

void
NetWriter::writePins()
{
  // A pin is the edge between an instance port and a net. Hierarchical pins
  // sit on two nets: the one outside the instance and the one inside it,
  // which the network reaches through a term.
  w_.u32(static_cast<uint32_t>(pins_.size()));
  for (const Pin *pin : pins_) {
    w_.u32(instanceId(network_->instance(pin)));
    w_.str(network_->portName(pin));
    const Net *net = network_->net(pin);
    w_.u32(net ? netId(net) : net_db_id_null);
    const Term *term = network_->term(pin);
    const Net *term_net = term ? network_->net(term) : nullptr;
    w_.u32(term_net ? netId(term_net) : net_db_id_null);
  }
}

std::vector<uint8_t>
NetWriter::bytes()
{
  collect();
  // Names carry the divider/escape convention of the session that wrote them.
  w_.u8(static_cast<uint8_t>(network_->pathDivider()));
  w_.u8(static_cast<uint8_t>(network_->pathEscape()));
  // Cells first: ports must exist before instances and pins refer to them.
  writeDesignCells();
  writeInstances();
  writeNets();
  writePins();
  return dbPack(w_);
}

std::vector<uint8_t>
writeNetDbBytes(Network *network,
                Report *report)
{
  NetWriter writer(network, report);
  return writer.bytes();
}

} // namespace sta
