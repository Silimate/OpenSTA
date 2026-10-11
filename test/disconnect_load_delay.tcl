# A driver's delay follows its load when a load pin is disconnected or deleted.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog disconnect_load_delay.v
link_design top
create_clock -name clk -period 10
set_input_delay 0 -clock clk [get_ports in]
set_output_delay 0 -clock clk [all_outputs]

proc show { label } {
  puts "$label: [format %.4f [get_property [get_pins u2/A] arrival_max_rise]]"
}

report_checks > /dev/null
show "both loads"
disconnect_pin n1 u3/A
show "u3/A disconnected"
connect_pin n1 u3/A
show "u3/A reconnected"
delete_instance u3
show "u3 deleted"
