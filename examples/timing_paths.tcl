# timing paths example
read_liberty nangate45_slow.lib.gz
read_verilog timing_paths.v
link_design timing_paths
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
write_timing_model timing_paths.lib