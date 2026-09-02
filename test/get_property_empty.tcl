# get_property on an empty collection used to abort with Error 2200, so any script that
# asked for a property of a get_*/all_* result that matched nothing died on the spot.
# It now warns under the same id and returns an empty list.
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog get_property_empty.v
link_design no_outputs
create_clock -name clk -period 10 [get_ports clk]

proc show { label value } {
  puts "$label: '$value' [llength $value]"
}

# The reported case: a design with no output ports, and the SDC idiom that consumes
# the names it returns.
show "all_outputs" [get_property [all_outputs] name]
show "nets of all_outputs" [get_nets -quiet [get_property [all_outputs] name]]

# Any object type whose lookup matched nothing.
show "get_ports" [get_property [get_ports -quiet no_such_port*] name]
show "get_pins" [get_property [get_pins -quiet no_such_pin*] direction]
show "get_cells" [get_property [get_cells -quiet no_such_inst*] full_name]
show "get_clocks" [get_property [get_clocks -quiet no_such_clock*] period]
show "get_lib_cells" [get_property [get_lib_cells -quiet *no_such_cell*] area]

# Literal empty object arguments, including the -object_type name lookup path.
show "empty list" [get_property {} name]
show "empty string" [get_property "" name]
show "empty -object_type" [get_property -object_type port {} name]

# The warning is silenced per call by -quiet, and by id for a whole script.
show "quiet" [get_property -quiet [all_outputs] name]
suppress_msg 2200
show "suppress_msg 2200" [get_property [all_outputs] name]
unsuppress_msg 2200

# Non-empty results are unchanged: a bare value for one object, a list for many.
show "one port" [get_property [get_ports in1] name]
show "all_inputs" [get_property [all_inputs] name]
