#pragma once

#include "LibertyClass.hh"
#include "SdcClass.hh"
#include <string>

namespace sta {

class GeneratedClock
{
public:
  ~GeneratedClock();
  std::string_view name() const { return name_; }
  std::string_view clockPin() const { return clock_pin_; }
  std::string_view masterPin() const { return master_pin_; }
  int dividedBy() const { return divided_by_; }
  int multipliedBy() const { return multiplied_by_; }
  float dutyCycle() const { return duty_cycle_; }
  bool invert() const { return invert_; }
  IntSeq *edges() const { return edges_; }
  FloatSeq *edgeShifts() const { return edge_shifts_; }

protected:
  GeneratedClock(const char *name,
                 const char *clock_pin,
                 const char *master_pin,
                 int divided_by,
                 int multiplied_by,
                 float duty_cycle,
                 bool invert,
                 IntSeq *edges,
                 FloatSeq *edge_shifts);

  std::string name_;
  std::string clock_pin_;
  std::string master_pin_;
  int divided_by_;
  int multiplied_by_;
  float duty_cycle_;
  bool invert_;
  IntSeq *edges_;
  FloatSeq *edge_shifts_;

  friend class LibertyCell;
};

} // namespace
