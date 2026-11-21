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

#include "Sequential.hh"

#include "FuncExpr.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"

namespace sta {

Sequential::Sequential(bool is_register,
                       FuncExpr *clock,
                       FuncExpr *data,
                       FuncExpr *clear,
                       FuncExpr *preset,
                       LogicValue clr_preset_out,
                       LogicValue clr_preset_out_inv,
                       LibertyPort *output,
                       LibertyPort *output_inv) :
  is_register_(is_register),
  clock_(clock),
  data_(data),
  clear_(clear),
  preset_(preset),
  clr_preset_out_(clr_preset_out),
  clr_preset_out_inv_(clr_preset_out_inv),
  output_(output),
  output_inv_(output_inv)
{
}

Sequential::Sequential(Sequential &&other) noexcept :
  is_register_(other.is_register_),
  clock_(other.clock_),
  data_(other.data_),
  clear_(other.clear_),
  preset_(other.preset_),
  clr_preset_out_(other.clr_preset_out_),
  clr_preset_out_inv_(other.clr_preset_out_inv_),
  output_(other.output_),
  output_inv_(other.output_inv_)
{
  other.clock_ = nullptr;
  other.data_ = nullptr;
  other.clear_ = nullptr;
  other.preset_ = nullptr;
}

Sequential &
Sequential::operator=(Sequential &&other) noexcept
{
  if (this != &other) {
    delete clock_;
    delete data_;
    delete clear_;
    delete preset_;
    is_register_ = other.is_register_;
    clock_ = other.clock_;
    data_ = other.data_;
    clear_ = other.clear_;
    preset_ = other.preset_;
    clr_preset_out_ = other.clr_preset_out_;
    clr_preset_out_inv_ = other.clr_preset_out_inv_;
    output_ = other.output_;
    output_inv_ = other.output_inv_;
    other.clock_ = nullptr;
    other.data_ = nullptr;
    other.clear_ = nullptr;
    other.preset_ = nullptr;
  }
  return *this;
}

Sequential::~Sequential()
{
  delete clock_;
  delete data_;
  delete clear_;
  delete preset_;
}

////////////////////////////////////////////////////////////////

Statetable::Statetable(LibertyPortSeq &input_ports,
                       LibertyPortSeq &internal_ports,
                       StatetableRows &table) :
  input_ports_(input_ports),
  internal_ports_(internal_ports),
  table_(table)
{
}

bool
Statetable::isLatch() const
{
  auto is_value = [](StateInternalValue value) {
    return value == StateInternalValue::low
      || value == StateInternalValue::high
      || value == StateInternalValue::low_high
      || value == StateInternalValue::high_low;
  };
  for (const StatetableRow &row : table_) {
    for (StateInputValue value : row.inputValues()) {
      if (value == StateInputValue::rise
          || value == StateInputValue::fall
          || value == StateInputValue::not_rise
          || value == StateInputValue::not_fall)
        return false;
    }
    // A register's second stage loads the first stage's stored value.
    const StateInternalValues &currents = row.currentValues();
    const StateInternalValues &nexts = row.nextValues();
    for (size_t i = 0; i < nexts.size(); i++) {
      if (is_value(nexts[i])) {
        for (size_t j = 0; j < currents.size(); j++) {
          if (j != i && is_value(currents[j]))
            return false;
        }
      }
    }
  }
  return true;
}

// The one input of row that is at a level, apart from skip.
static bool
rowLevelInput(const StatetableRow &row,
              size_t skip,
              // Return values.
              size_t &input,
              bool &high)
{
  bool found = false;
  const StateInputValues &inputs = row.inputValues();
  for (size_t i = 0; i < inputs.size(); i++) {
    if (i != skip && inputs[i] != StateInputValue::dont_care) {
      if (found
          || (inputs[i] != StateInputValue::low
              && inputs[i] != StateInputValue::high))
        return false;
      input = i;
      high = inputs[i] == StateInputValue::high;
      found = true;
    }
  }
  return found;
}

bool
Statetable::latchInputs(size_t node,
                        // Return values.
                        size_t &enable,
                        bool &enable_inv,
                        size_t &data,
                        bool &data_inv) const
{
  bool has_enable = false;
  // A row that holds the node gives the enable alone, at the level that closes it.
  for (const StatetableRow &row : table_) {
    if (row.nextValues()[node] == StateInternalValue::hold) {
      size_t input;
      bool high;
      if (!rowLevelInput(row, row.inputValues().size(), input, high)
          || (has_enable && (input != enable || high != enable_inv)))
        return false;
      enable = input;
      enable_inv = high;
      has_enable = true;
    }
  }
  // A row that loads it has the enable open and gives the data.
  bool loads_low = false;
  bool loads_high = false;
  for (const StatetableRow &row : table_) {
    for (StateInternalValue current : row.currentValues()) {
      if (current != StateInternalValue::unspecified)
        return false;
    }
    StateInternalValue next = row.nextValues()[node];
    if (next == StateInternalValue::low || next == StateInternalValue::high) {
      bool next_high = next == StateInternalValue::high;
      size_t input;
      bool high;
      if (!has_enable
          || row.inputValues()[enable] != (enable_inv
                                          ? StateInputValue::low
                                          : StateInputValue::high)
          || !rowLevelInput(row, enable, input, high)
          || ((loads_low || loads_high)
              && (input != data || (high != next_high) != data_inv)))
        return false;
      data = input;
      data_inv = high != next_high;
      (next_high ? loads_high : loads_low) = true;
    }
    else if (next != StateInternalValue::hold)
      return false;
  }
  return loads_low && loads_high;
}

StatetableRow::StatetableRow(StateInputValues &input_values,
                             StateInternalValues &current_values,
                             StateInternalValues &next_values) :
  input_values_(input_values),
  current_values_(current_values),
  next_values_(next_values)
{
}

} // namespace sta
