# A get_ports miss must report a *port*, not an instance.
# port_typename fed find_objects_complete the string "instance", so every missing
# port was announced as a missing instance and could not be told apart from a real
# get_cells miss (warnings 125/127).

read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v
link_design gcd

# Present: no warning, one object.
puts "req_msg\[0\] -> [sizeof_collection [get_ports {req_msg[0]}]]"

# Absent: warning must name a port. gcd's req_msg is [31:0], so [32] is off the end.
puts "req_msg\[32\] -> [sizeof_collection [get_ports {req_msg[32]}]]"

# Absent scalar, and a genuine get_cells miss for contrast.
puts "no_such_port -> [sizeof_collection [get_ports no_such_port]]"
puts "no_such_cell -> [sizeof_collection [get_cells -quiet no_such_cell]]"
