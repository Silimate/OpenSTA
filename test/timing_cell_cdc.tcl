# timing cell dff example
read_liberty asap7_small.lib.gz
read_verilog timing_cell_cdc.v
link_design timing_cell_cdc
create_clock -name clk -period 500 {clk_a clk_b}
set_input_delay -clock clk 0 {in}
write_timing_model -paths results/timing_cell_cdc.log
