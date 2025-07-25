# timing cel comb example
read_liberty nangate45_slow.lib.gz
read_verilog timing_cell_comb.v
link_design timing_cell_comb
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
write_timing_model results/timing_cell_comb.log