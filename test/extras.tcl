read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v
link_design gcd

set_dont_use sky130_fd_sc_hd__a2111o_1
set_dont_touch sky130_fd_sc_hd__a2111o_1
# get_db/set_db point at the SDC translation guide
catch { get_db program_short_name } msg
puts $msg
all_fanin -to [get_ports resp_rdy]
all_fanout -from [get_ports req_rdy]

check_units -time ps -resistance kOhm -capacitance fF -voltage V -current mA
set_units -time ps -resistance kOhm -capacitance fF -voltage V -current mA
check_units -time ps -resistance kOhm -capacitance fF -voltage V -current mA

create_clock -name clk -period 10 [get_ports clk]
puts "Clock period: [get_attribute period [get_clocks clk]]"
puts "Clock period: [get_attribute [get_clocks clk] period]"

# get_attribute -quiet support
set dummy [get_attribute -quiet dummy [get_clocks clk]]
if { $dummy == "" } {
  puts "dummy is empty"
} else {
  puts "dummy is not empty"
}
# Should show warning, not an error
puts [get_attribute dummy [get_clocks clk]]

# get_object_name on invalid returns warning
get_object_name [get_clocks *_x4*]

# get_object_name on lists
create_clock -name clk_x4 -period 10000
create_clock -name clk_x41 -period 10101
puts [get_object_name [get_clocks *_x4*]]

# reset/remove aliases for "unset" commands
set test_port [get_ports req_val]

# Sets delay and verifies the path has input external delay
set_input_delay -clock clk 100 $test_port
report_checks -from $test_port -path_delay max -format full -endpoint_path_count 1 -unconstrained

# Removes the delay and verifies the path has no input external delay
remove_input_delay -clock clk $test_port
report_checks -from $test_port -path_delay max -format full -endpoint_path_count 1 -unconstrained

# Same thing but for reset
set_input_delay -clock clk 100 $test_port
report_checks -from $test_port -path_delay max -format full -endpoint_path_count 1 -unconstrained

reset_input_delay -clock clk $test_port
report_checks -from $test_port -path_delay max -format full -endpoint_path_count 1 -unconstrained

# support for -q quiet flag, should be the same for last 2 (empty)
puts [get_pins clk_x41]
puts [get_pins -quiet clk_x41]
puts [get_pins -q clk_x41]

# support for -th or -thr flag for through lists in general

# Test set_false_path with -through (full form) using net
set_false_path -from $test_port -thr [get_pins _282_/Y]
set_false_path -from $test_port -th [get_pins _289_/Y]

# Nothing should show up since we set these false paths
report_checks -from $test_port -path_delay max -format full -endpoint_path_count 1 -unconstrained
