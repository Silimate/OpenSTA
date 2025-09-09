read_liberty asap7_small.lib.gz
read_verilog timing_paths_propagated_clock.v
link_design timing_cell
create_clock -name clk -period 500 {clk}
set_propagated_clock {clk}
set_input_delay -clock clk 0 {in}
write_timing_model -paths results/timing_paths_propagated_clock.log
