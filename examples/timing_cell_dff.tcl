# timing cell dff example
read_liberty nangate45_slow.lib.gz
read_verilog timing_cell_dff.v
link_design timing_cell_dff
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
write_timing_model -paths timing_cell_dff.lib
report_checks -path_delay min_rise -from r1/Q