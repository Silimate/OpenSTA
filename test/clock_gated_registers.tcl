# sta::clock_gated_registers follows netlist edits between timing updates.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog clock_gated_registers.v
link_design top
create_clock -name clk -period 10 [get_ports clk]

proc show_gated { label } {
  report_wns
  set names {}
  foreach inst [sta::clock_gated_registers] {
    lappend names [get_full_name $inst]
  }
  puts "$label: [lsort $names]"
}

show_gated "initial"
disconnect_pin gclk r1/CK
connect_pin clk r1/CK
show_gated "r1 on clk"
disconnect_pin clk r2/CK
connect_pin gclk r2/CK
show_gated "r2 on gclk"
