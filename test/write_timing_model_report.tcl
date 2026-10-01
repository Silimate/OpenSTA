# report_checks for paths that end at write_timing_model -internal_paths
# model internal pins, flat and with the block model.
source helpers.tcl

proc run_child { tag body } {
  set script [make_result_file "write_timing_model_report.$tag.tcl"]
  set stream [open $script "w"]
  puts $stream $body
  puts $stream "exit"
  close $stream
  catch { exec [info nameofexecutable] -no_init -no_splash -exit $script 2>@1 } output
  return $output
}

set model_file [make_result_file "write_timing_model_report_blk.lib"]

puts -nonewline [run_child blk "
read_liberty write_timing_model_hier.lib
read_verilog write_timing_model_hier_blk.v
link_design blk
create_clock -name clka -period 10 \[get_ports clka\]
create_clock -name clkb -period 10 \[get_ports clkb\]
set_propagated_clock \[all_clocks\]
write_timing_model -scalar -internal_paths $model_file
"]

set reports {
link_design top
create_clock -name clk1 -period 4 [get_ports ck1]
create_clock -name clk2 -period 3 -waveform {0.5 2} [get_ports ck2]
set_input_delay 0.5 -clock clk1 [get_ports {in0 rst}]
set_output_delay 0.5 -clock clk2 [get_ports {out0 out1 out2}]
set_propagated_clock [all_clocks]
# register -> register inside the block
report_checks -to i2/r2/D -format full_clock_expanded
# input -> register inside a nested block
report_checks -to i0/k0/r1/D -format full_clock_expanded
# hold
report_checks -path_delay min -to i2/r5/D
# recovery
report_checks -to i2/r6/RN
# register inside the block -> block output -> output port
report_checks -to out2
}

puts "flat"
puts -nonewline [run_child flat "
read_liberty write_timing_model_hier.lib
read_verilog write_timing_model_hier_blk.v
read_verilog write_timing_model_hier_mid.v
read_verilog write_timing_model_hier_top.v
$reports"]

puts "model"
puts -nonewline [run_child model "
read_liberty write_timing_model_hier.lib
read_liberty $model_file
read_verilog write_timing_model_hier_mid.v
read_verilog write_timing_model_hier_top.v
$reports"]
