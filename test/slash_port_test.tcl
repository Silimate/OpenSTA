# Test: ports with escaped names containing slashes
# Exercises get_port_pins_error and parse_*_arg Port-to-Pin conversion
# with escaped Verilog identifiers like \level1/level2/level3
read_liberty asap7_small.lib.gz
read_verilog slash_port_test.v
link_design slash_port_test

create_clock -name clk -period 500 [get_ports out]

puts {get_ports *}
report_object_full_names [get_ports *]

puts {all_inputs}
report_object_full_names [all_inputs]

puts {all_outputs}
report_object_full_names [all_outputs]

puts {get_pins *}
report_object_full_names [get_pins *]

puts {get_cells *}
report_object_full_names [get_cells *]

puts {get_nets *}
report_object_full_names [get_nets *]

puts {get_ports * -filter direction==input}
report_object_full_names [get_ports * -filter {direction == input}]

puts {get_ports * -filter direction==output}
report_object_full_names [get_ports * -filter {direction == output}]

# Bug 1+2: set_input_delay with Port objects (from all_inputs)
# get_port_pins_error receives Port objects, must convert to Pins
# without string round-trip through find_pin [get_name $port]
puts {set_input_delay -clock clk 0 [all_inputs]}
set_input_delay -clock clk 0 [all_inputs]

# Bug 1+2: set_input_delay with individual Port objects
foreach port [all_inputs] {
  puts "set_input_delay -clock clk 0 <Port '[get_name $port]'>"
  set_input_delay -clock clk 0 $port
}

# Bug 3: set_false_path -from with Port objects
# parse_clk_inst_port_pin_arg converts Port to Pin
puts {set_false_path -from [all_inputs] -to [all_outputs]}
set_false_path -from [all_inputs] -to [all_outputs]

# Verify constraints were applied by reporting
puts {report_checks}
report_checks
