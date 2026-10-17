# A driver's delay follows its load when a load pin is disconnected or deleted.
read_liberty ../examples/nangate45_typ.lib.gz
proc constrain {} {
  create_clock -name clk -period 10
  set_input_delay 0 -clock clk [all_inputs]
  set_output_delay 0 -clock clk [all_outputs]
}

proc show { label } {
  puts "$label: [format %.4f [get_property [get_pins u2/A] arrival_max_rise]]"
}

read_verilog disconnect_load_delay.v
link_design top
constrain
report_checks > /dev/null
show "both loads"
disconnect_pin n1 u3/A
show "u3/A disconnected"
connect_pin n1 u3/A
show "u3/A reconnected"
delete_instance u3
show "u3 deleted"

read_verilog disconnect_load_delay.v
link_design top_multi_one_load
constrain
report_checks > /dev/null
show "multi-driver, fresh with one load"

read_verilog disconnect_load_delay.v
link_design top_multi
constrain
report_checks > /dev/null
show "multi-driver, both loads"
disconnect_pin n1 u3/A
show "multi-driver, u3/A disconnected"
connect_pin n1 u3/A
show "multi-driver, u3/A reconnected"
delete_instance u3
show "multi-driver, u3 deleted"
