# Clock collections passed to commands that take a clock set (all_registers -clock crashed).
source helpers.tcl
set sta_enable_collections 1
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog get_cell_hierarchy.v
link_design dut

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 10 [get_ports clk2]
set_input_delay -clock clk1 1 [get_ports in1]
set_output_delay -clock clk2 1 [get_ports out]

foreach cmd {
  {all_registers -clock clk1}
  {all_registers -clock [get_clocks clk*]}
  {all_registers -rise_clock clk2 -data_pins}
  {all_registers -fall_clock clk1}
  {all_inputs -clock clk1}
  {all_outputs -clock clk2}
} {
  puts $cmd
  report_object_full_names [eval $cmd]
}

set_clock_groups -asynchronous -group clk1 -group clk2
set_sense -type clock -positive -clocks clk1 [get_pins u_blk1/blk_r1/CK]
set sdc_file [make_result_file clock_set_collections.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
report_clock_latency -clocks clk1
