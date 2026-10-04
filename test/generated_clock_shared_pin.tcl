# Generated clocks from Liberty generated_clock groups that share a clock
# pin are named by their group, and a group named like another generated
# clock pin of the cell does not replace that pin's clock.
read_liberty generated_clock_shared_pin.lib
read_verilog generated_clock_shared_pin.v
link_design generated_clock_shared_pin
create_clock -name clk -period 10 [get_ports clk]
foreach clk [lsort [get_object_name [get_clocks *]]] {
  set clk_obj [get_clocks $clk]
  puts "$clk period [get_attribute $clk_obj period] pins [lsort [get_full_name [get_attribute $clk_obj sources]]]"
}
