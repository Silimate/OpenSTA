# Report pin activities
proc report_activities { } {
  set pins [get_pins -hierarchical *]
  set clk_freq [expr 1.0 / (10 * 1e-12)]
  puts "Pin Name Activity Duty Cycle"
  puts "--------------------------------------------------------"
  foreach pin $pins {
    set prop [get_property $pin activity]
    # Split string into proper list if needed
    set prop_list [split $prop]
    set trans_str [lindex $prop_list 0]
    set duty_str [lindex $prop_list 1]
    # Extract numeric values
    if {[scan $trans_str "%g" trans_num] == 1 && [scan $duty_str "%g" duty_num] == 1} {
      set activity [expr {double($trans_num) / ($clk_freq * 2.0)}]
      puts "[get_full_name $pin] $activity $duty_num"
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
