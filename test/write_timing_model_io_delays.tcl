# write_timing_model leaves set_input_delay and set_output_delay out of
# the model unless -io_delays is used.
source helpers.tcl

# The model's cell, without the library header.
proc report_model_cell { file } {
  set stream [open $file r]
  set in_cell 0
  while { [gets $stream line] >= 0 } {
    if { [string match "*cell (*" $line] } {
      set in_cell 1
    }
    if { $in_cell } {
      puts $line
    }
  }
  close $stream
}

read_liberty write_timing_model_hier.lib
read_verilog write_timing_model_io_delays.v
link_design io_delays_blk
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 2 [get_ports {a b d}]
set_output_delay -clock clk 3 [get_ports {y q}]

set model_file [make_result_file write_timing_model_io_delays.lib]
foreach flags {{-scalar} {-scalar -io_delays} {-io_delays}} {
  puts "write_timing_model $flags"
  write_timing_model {*}$flags $model_file
  report_model_cell $model_file
}
# The block's own timing is unchanged.
report_checks -from a -to y
