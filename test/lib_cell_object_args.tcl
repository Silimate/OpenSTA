# Commands whose lib_cell argument is a liberty cell object instead of a name.
# Collections are on so a one-cell LibertyCellSeq can be passed.
set sta_enable_collections 1
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog ../examples/example1.v
link_design top

proc ref_name { inst } {
  return [get_property [get_cells $inst] ref_name]
}

set buf_x2 [get_lib_cells */BUF_X2]
puts "[sta::object_type $buf_x2]: [replace_cell u1 $buf_x2]"
puts "u1 [ref_name u1]"

# -filter returns a LibertyCellSeq even for a single match.
set buf_x4 [get_lib_cells -filter "name == BUF_X4" */*]
puts "[sta::object_type $buf_x4]: [replace_cell u1 $buf_x4]"
puts "u1 [ref_name u1]"

make_instance u3 [get_lib_cells */INV_X1]
puts "u3 [ref_name u3]"

# An object that is not a liberty cell warns, and nothing changes.
puts "instance: [replace_cell u1 [get_cells u2]]"
puts "u1 [ref_name u1]"
