# reset_path
source helpers.tcl
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc report_exceptions { title } {
  set sdc_file [make_result_file reset_path.sdc]
  write_sdc -no_timestamp $sdc_file
  set stream [open $sdc_file r]
  set sdc [string map {"\\\n" ""} [read $stream]]
  close $stream
  puts "-- $title"
  foreach line [split $sdc "\n"] {
    if { [regexp {^(set_false_path|set_multicycle_path|set_max_delay|set_min_delay|set_path_margin|group_path)} $line] } {
      puts [string trim [regsub -all {\s+} $line " "]]
    }
  }
}

set_false_path -from [get_cells r1]
set_false_path -from [get_cells r2] -to [get_cells r3]
set_max_delay 3 -through [get_pins u1/Z]
report_exceptions "initial"
reset_path -from [get_cells r1]
report_exceptions "reset_path -from r1"
reset_path -from [get_cells r2]
report_exceptions "reset_path -from r2"
reset_path -through [get_pins u1/Z]
report_exceptions "reset_path -through u1/Z"

set_false_path -from [get_cells {r1 r2}] -to [get_cells r3]
reset_path -from [get_cells r1] -to [get_cells r3]
report_exceptions "reset_path -from r1 -to r3 with -from {r1 r2}"
reset_path -to [get_cells r3]
report_exceptions "reset_path -to r3"

set_multicycle_path -setup 2 -to [get_pins r3/D]
set_multicycle_path -hold 1 -to [get_pins r3/D]
reset_path -hold -to [get_pins r3/D]
report_exceptions "reset_path -hold"
reset_path -setup -to [get_pins r3/D]
report_exceptions "reset_path -setup"

set_false_path -rise_from [get_cells r1]
set_false_path -fall_to [get_pins r3/D]
reset_path -from [get_cells r1]
reset_path -to [get_pins r3/D]
report_exceptions "reset_path -from/-to leaves -rise_from/-fall_to"
reset_path -rise_from [get_cells r1]
reset_path -fall_to [get_pins r3/D]
report_exceptions "reset_path -rise_from/-fall_to"

set_false_path -rise -from [get_cells r1]
set_false_path -rise -from [get_cells r2] -to [get_cells r3]
reset_path -fall -from [get_cells {r1 r2}]
report_exceptions "reset_path -fall leaves -rise"
reset_path -rise -from [get_cells {r1 r2}]
report_exceptions "reset_path -rise"

set_false_path -fall -from [get_cells {r1 r2}]
reset_path -from [get_cells r1]
report_exceptions "reset_path -from r1 with -fall -from {r1 r2}"
reset_path -from [get_cells r2]

set_min_delay 1 -from [get_ports in2]
set_path_margin -setup 0.5 -to [get_pins r3/D]
group_path -name g1 -from [get_ports in2]
reset_path -from [get_ports in2]
reset_path -to [get_pins r3/D]
report_exceptions "reset_path set_min_delay/set_path_margin leaves group_path"

set_false_path -to [get_pins r3/D]
report_checks -to [get_pins r3/D] -format end
reset_path -to [get_pins r3/D]
report_checks -to [get_pins r3/D] -format end

set sdc_file [make_result_file reset_path_read.sdc]
set stream [open $sdc_file w]
puts $stream "set_false_path -from \[get_cells r1\] -to \[get_cells r3\]"
puts $stream "set_multicycle_path -setup 2 -from \[get_cells r2\]"
puts $stream "reset_path -from \[get_cells r1\]"
close $stream
read_sdc $sdc_file
report_exceptions "read_sdc with reset_path"

catch { reset_path -setup } msg
puts $msg
catch { reset_path -from [get_cells r2] r3 } msg
puts $msg
