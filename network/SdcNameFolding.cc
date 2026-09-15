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

// SILIMATE: name-folding fallback for SDC object queries.
//
// Constraints written against a synthesis netlist name objects the way
// the synthesis tool flattened them (gen_0_child, wrapper_child, reg),
// while a netlist elaborated from RTL keeps generate scopes, wrappers and
// bit indices (gen[0].child, wrapper/child, reg[0]). See SdcNetwork.hh
// for the rules.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FuncExpr.hh"
#include "Liberty.hh"
#include "PatternMatch.hh"
#include "Report.hh"
#include "SdcNetwork.hh"
#include "Sequential.hh"
#include "Variables.hh"

namespace sta {

// Folded path name hashes of every instance (and net) in the network.
class SdcNameFoldIndex
{
public:
  using InstanceEntry = std::pair<size_t, const Instance*>;
  using NetEntry = std::pair<size_t, const Net*>;

  // Network::nameEditCount when the index was built.
  uint64_t edit_count_{0};
  bool edit_count_valid_{false};
  bool instances_built_{false};
  bool nets_built_{false};
  // Folded path name hash -> instance, sorted by hash.
  std::vector<InstanceEntry> instances_;
  // Leaf instances whose name ends in a bit index, keyed by the folded
  // path name without the index (reg[0] -> reg).
  std::vector<InstanceEntry> bit_index_instances_;
  // Folded path name hash -> net, sorted by hash.
  std::vector<NetEntry> nets_;
};

namespace {

constexpr size_t fold_candidates_reported = 5;

size_t
hashFolded(std::string_view folded)
{
  return std::hash<std::string_view>()(folded);
}

template <class OBJ>
void
sortEntries(std::vector<std::pair<size_t, OBJ>> &entries)
{
  std::sort(entries.begin(), entries.end(),
            [](const std::pair<size_t, OBJ> &e1,
               const std::pair<size_t, OBJ> &e2) {
              return e1.first < e2.first;
            });
}

template <class OBJ>
std::vector<OBJ>
entriesWithHash(const std::vector<std::pair<size_t, OBJ>> &entries,
                size_t hash)
{
  std::vector<OBJ> objs;
  auto lower = std::lower_bound(entries.begin(), entries.end(), hash,
                                [](const std::pair<size_t, OBJ> &entry,
                                   size_t h) {
                                  return entry.first < h;
                                });
  for (auto itr = lower; itr != entries.end() && itr->first == hash; itr++)
    objs.push_back(itr->second);
  return objs;
}

// Literal characters of a folded glob before its first wildcard.
std::string_view
literalPrefix(std::string_view glob)
{
  size_t wild = glob.find_first_of("*?");
  return wild == std::string_view::npos ? glob : glob.substr(0, wild);
}

// Can a descendant of an object whose folded path is folded match a glob
// that starts with prefix? Descendant paths are folded + '_' + name.
bool
descendantsMayMatch(std::string_view folded,
                    std::string_view prefix)
{
  if (folded.size() < prefix.size())
    return prefix.substr(0, folded.size()) == folded
      && (folded.empty() || prefix[folded.size()] == '_');
  return folded.substr(0, prefix.size()) == prefix;
}

// Start of a trailing bit index in an instance name (reg[0] or the escaped
// reg\[0\]), or npos.
size_t
bitIndexStart(std::string_view name,
              char escape)
{
  size_t end = name.size();
  if (end == 0 || name[end - 1] != ']')
    return std::string_view::npos;
  end--;
  if (end > 0 && name[end - 1] == escape)
    end--;
  size_t digits_end = end;
  while (end > 0 && std::isdigit(static_cast<unsigned char>(name[end - 1])))
    end--;
  if (end == digits_end || end == 0 || name[end - 1] != '[')
    return std::string_view::npos;
  end--;
  if (end > 0 && name[end - 1] == escape)
    end--;
  return end == 0 ? std::string_view::npos : end;
}

bool
nameIn(std::string_view name,
       const std::initializer_list<std::string_view> &names)
{
  for (std::string_view alias : names) {
    if (alias.size() == name.size()
        && std::equal(alias.begin(), alias.end(), name.begin(),
                      [](char c1, char c2) {
                        return std::toupper(static_cast<unsigned char>(c1))
                          == std::toupper(static_cast<unsigned char>(c2));
                      }))
      return true;
  }
  return false;
}

// Clock pin spellings used by common standard cell libraries.
bool
isClockPinAlias(std::string_view name)
{
  return nameIn(name, {"CP", "CPN", "CK", "CKN", "CLK", "CLKN", "C",
                       "CLOCK", "CLKIN"});
}

// Latch enable spellings. Only used on cells whose sequentials are all
// latches, so a register data enable (E/EN) is never mistaken for a clock.
bool
isLatchEnableAlias(std::string_view name)
{
  return nameIn(name, {"E", "EN", "G", "GN", "GATE", "GATE_N"});
}

std::string
quotedList(const std::vector<std::string> &names)
{
  std::string list;
  size_t count = 0;
  for (const std::string &name : names) {
    if (count == fold_candidates_reported) {
      list += ", ...";
      break;
    }
    if (count > 0)
      list += ", ";
    list += '\'';
    list += name;
    list += '\'';
    count++;
  }
  return list;
}

} // namespace

////////////////////////////////////////////////////////////////

SdcNetwork::~SdcNetwork()
{
  delete fold_index_;
}

std::string
SdcNetwork::foldName(std::string_view name) const
{
  std::string folded;
  foldAppend(folded, name);
  return folded;
}

// Append name to a folded path. Each run of fold characters becomes one
// '_', a run at the end of name is dropped, and a non-empty folded path is
// separated from name by '_' (the hierarchy divider).
void
SdcNetwork::foldAppend(std::string &folded,
                       std::string_view name) const
{
  char divider = pathDivider();
  char escape = pathEscape();
  bool pending = !folded.empty();
  for (char ch : name) {
    if (ch == '/' || ch == '.' || ch == '[' || ch == ']' || ch == '_'
        || ch == divider || ch == escape)
      pending = true;
    else {
      if (pending)
        folded += '_';
      pending = false;
      folded += ch;
    }
  }
}

std::string
SdcNetwork::foldedPathName(const Instance *instance) const
{
  if (instance == nullptr || instance == network_->topInstance())
    return "";
  return foldName(network_->pathName(instance));
}

SdcNameFoldIndex &
SdcNetwork::foldIndex(bool nets) const
{
  uint64_t edit_count;
  bool counted = network_->nameEditCount(edit_count);
  if (fold_index_ == nullptr
      // Networks that do not count edits cannot keep an index.
      || !counted
      || !fold_index_->edit_count_valid_
      || fold_index_->edit_count_ != edit_count) {
    delete fold_index_;
    fold_index_ = new SdcNameFoldIndex;
    fold_index_->edit_count_ = edit_count;
    fold_index_->edit_count_valid_ = counted;
  }
  SdcNameFoldIndex &index = *fold_index_;
  Instance *top = network_->topInstance();
  if (!index.instances_built_ && top) {
    std::string folded;
    foldIndexInstances(top, folded, index);
    sortEntries(index.instances_);
    sortEntries(index.bit_index_instances_);
    index.instances_built_ = true;
  }
  if (nets && !index.nets_built_ && top) {
    std::string folded;
    foldIndexNets(top, folded, index);
    sortEntries(index.nets_);
    index.nets_built_ = true;
  }
  return index;
}

void
SdcNetwork::foldIndexInstances(const Instance *parent,
                               std::string &folded,
                               SdcNameFoldIndex &index) const
{
  char escape = pathEscape();
  InstanceChildIterator *child_iter = network_->childIterator(parent);
  while (child_iter->hasNext()) {
    const Instance *child = child_iter->next();
    std::string name = network_->name(child);
    size_t parent_length = folded.size();
    foldAppend(folded, name);
    index.instances_.emplace_back(hashFolded(folded), child);
    if (network_->isLeaf(child)) {
      size_t bit_index = bitIndexStart(name, escape);
      if (bit_index != std::string::npos) {
        std::string unindexed = folded.substr(0, parent_length);
        foldAppend(unindexed, std::string_view(name).substr(0, bit_index));
        index.bit_index_instances_.emplace_back(hashFolded(unindexed), child);
      }
    }
    else
      foldIndexInstances(child, folded, index);
    folded.resize(parent_length);
  }
  delete child_iter;
}

void
SdcNetwork::foldIndexNets(const Instance *instance,
                          std::string &folded,
                          SdcNameFoldIndex &index) const
{
  size_t inst_length = folded.size();
  InstanceNetIterator *net_iter = network_->netIterator(instance);
  while (net_iter->hasNext()) {
    const Net *net = net_iter->next();
    foldAppend(folded, network_->name(net));
    index.nets_.emplace_back(hashFolded(folded), net);
    folded.resize(inst_length);
  }
  delete net_iter;

  InstanceChildIterator *child_iter = network_->childIterator(instance);
  while (child_iter->hasNext()) {
    const Instance *child = child_iter->next();
    if (!network_->isLeaf(child)) {
      foldAppend(folded, network_->name(child));
      foldIndexNets(child, folded, index);
      folded.resize(inst_length);
    }
  }
  delete child_iter;
}

void
SdcNetwork::foldGlobInstances(const Instance *parent,
                              std::string &folded,
                              const PatternMatch *glob,
                              std::string_view literal_prefix,
                              InstanceSeq &matches) const
{
  InstanceChildIterator *child_iter = network_->childIterator(parent);
  while (child_iter->hasNext()) {
    const Instance *child = child_iter->next();
    size_t parent_length = folded.size();
    foldAppend(folded, network_->name(child));
    if (glob->match(folded))
      matches.push_back(child);
    if (!network_->isLeaf(child)
        && descendantsMayMatch(folded, literal_prefix))
      foldGlobInstances(child, folded, glob, literal_prefix, matches);
    folded.resize(parent_length);
  }
  delete child_iter;
}

void
SdcNetwork::foldGlobNets(const Instance *instance,
                         std::string &folded,
                         const PatternMatch *glob,
                         std::string_view literal_prefix,
                         NetSeq &matches) const
{
  size_t inst_length = folded.size();
  InstanceNetIterator *net_iter = network_->netIterator(instance);
  while (net_iter->hasNext()) {
    const Net *net = net_iter->next();
    foldAppend(folded, network_->name(net));
    if (glob->match(folded))
      matches.push_back(net);
    folded.resize(inst_length);
  }
  delete net_iter;

  InstanceChildIterator *child_iter = network_->childIterator(instance);
  while (child_iter->hasNext()) {
    const Instance *child = child_iter->next();
    if (!network_->isLeaf(child)) {
      foldAppend(folded, network_->name(child));
      if (descendantsMayMatch(folded, literal_prefix))
        foldGlobNets(child, folded, glob, literal_prefix, matches);
      folded.resize(inst_length);
    }
  }
  delete child_iter;
}

////////////////////////////////////////////////////////////////

// Instances whose folded path name matches pattern, relative to context.
InstanceSeq
SdcNetwork::foldInstances(const Instance *context,
                          const PatternMatch *pattern,
                          bool glob) const
{
  InstanceSeq matches;
  const Variables *vars = variables();
  bool nocase = pattern->nocase()
    || (vars && vars->caseInsensitiveMatching());
  if (glob || nocase) {
    std::string folded_pattern = foldName(pattern->pattern());
    PatternMatch folded_glob(folded_pattern, pattern);
    // Case-insensitive matches cannot be pruned by a literal prefix.
    std::string_view prefix = nocase ? "" : literalPrefix(folded_pattern);
    std::string folded;
    foldGlobInstances(context, folded, &folded_glob, prefix, matches);
  }
  else {
    std::string folded = foldedPathName(context);
    foldAppend(folded, pattern->pattern());
    const SdcNameFoldIndex &index = foldIndex(false);
    for (const Instance *inst : entriesWithHash(index.instances_,
                                                hashFolded(folded))) {
      if (foldName(network_->pathName(inst)) == folded)
        matches.push_back(inst);
    }
  }
  return matches;
}

// Register instances named inst_path plus one trailing bit index.
InstanceSeq
SdcNetwork::foldBitIndexRegisters(const Instance *context,
                                  std::string_view inst_path) const
{
  InstanceSeq matches;
  std::string folded = foldedPathName(context);
  foldAppend(folded, inst_path);
  const SdcNameFoldIndex &index = foldIndex(false);
  char escape = pathEscape();
  for (const Instance *inst : entriesWithHash(index.bit_index_instances_,
                                              hashFolded(folded))) {
    const LibertyCell *cell = network_->libertyCell(inst);
    if (cell == nullptr || cell->sequentials().empty())
      continue;
    std::string path = network_->pathName(inst);
    size_t bit_index = bitIndexStart(path, escape);
    if (bit_index != std::string::npos
        && foldName(std::string_view(path).substr(0, bit_index)) == folded)
      matches.push_back(inst);
  }
  return matches;
}

void
SdcNetwork::foldInstancePins(const InstanceSeq &insts,
                             const PatternMatch *port_pattern,
                             bool glob,
                             PinSeq &pins) const
{
  for (const Instance *inst : insts) {
    if (glob)
      visitPinTail(inst, port_pattern, pins);
    else {
      const Pin *pin = findPin(inst, port_pattern->pattern());
      if (pin)
        pins.push_back(pin);
    }
  }
}

void
SdcNetwork::foldClockPinAliases(const InstanceSeq &insts,
                                std::string_view port_name,
                                PinSeq &pins) const
{
  if (!isClockPinAlias(port_name) && !isLatchEnableAlias(port_name))
    return;
  for (const Instance *inst : insts) {
    const Pin *pin = clockPinAlias(inst, port_name);
    if (pin)
      pins.push_back(pin);
  }
}

// The only clock (or latch enable) pin of a register/latch instance when
// port_name is one of its common spellings.
const Pin *
SdcNetwork::clockPinAlias(const Instance *inst,
                          std::string_view port_name) const
{
  const LibertyCell *cell = network_->libertyCell(inst);
  if (cell == nullptr)
    return nullptr;
  const LibertyPort *clk_port = nullptr;
  bool has_register = false;
  for (const Sequential &seq : cell->sequentials()) {
    const FuncExpr *clk_expr = seq.clock();
    if (clk_expr == nullptr)
      continue;
    for (const LibertyPort *port : clk_expr->ports()) {
      if (clk_port && clk_port != port)
        // More than one clock pin; no single pin to alias to.
        return nullptr;
      clk_port = port;
    }
    if (seq.isRegister())
      has_register = true;
  }
  if (clk_port == nullptr)
    return nullptr;
  if (isClockPinAlias(port_name)
      || (!has_register && isLatchEnableAlias(port_name)))
    return network_->findPin(inst, clk_port);
  return nullptr;
}

////////////////////////////////////////////////////////////////

InstanceSeq
SdcNetwork::findInstancesFolded(const Instance *context,
                                const PatternMatch *pattern) const
{
  InstanceSeq matches;
  if (pattern->isRegexp())
    return matches;
  const std::string &query = pattern->pattern();
  bool glob = patternWildcards(query);
  matches = foldInstances(context, pattern, glob);
  if (!glob && matches.size() > 1) {
    std::vector<std::string> candidates;
    for (const Instance *inst : matches)
      candidates.push_back(pathName(inst));
    reportFoldAmbiguous(query, candidates);
    matches.clear();
  }
  if (!matches.empty()) {
    std::vector<std::string> targets;
    for (const Instance *inst : matches)
      targets.push_back(pathName(inst));
    reportFoldRescue(query, targets, "");
  }
  return matches;
}

PinSeq
SdcNetwork::findPinsFolded(const Instance *context,
                           const PatternMatch *pattern) const
{
  PinSeq pins;
  if (pattern->isRegexp())
    return pins;
  const std::string &query = pattern->pattern();
  std::string inst_path, port_name;
  pathNameLast(query, inst_path, port_name);
  if (inst_path.empty() || port_name.empty())
    return pins;
  bool inst_glob = patternWildcards(inst_path);
  bool port_glob = patternWildcards(port_name);
  PatternMatch inst_pattern(inst_path, pattern);
  PatternMatch port_pattern(port_name, pattern);
  std::string rule;

  InstanceSeq insts = findInstancesMatching(context, &inst_pattern);
  if (!insts.empty()) {
    // The instances exist under their exact names but lack the port.
    if (!port_glob)
      foldClockPinAliases(insts, port_name, pins);
    rule = " (clock pin alias)";
  }
  else {
    insts = foldInstances(context, &inst_pattern, inst_glob);
    if (insts.empty() && !inst_glob) {
      insts = foldBitIndexRegisters(context, inst_path);
      rule = " (register bit index)";
    }
    if (!inst_glob && insts.size() > 1) {
      std::vector<std::string> candidates;
      for (const Instance *inst : insts)
        candidates.push_back(pathName(inst) + divider_ + port_name);
      reportFoldAmbiguous(query, candidates);
      return pins;
    }
    foldInstancePins(insts, &port_pattern, port_glob, pins);
    if (pins.empty() && !port_glob) {
      foldClockPinAliases(insts, port_name, pins);
      rule = rule.empty()
        ? " (clock pin alias)"
        : " (register bit index, clock pin alias)";
    }
  }
  if (!pins.empty()) {
    std::vector<std::string> targets;
    for (const Pin *pin : pins)
      targets.push_back(pathName(pin));
    reportFoldRescue(query, targets, rule);
  }
  return pins;
}

Pin *
SdcNetwork::findPinFolded(std::string_view path_name) const
{
  PatternMatch pattern(path_name);
  PinSeq pins = findPinsFolded(network_->topInstance(), &pattern);
  // Glob path names are not pin names; only a single pin is a match.
  if (pins.size() == 1)
    return const_cast<Pin*>(pins[0]);
  return nullptr;
}

NetSeq
SdcNetwork::findNetsFolded(const Instance *context,
                           const PatternMatch *pattern) const
{
  NetSeq matches;
  if (pattern->isRegexp())
    return matches;
  const std::string &query = pattern->pattern();
  bool glob = patternWildcards(query);
  const Variables *vars = variables();
  bool nocase = pattern->nocase()
    || (vars && vars->caseInsensitiveMatching());
  if (glob || nocase) {
    std::string folded_pattern = foldName(query);
    PatternMatch folded_glob(folded_pattern, pattern);
    std::string_view prefix = nocase ? "" : literalPrefix(folded_pattern);
    std::string folded;
    foldGlobNets(context, folded, &folded_glob, prefix, matches);
  }
  else {
    std::string folded = foldedPathName(context);
    foldAppend(folded, query);
    const SdcNameFoldIndex &index = foldIndex(true);
    for (const Net *net : entriesWithHash(index.nets_, hashFolded(folded))) {
      if (foldName(network_->pathName(net)) == folded)
        matches.push_back(net);
    }
  }
  if (!glob && matches.size() > 1) {
    std::vector<std::string> candidates;
    for (const Net *net : matches)
      candidates.push_back(pathName(net));
    reportFoldAmbiguous(query, candidates);
    matches.clear();
  }
  if (!matches.empty()) {
    std::vector<std::string> targets;
    for (const Net *net : matches)
      targets.push_back(pathName(net));
    reportFoldRescue(query, targets, "");
  }
  return matches;
}

PortSeq
SdcNetwork::findPortsFolded(const Cell *cell,
                            const PatternMatch *pattern) const
{
  PortSeq matches;
  if (pattern->isRegexp())
    return matches;
  const std::string &query = pattern->pattern();
  bool glob = patternWildcards(query);
  std::string rule;
  if (glob) {
    // Bus bit names, as tools that expand buses into bits match globs.
    CellPortIterator *port_iter = network_->portIterator(cell);
    while (port_iter->hasNext()) {
      const Port *port = port_iter->next();
      if (network_->hasMembers(port)) {
        PortMemberIterator *member_iter = network_->memberIterator(port);
        while (member_iter->hasNext()) {
          const Port *member = member_iter->next();
          if (pattern->match(name(member)))
            matches.push_back(member);
        }
        delete member_iter;
      }
    }
    delete port_iter;
    rule = " (bus bit match)";
  }
  if (matches.empty()) {
    rule.clear();
    std::string folded_pattern = foldName(query);
    PatternMatch folded_match(folded_pattern, pattern);
    auto visit = [&](const Port *port) {
      if (folded_match.match(foldName(network_->name(port))))
        matches.push_back(port);
    };
    CellPortIterator *port_iter = network_->portIterator(cell);
    while (port_iter->hasNext()) {
      const Port *port = port_iter->next();
      visit(port);
      if (network_->hasMembers(port)) {
        PortMemberIterator *member_iter = network_->memberIterator(port);
        while (member_iter->hasNext())
          visit(member_iter->next());
        delete member_iter;
      }
    }
    delete port_iter;
    if (!glob && matches.size() > 1) {
      std::vector<std::string> candidates;
      for (const Port *port : matches)
        candidates.push_back(name(port));
      reportFoldAmbiguous(query, candidates);
      matches.clear();
    }
  }
  if (!matches.empty()) {
    std::vector<std::string> targets;
    for (const Port *port : matches)
      targets.push_back(name(port));
    reportFoldRescue(query, targets, rule);
  }
  return matches;
}

////////////////////////////////////////////////////////////////

void
SdcNetwork::reportFoldRescue(std::string_view query,
                             const std::vector<std::string> &targets,
                             std::string_view rule) const
{
  std::string key = "resolved ";
  key += query;
  if (!fold_reported_.insert(key).second)
    return;
  const std::string &first = targets[0];
  if (targets.size() == 1)
    report_->warn(2740, "SDC object '{}' resolved to '{}' by name folding{}.",
                  query, first, rule);
  else {
    size_t more = targets.size() - 1;
    report_->warn(2741, "SDC object '{}' resolved to '{}' and {} more by name folding{}.",
                  query, first, more, rule);
  }
}

void
SdcNetwork::reportFoldAmbiguous(std::string_view query,
                                const std::vector<std::string> &candidates) const
{
  std::string key = "ambiguous ";
  key += query;
  if (!fold_reported_.insert(key).second)
    return;
  size_t count = candidates.size();
  std::string list = quotedList(candidates);
  report_->warn(2742, "SDC object '{}' is ambiguous under name folding ({} candidates: {}); not resolved.",
                query, count, list);
}

} // namespace sta
