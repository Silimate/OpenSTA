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

#include <functional>

#include "Map.hh"
#include "StringUtil.hh"
#include "LibertyClass.hh"
#include "VertexId.hh"
#include "NetworkClass.hh"
#include "StaState.hh"

namespace sta {

class Report;
class PatternMatch;
class PinVisitor;

typedef Map<const char*, LibertyLibrary*, CharPtrLess> LibertyLibraryMap;
// Link network function returns top level instance.
// Return nullptr if link fails.
typedef std::function<Instance* (const char *top_cell_name,
                                 bool make_black_boxes)> LinkNetworkFunc;
typedef Map<const Net*, PinSet*> NetDrvrPinsMap;

// The Network class defines the network API used by sta.
// The interface to a network implementation is constructed by
// deriving a class from it.  This interface class is known as a
// ADAPTER/DELEGATE.  An instance of the adapter is used to
// translate calls in the sta to a target network implementation.

// Network data types:
//   Libraries are collections of cells.
//   Cells are masters (ala verilog module or liberty cell).
//   Ports define the connections to a cell.
//     There are four different sub-classes of ports:
//       Simple non-bus ports such as "enable".
//       Bundles of simple or single bit ports, such as D bundling D0, D1.
//       Buses such as D[3:0]
//       Bus bit ports, which are one bit of a bus port such as D[0].
//         Bus bit ports are always members of a bus.
//   Instances are calls of cells in the design hierarchy
//     Hierarchical and leaf instances are in the network.
//     At the top of the hierarchy is a top level instance that
//     has instances for the top level netlist.
//   Pins are a connection between an instance and a net corresponding
//     to a port.  Ports on the top level instance also have pins in
//     the network.
//   Terminals are a connection between a net and a pin on the parent
//     instance.
//   Nets connect pins at a level of hierarchy.  Both hierarchical
//     instance pins and leaf instance pins are connected by net.
//
// The network API only contains pointers/handles to these data types.
// The adapter casts these handles to its native data types and calls
// the appropriate code to implement the member functions of the Network
// class.
//
// Pattern arguments used by lookup find*Matching functions
// use a simple unix shell or tcl "string match" style pattern matching:
//  '*' matches zero or more characters
//  '?' matches any character
//
// Note that some of the network member functions are not virtual because
// they can be implemented in terms of the other functions.
// Only the pure virtual functions MUST be implemented by a derived class.

class Network : public StaState
{
public:
  Network();
  virtual ~Network();
  virtual void clear();

  // Linking the hierarchy creates the instance/pin/net network hierarchy.
  // Most network functions are only useful after the hierarchy
  // has been linked.  When the network interfaces to an external database,
  // linking is not necessary because the network has already been expanded.
  // Return true if successful.
  virtual bool linkNetwork(const char *top_cell_name,
			   bool make_black_boxes,
			   Report *report) = 0;
  virtual bool isLinked() const;
  virtual bool isEditable() const { return false; }

  ////////////////////////////////////////////////////////////////
  // Library functions.
  virtual const char *name(const Library *library) const = 0;
  virtual ObjectId id(const Library *library) const = 0;
  virtual LibraryIterator *libraryIterator() const = 0;
  virtual LibertyLibraryIterator *libertyLibraryIterator() const = 0;
  virtual Library *findLibrary(const char *name) = 0;
  virtual LibertyLibrary *findLiberty(const char *name) = 0;
  // Find liberty library by filename.
  virtual LibertyLibrary *findLibertyFilename(const char *filename);
  virtual Cell *findCell(const Library *library,
			 const char *name) const = 0;
  // Search the design (non-liberty) libraries for cells matching pattern.
  virtual CellSeq findCellsMatching(const Library *library,
                                    const PatternMatch *pattern) const = 0;
  // Search liberty libraries for cell name.
  virtual LibertyCell *findLibertyCell(const char *name) const;
  virtual LibertyLibrary *makeLibertyLibrary(const char *name,
					     const char *filename) = 0;
  // Hook for network after reading liberty library.
  virtual void readLibertyAfter(LibertyLibrary *library);
  // First liberty library read is used to look up defaults.
  // This corresponds to a link_path of '*'.
  virtual LibertyLibrary *defaultLibertyLibrary() const;
  void setDefaultLibertyLibrary(LibertyLibrary *library);
  // Check liberty cells used by the network to make sure they exist
  // for all the defined corners.
  void checkNetworkLibertyCorners();
  // Check liberty cells to make sure they exist for all the defined corners.
  void checkLibertyCorners();

  ////////////////////////////////////////////////////////////////
  // Cell functions.
  virtual const char *name(const Cell *cell) const = 0;
  virtual ObjectId id(const Cell *cell) const = 0;
  virtual Library *library(const Cell *cell) const = 0;
  virtual LibertyLibrary *libertyLibrary(const Cell *cell) const;
  // Find the corresponding liberty cell.
  virtual const LibertyCell *libertyCell(const Cell *cell) const = 0;
  virtual LibertyCell *libertyCell(Cell *cell) const = 0;
  virtual const Cell *cell(const LibertyCell *cell) const = 0;
  virtual Cell *cell(LibertyCell *cell) const = 0;
  // Filename may return null.
  virtual const char *filename(const Cell *cell) = 0;
  // Attributes can be null
  virtual std::string getAttribute(const Cell *cell,
                                   const std::string &key) const = 0;
  virtual const AttributeMap &attributeMap(const Cell *cell) const = 0;
  // Name can be a simple, bundle, bus, or bus bit name.
  virtual Port *findPort(const Cell *cell,
			 const char *name) const = 0;
  virtual PortSeq findPortsMatching(const Cell *cell,
                                    const PatternMatch *pattern) const;
  virtual bool isLeaf(const Cell *cell) const = 0;
  virtual CellPortIterator *portIterator(const Cell *cell) const = 0;
  // Iterate over port bits (expanded buses).
  virtual CellPortBitIterator *portBitIterator(const Cell *cell) const = 0;
  // Port bit count (expanded buses).
  virtual int portBitCount(const Cell *cell) const = 0;

  ////////////////////////////////////////////////////////////////
  // Port functions
  virtual const char *name(const Port *port) const = 0;
  virtual ObjectId id(const Port *port) const = 0;
  virtual Cell *cell(const Port *port) const = 0;
  virtual LibertyPort *libertyPort(const Port *port) const = 0;
  virtual PortDirection *direction(const Port *port) const = 0;
  // Bus port functions.
  virtual bool isBus(const Port *port) const = 0;
  virtual bool isBundle(const Port *port) const = 0;
  // Size is the bus/bundle member count (1 for non-bus/bundle ports).
  virtual int size(const Port *port) const = 0;
  // Bus range bus[from:to].
  virtual const char *busName(const Port *port) const = 0;
  // Bus member, bus[subscript].
  virtual Port *findBusBit(const Port *port,
			   int index) const = 0;
  virtual int fromIndex(const Port *port) const = 0;
  virtual int toIndex(const Port *port) const = 0;
  // Predicate to determine if index is within bus range.
  //     (toIndex > fromIndex) && fromIndex <= index <= toIndex
  //  || (fromIndex > toIndex) && fromIndex >= index >= toIndex
  bool busIndexInRange(const Port *port,
		       int index);
  // Find Bundle/bus member by index.
  virtual Port *findMember(const Port *port,
			   int index) const = 0;
  // Iterate over the bits of a bus port or members of a bundle.
  // from_index -> to_index
  virtual PortMemberIterator *memberIterator(const Port *port) const = 0;
  // A port has members if it is a bundle or bus.
  virtual bool hasMembers(const Port *port) const;

  ////////////////////////////////////////////////////////////////
  // Instance functions
  // Name local to containing cell/instance.
  virtual const char *name(const Instance *instance) const = 0;
  virtual ObjectId id(const Instance *instance) const = 0;
  // Top level instance of the design (defined after link).
  virtual Instance *topInstance() const = 0;
  virtual bool isTopInstance(const Instance *inst) const;
  virtual Instance *findInstance(const char *path_name) const;
  // Find instance relative to hierarchical instance.
  virtual Instance *findInstanceRelative(const Instance *inst,
                                         const char *path_name) const;
  // Default implementation uses linear search.
  virtual InstanceSeq findInstancesMatching(const Instance *context,
                                            const PatternMatch *pattern) const;
  virtual InstanceSeq findInstancesHierMatching(const Instance *instance,
                                                const PatternMatch *pattern) const;
  virtual std::string getAttribute(const Instance *inst,
                                   const std::string &key) const = 0;
  virtual const char *getDesignType(const Instance *inst) const = 0;
  virtual const AttributeMap &attributeMap(const Instance *inst) const = 0;
  // Hierarchical path name.
  virtual const char *pathName(const Instance *instance) const;
  bool pathNameLess(const Instance *inst1,
		    const Instance *inst2) const;
  int pathNameCmp(const Instance *inst1,
		  const Instance *inst2) const;
  // Path from instance up to top level (last in the sequence).
  void path(const Instance *inst,
	    // Return value.
	    InstanceSeq &path) const;
  virtual Cell *cell(const Instance *instance) const = 0;
  virtual const char *cellName(const Instance *instance) const;
  virtual LibertyLibrary *libertyLibrary(const Instance *instance) const;
  virtual LibertyCell *libertyCell(const Instance *instance) const;
  virtual Instance *parent(const Instance *instance) const = 0;
  virtual bool isLeaf(const Instance *instance) const = 0;
  virtual bool isHierarchical(const Instance *instance) const;
  virtual Instance *findChild(const Instance *parent,
			      const char *name) const = 0;
  virtual void findChildrenMatching(const Instance *parent,
				    const PatternMatch *pattern,
                                    // Return value.
                                    InstanceSeq &matches) const;
  // Is inst inside of hier_inst?
  bool isInside(const Instance *inst,
		const Instance *hier_inst) const;

  // Iterate over all of the leaf instances in the hierarchy
  // This iterator is not virtual because it can be written in terms of
  // the other primitives.
  LeafInstanceIterator *leafInstanceIterator() const;
  LeafInstanceIterator *leafInstanceIterator(const Instance *hier_inst) const;
  InstanceSeq leafInstances();
  // Iterate over the children of an instance.
  virtual InstanceChildIterator *
  childIterator(const Instance *instance) const = 0;
  // Iterate over the pins on an instance.
  virtual InstancePinIterator *
  pinIterator(const Instance *instance) const = 0;
  // Iterate over the nets in an instance.
  // This should include nets that are connected to the
  // instance parent thru pins.
  virtual InstanceNetIterator *
  netIterator(const Instance *instance) const = 0;
  int instanceCount();
  int instanceCount(Instance *inst);
  int leafInstanceCount();

  ////////////////////////////////////////////////////////////////
  // Pin functions
  // Name is instance_name/port_name (the same as path name).
  virtual const char *name(const Pin *pin) const;
  virtual ObjectId id(const Pin *pin) const = 0;
  virtual Pin *findPin(const char *path_name) const;
  virtual Pin *findPin(const Instance *instance,
		       const char *port_name) const = 0;
  virtual Pin *findPin(const Instance *instance,
		       const Port *port) const;
  virtual Pin *findPin(const Instance *instance,
		       const LibertyPort *port) const;
  // Find pin relative to hierarchical instance.
  Pin *findPinRelative(const Instance *inst,
		       const char *path_name) const;
  // Default implementation uses linear search.
  virtual PinSeq findPinsMatching(const Instance *instance,
                                  const PatternMatch *pattern) const;
  // Traverse the hierarchy from instance down and find pins matching
  // pattern of the form instance_name/port_name.
  virtual PinSeq findPinsHierMatching(const Instance *instance,
                                      const PatternMatch *pattern) const;
  virtual const char *portName(const Pin *pin) const;
  // Path name is instance_name/port_name.
  virtual const char *pathName(const Pin *pin) const;
  bool pathNameLess(const Pin *pin1,
		    const Pin *pin2) const;
  int pathNameCmp(const Pin *pin1,
		  const Pin *pin2) const;
  virtual Port *port(const Pin *pin) const = 0;
  virtual LibertyPort *libertyPort(const Pin *pin) const;
  virtual Instance *instance(const Pin *pin) const = 0;
  virtual Net *net(const Pin *pin) const = 0;
  virtual Term *term(const Pin *pin) const = 0;
  virtual PortDirection *direction(const Pin *pin) const = 0;
  virtual bool isLeaf(const Pin *pin) const;
  bool isHierarchical(const Pin *pin) const;
  bool isTopLevelPort(const Pin *pin) const;
  // Is pin inside the instance hier_pin is attached to?
  bool isInside(const Pin *pin,
		const Pin *hier_pin) const;
  // Is pin inside of hier_inst?
  bool isInside(const Pin *pin,
		const Instance *hier_inst) const;
  bool isDriver(const Pin *pin) const;
  bool isLoad(const Pin *pin) const;
  bool isClock(const Pin *pin) const;
  bool isRiseEdgeTriggered(const Pin *pin) const;
  bool isFallEdgeTriggered(const Pin *pin) const;
  // Has register/latch rise/fall edges from pin.
  bool isRegClkPin(const Pin *pin) const;
  // Pin clocks a timing check.
  bool isCheckClk(const Pin *pin) const;
  bool isLatchData(const Pin *pin) const;

  // Iterate over all of the pins connected to a pin and the parent
  // and child nets it is hierarchically connected to (port, leaf and
  // hierarchical pins).
  virtual PinConnectedPinIterator *connectedPinIterator(const Pin *pin) const;
  virtual void visitConnectedPins(const Pin *pin,
				  PinVisitor &visitor) const;

  // Find driver pins for the net connected to pin.
  // Return value is owned by the network.
  virtual PinSet *drivers(const Pin *pin);
  virtual bool pinLess(const Pin *pin1,
		       const Pin *pin2) const;
  // Return the id of the pin graph vertex.
  virtual VertexId vertexId(const Pin *pin) const = 0;
  virtual void setVertexId(Pin *pin,
			   VertexId id) = 0;
  // Return the physical X/Y coordinates of the pin.
  virtual void location(const Pin *pin,
			// Return values.
			double &x,
			double &y,
			bool &exists) const;

  int pinCount();
  int pinCount(Instance *inst);
  int leafPinCount();

  ////////////////////////////////////////////////////////////////
  // Terminal functions
  // Name is instance_name/port_name (the same as path name).
  virtual const char *name(const Term *term) const;
  virtual ObjectId id(const Term *term) const = 0;
  virtual const char *portName(const Term *term) const;
  // Path name is instance_name/port_name (pin name).
  virtual const char *pathName(const Term *term) const;
  virtual Net *net(const Term *term) const = 0;
  virtual Pin *pin(const Term *term) const = 0;

  ////////////////////////////////////////////////////////////////
  // Net functions
  virtual const char *name(const Net *net) const = 0; // no hierarchy prefix
  virtual ObjectId id(const Net *net) const = 0;
  virtual Net *findNet(const char *path_name) const;
  // Find net relative to hierarchical instance.
  virtual Net *findNetRelative(const Instance *inst,
                               const char *path_name) const;
  // Default implementation uses linear search.
  virtual NetSeq findNetsMatching(const Instance *context,
                                  const PatternMatch *pattern) const;
  virtual Net *findNet(const Instance *instance,
		       const char *net_name) const = 0;
  // Traverse the hierarchy from instance down and find nets matching
  // pattern of the form instance_name/net_name.
  virtual NetSeq findNetsHierMatching(const Instance *instance,
                                      const PatternMatch *pattern) const;
  // Primitive used by findNetsMatching.
  virtual void findInstNetsMatching(const Instance *instance,
                                    const PatternMatch *pattern,
                                    NetSeq &matches) const = 0;
  virtual const char *pathName(const Net *net) const;
  bool pathNameLess(const Net *net1,
		    const Net *net2) const;
  int pathNameCmp(const Net *net1,
		  const Net *net2) const;
  virtual Instance *instance(const Net *net) const = 0;
  // Is net inside of hier_inst?
  virtual bool isInside(const Net *net,
			const Instance *hier_inst) const;
  // Is pin connected to net anywhere in the hierarchy?
  virtual bool isConnected(const Net *net,
			   const Pin *pin) const;
  // Is net1 connected to net2 anywhere in the hierarchy?
  virtual bool isConnected(const Net *net1,
			   const Net *net2) const;
  virtual Net *highestNetAbove(Net *net) const;
  virtual const Net *highestConnectedNet(Net *net) const;
  virtual void connectedNets(Net *net,
			     NetSet *nets) const;
  virtual void connectedNets(const Pin *pin,
			     NetSet *nets) const;
  virtual bool isPower(const Net *net) const = 0;
  virtual bool isGround(const Net *net) const = 0;

  // Iterate over the pins connected to a net (port, leaf and hierarchical).
  virtual NetPinIterator *pinIterator(const Net *net) const = 0;
  // Iterate over the terminals connected to a net.
  virtual NetTermIterator *termIterator(const Net *net) const = 0;
  // Iterate over all of the pins connected to a net and the parent
  // and child nets it is hierarchically connected to (port, leaf and
  // hierarchical pins).
  virtual NetConnectedPinIterator *connectedPinIterator(const Net *net) const;
  virtual void visitConnectedPins(const Net *net,
				  PinVisitor &visitor) const;
  // Find driver pins for net.
  // Return value is owned by the network.
  virtual PinSet *drivers(const Net *net);
  int netCount();
  int netCount(Instance *inst);

  // Iterate over pins connected to nets tied off to logic zero and one.
  virtual ConstantPinIterator *constantPinIterator() = 0;

  ////////////////////////////////////////////////////////////////

  // Parse path into first/tail (first hierarchy divider separated token).
  // first and tail are both null if there are no dividers in path.
  // Caller must delete first and tail.
  void pathNameFirst(const char *path_name,
		     char *&first,
		     char *&tail) const;
  // Parse path into head/last (last hierarchy divider separated token).
  // head and last are both null if there are no dividers in path.
  // Caller must delete head and last.
  void pathNameLast(const char *path_name,
		    char *&head,
		    char *&last) const;

  // Divider between instance names in a hierarchical path name.
  virtual char pathDivider() const { return divider_; }
  virtual void setPathDivider(char divider);
  // Escape prefix for path dividers in path names.
  virtual char pathEscape() const { return escape_; }
  virtual void setPathEscape(char escape);

protected:
  Pin *findPinLinear(const Instance *instance,
		     const char *port_name) const;
  void findInstancesMatching1(const Instance *context,
			      size_t context_name_length,
			      const PatternMatch *pattern,
			      InstanceSeq &insts) const;
  void findInstancesHierMatching1(const Instance *instance,
                                  const PatternMatch *pattern,
                                  InstanceSeq &matches) const;
  void findNetsMatching(const Instance *context,
                        const PatternMatch *pattern,
                        NetSeq &matches) const;
  void findNetsHierMatching(const Instance *instance,
                            const PatternMatch *pattern,
                            NetSeq &matches) const;
  void findPinsHierMatching(const Instance *instance,
                            const PatternMatch *pattern,
                            // Return value.
                            PinSeq &matches) const;
  bool isConnected(const Net *net,
		   const Pin *pin,
		   NetSet &nets) const;
  bool isConnected(const Net *net1,
		   const Net *net2,
		   NetSet &nets) const;
  int hierarchyLevel(const Net *net) const;
  virtual void visitConnectedPins(const Net *net,
				  PinVisitor &visitor,
				  NetSet &visited_nets) const;
  // Default implementation uses linear search.
  virtual void findInstPinsMatching(const Instance *instance,
                                    const PatternMatch *pattern,
                                    // Return value.
                                    PinSeq &matches) const;
  void findInstPinsHierMatching(const Instance *parent,
                                const PatternMatch *pattern,
                                // Return value.
                                PinSeq &matches) const;
  // findNet using linear search.
  Net *findNetLinear(const Instance *instance,
		     const char *net_name) const;
  // findNetsMatching using linear search.
  NetSeq findNetsMatchingLinear(const Instance *instance,
                                const PatternMatch *pattern) const;
  // Connect/disconnect net/pins should clear the net->drvrs map.
  // Incrementally maintaining the map is expensive because 
  // nets may be connected across hierarchy levels.
  void clearNetDrvrPinMap();

  LibertyLibrary *default_liberty_;
  char divider_;
  char escape_;
  NetDrvrPinsMap net_drvr_pin_map_;
};

// Network API to support network edits.
class NetworkEdit : public Network
{
public:
  NetworkEdit();
  virtual bool isEditable() const { return true; }
  virtual Instance *makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent) = 0;
  virtual void makePins(Instance *inst) = 0;
  virtual void replaceCell(Instance *inst,
			   Cell *cell) = 0;
  // Deleting instance also deletes instance pins.
  virtual void deleteInstance(Instance *inst) = 0;
  // Connect the port on an instance to a net.
  virtual Pin *connect(Instance *inst,
		       Port *port,
		       Net *net) = 0;
  virtual Pin *connect(Instance *inst,
		       LibertyPort *port,
		       Net *net) = 0;
  // makePin/connectPin replaced by connect.
  // deprecated 2018-09-28
  virtual void connectPin(Pin *pin,
			  Net *net) __attribute__ ((deprecated));
  // Disconnect pin from net.
  virtual void disconnectPin(Pin *pin) = 0;
  virtual void deletePin(Pin *pin) = 0;
  virtual Net *makeNet(const char *name,
		       Instance *parent) = 0;
  // Deleting net disconnects (but does not delete) net pins.
  virtual void deleteNet(Net *net) = 0;
  virtual void mergeInto(Net *net,
			 Net *into_net) = 0;
  virtual Net *mergedInto(Net *net) = 0;
};

// Network API to support the Parallax readers.
class NetworkReader : public NetworkEdit
{
public:
  NetworkReader() {}
  // Called before reading a netlist to delete any previously linked network.
  virtual void readNetlistBefore() = 0;
  virtual void setLinkFunc(LinkNetworkFunc link) = 0;
  virtual Library *makeLibrary(const char *name,
			       const char *filename) = 0;
  virtual void deleteLibrary(Library *library) = 0;
  // Search the libraries in read order for a cell by name.
  virtual Cell *findAnyCell(const char *name) = 0;
  virtual Cell *makeCell(Library *library,
			 const char *name,
			 bool is_leaf,
			 const char *filename) = 0;
  virtual void deleteCell(Cell *cell) = 0;
  virtual void setName(Cell *cell,
		       const char *name) = 0;
  virtual void setIsLeaf(Cell *cell,
			 bool is_leaf) = 0;
  virtual void setAttribute(Cell *cell,
                            const std::string &key,
                            const std::string &value) = 0;
  virtual void setAttribute(Instance *instance,
                            const std::string &key,
                            const std::string &value) = 0;
  virtual Port *makePort(Cell *cell,
			 const char *name) = 0;
  virtual Port *makeBusPort(Cell *cell,
			    const char *name,
			    int from_index,
			    int to_index) = 0;
  virtual void groupBusPorts(Cell *cell,
                             std::function<bool(const char*)> port_msb_first) = 0;
  virtual Port *makeBundlePort(Cell *cell,
			       const char *name,
			       PortSeq *members) = 0;
  virtual Instance *makeInstance(Cell *cell,
				 const char *name,
				 Instance *parent) = 0;
  virtual Pin *makePin(Instance *inst,
		       Port *port,
		       Net *net) = 0;
  virtual Term *makeTerm(Pin *pin,
			 Net *net) = 0;
  virtual void setDirection(Port *port,
			    PortDirection *dir) = 0;
  // Instance is the network view for cell.
  virtual void setCellNetworkView(Cell *cell,
				  Instance *inst) = 0;
  virtual Instance *cellNetworkView(Cell *cell) = 0;
  virtual void deleteCellNetworkViews() = 0;
  virtual void addConstantNet(Net *net,
			      LogicValue const_value) = 0;

  using NetworkEdit::makeInstance;
};

Instance *
linkReaderNetwork(Cell *top_cell,
		  bool make_black_boxes,
		  Report *report,
		  NetworkReader *network);

// Abstract class for Network::constantPinIterator().
class ConstantPinIterator
{
public:
  ConstantPinIterator() {}
  virtual ~ConstantPinIterator() {}
  virtual bool hasNext() = 0;
  virtual void next(const Pin *&pin,
		    LogicValue &value) = 0;
};

// Implementation class for Network::constantPinIterator().
class NetworkConstantPinIterator : public ConstantPinIterator
{
public:
  NetworkConstantPinIterator(const Network *network,
			      NetSet &zero_nets,
			      NetSet &one_nets);
  ~NetworkConstantPinIterator();
  virtual bool hasNext();
  virtual void next(const Pin *&pin, LogicValue &value);

private:
  void findConstantPins(NetSet &nets,
			PinSet &pins);

  const Network *network_;
  PinSet constant_pins_[2];
  LogicValue value_;
  PinSet::Iterator *pin_iter_;
};

// Abstract base class for visitDrvrLoadsThruHierPin visitor.
class HierPinThruVisitor
{
public:
  HierPinThruVisitor() {}
  virtual ~HierPinThruVisitor() {}
  virtual void visit(const Pin *drvr,
		     const Pin *load) = 0;
};

class PinVisitor
{
public:
  virtual ~PinVisitor() {}
  virtual void operator()(const Pin *pin) = 0;
};

class FindNetDrvrLoads : public PinVisitor
{
public:
  FindNetDrvrLoads(const Pin *drvr_pin,
		   PinSet &visited_drvrs,
		   PinSeq &loads,
		   PinSeq &drvrs,
		   const Network *network);
  virtual void operator()(const Pin *pin);

protected:
  const Pin *drvr_pin_;
  PinSet &visited_drvrs_;
  PinSeq &loads_;
  PinSeq &drvrs_;
  const Network *network_;
};

// Visit driver/loads pins through a hierarcial pin.
void
visitDrvrLoadsThruHierPin(const Pin *hpin,
			  const Network *network,
			  HierPinThruVisitor *visitor);
void
visitDrvrLoadsThruNet(const Net *net,
		      const Network *network,
		      HierPinThruVisitor *visitor);

char
logicValueString(LogicValue value);

} // namespace
