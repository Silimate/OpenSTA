# delay calc example
read_liberty nangate45_slow.lib.gz
read_verilog timing_test.v
link_design timing_test
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2 in3}
write_timing_model timing_test.lib