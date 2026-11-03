# write_timing_model finds the load and limits of block ports on one
# net, such as outputs tied to a constant, once for the net.
source helpers.tcl
read_liberty write_timing_model_hier.lib
read_verilog write_timing_model_shared_nets.v
link_design shared_nets_blk
create_clock -name clk -period 10 [get_ports clk]
set_load 0.01 [get_ports y0]
set_load 0.02 [get_ports y1]
set_load 0.03 [get_ports t0]
set_load 0.04 [get_ports t1]
set_max_transition 2 [get_ports {y0 t0}]
set model_file [make_result_file write_timing_model_shared_nets.lib]
write_timing_model -scalar $model_file
report_file $model_file
