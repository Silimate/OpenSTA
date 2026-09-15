# get_nets/get_pins -of_objects on top level ports

read_liberty asap7_small.lib.gz
read_verilog get_of_objects_ports.v
link_design top

# A port matches the net connected to it.
puts {[get_nets -of_objects [all_outputs]]}
report_object_full_names [get_nets -of_objects [all_outputs]]
puts {[get_nets -of_objects [get_ports in1]]}
report_object_full_names [get_nets -of_objects [get_ports in1]]

# A port matches the pins on the net connected to it, so the driver idiom
# documented by get_pins works on a port as well as a net.
puts {[get_pins -of_objects [all_outputs]]}
report_object_full_names [get_pins -of_objects [all_outputs]]
puts {[get_pins -of_objects [all_outputs] -filter "direction==output"]}
report_object_full_names [get_pins -of_objects [all_outputs] -filter "direction==output"]
puts {[get_pins -of_objects [get_ports in2]]}
report_object_full_names [get_pins -of_objects [get_ports in2]]

# Ports mix with the other object types accepted by -of_objects.
puts {[get_nets -of_objects [list [get_ports in1] [get_pins u1/Y]]]}
report_object_full_names [get_nets -of_objects [list [get_ports in1] [get_pins u1/Y]]]
puts {[get_pins -of_objects [list [get_ports out1] [get_cells u4]]]}
report_object_full_names [get_pins -of_objects [list [get_ports out1] [get_cells u4]]]

# u4/Y is unconnected, so it contributes no net rather than a null one.
puts {[get_nets -of_objects [get_cells u4]]}
report_object_full_names [get_nets -of_objects [get_cells u4]]
puts {[get_nets -of_objects [get_pins u4/Y]]}
report_object_full_names [get_nets -of_objects [get_pins u4/Y]]
