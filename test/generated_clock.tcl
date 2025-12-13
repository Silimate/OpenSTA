read_liberty generated_clock.lib

read_verilog generated_clock.v
link_design generated_clock
create_clock -name clk -period 10 [get_ports SYS_CLK_IN]

# Should see two clocks clk, CLK_OUT_DIV2
puts [get_object_name [get_clocks]]

# Period of the generated clock should be 2x the period of the source clock 'clk'
foreach_in_collection clk [get_clocks] {
  puts "[get_object_name $clk] period: [get_attribute $clk period]"
}
