puts "Reading liberty"
read_liberty nangate45_slow.lib.gz
read_liberty timing_test.lib
puts "Reading verilog"
read_verilog top.v
puts "Linking design"
link_design top
puts "Creating clock"
create_clock -name clk -period 10 {clk1 clk2 clk3}
puts "Setting input delay"
set_input_delay -clock clk 0 {in1 in2 in3}
puts "Report checks"
report_checks