puts "Reading liberty"
read_liberty nangate45_slow.lib.gz
read_liberty timing_paths.lib
puts "Reading verilog"
read_verilog report_checks.v
puts "Linking design"
link_design top
puts "Creating clock"
create_clock -name clk -period 10 {clk}
puts "Setting input delay"
set_input_delay -clock clk 0 {in}
puts "Report checks"
report_checks