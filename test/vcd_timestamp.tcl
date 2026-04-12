# Report pin activities
proc report_activities { } {
  set pins [get_pins -hierarchical *]
  set clk_freq [expr 1.0 / (10 * 1e-12)]
  puts [format "%-30s %-15s %-15s" "Pin Name" "Activity" "Duty Cycle"]
  puts [string repeat "-" 60]
  foreach pin $pins {
    set prop [get_property $pin activity]
    set transitions_per_sec [lindex $prop 0]
    set duty [lindex $prop 1]
    if {[scan $transitions_per_sec "%g" trans_num] == 1 && [scan $duty "%g" duty_num] == 1} {
      set activity [expr double($trans_num) / [expr $clk_freq * 2]]
      puts [format "%-30s %-15.6f %-15.6f" [get_full_name $pin] $activity $duty_num]
    }
  }
  puts ""
}

# Setup
read_liberty asap7_invbuf.lib.gz
read_verilog vcd_timestamp.v
link_design top

# Define clock period in ps
create_clock -name vclk -period 10

# Full VCD reading works (normal behavior)
read_vcd vcd_timestamp.vcd -scope top
report_activities

# Read VCD from start to halfway point
sta::clear_power
read_vcd vcd_timestamp.vcd -scope top -end_time 50
report_activities

# Read VCD from halfway point to end
sta::clear_power
read_vcd vcd_timestamp.vcd -scope top -start_time 50
report_activities

# Read VCD around the input change at time 50
sta::clear_power
read_vcd vcd_timestamp.vcd -scope top -start_time 40 -end_time 60
report_activities

sta::clear_power
read_vcd vcd_timestamp.vcd -scope top -start_time 20 -end_time 60
report_activities

sta::clear_power
read_vcd vcd_timestamp.vcd -scope top -start_time 40 -end_time 80
report_activities
