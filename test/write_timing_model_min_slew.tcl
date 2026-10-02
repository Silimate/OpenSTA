# write_timing_model keeps the min (retaining) model of an arc when the
# min and max delays are the same but the slews are different.
source helpers.tcl

proc run_child { tag body } {
  set script [make_result_file "write_timing_model_min_slew.$tag.tcl"]
  set stream [open $script "w"]
  puts $stream $body
  puts $stream "exit"
  close $stream
  catch { exec [info nameofexecutable] -no_init -no_splash -exit $script 2>@1 } output
  return $output
}

set model_file [make_result_file "write_timing_model_min_slew_blk.lib"]
set output [run_child blk "
read_liberty write_timing_model_hier.lib
read_verilog write_timing_model_min_slew.v
link_design min_slew_blk
create_clock -name clk -period 10
set_input_delay -clock clk 0 \[all_inputs\]
set_output_delay -clock clk 0 \[all_outputs\]
write_timing_model -scalar -cell_name min_slew_model $model_file
"]
if { $output != "" } {
  puts $output
}

foreach top {flat_top model_top} {
  puts $top
  puts [run_child $top "
read_liberty write_timing_model_hier.lib
read_liberty $model_file
read_verilog write_timing_model_min_slew.v
link_design $top
report_slews u2/A
"]
}
