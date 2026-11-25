# all_connected returns a net's pins and ports, or a pin's or port's net
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog get_cell_hierarchy.v
link_design dut

puts {[all_connected [get_nets w_mid1]]}
report_object_full_names [all_connected [get_nets w_mid1]]

puts {[all_connected -leaf [get_nets w_mid1]]}
report_object_full_names [all_connected -leaf [get_nets w_mid1]]

puts {[all_connected [get_nets clk1]]}
report_object_full_names [all_connected [get_nets clk1]]

puts {[all_connected -leaf [get_nets clk1]]}
report_object_full_names [all_connected -leaf [get_nets clk1]]

puts {[all_connected [get_pins u_blk1/blk_r1/Q]]}
report_object_full_names [all_connected [get_pins u_blk1/blk_r1/Q]]

puts {[all_connected u_blk1/blk_r1/Q]}
report_object_full_names [all_connected u_blk1/blk_r1/Q]

puts {[all_connected [get_ports clk1]]}
report_object_full_names [all_connected [get_ports clk1]]

puts {[all_connected [all_connected [get_pins u_blk2/blk_u2/A1]]]}
report_object_full_names [all_connected [all_connected [get_pins u_blk2/blk_u2/A1]]]

puts {[all_connected u_blk1]}
report_object_full_names [all_connected u_blk1]

puts {[all_connected -leaf [get_nets u_blk1/clk1]]}
report_object_full_names [all_connected -leaf [get_nets u_blk1/clk1]]

puts {llength [all_connected -leaf [get_nets {w_mid1 u_blk1/mid1 u_blk2/mid1}]]}
puts [llength [all_connected -leaf [get_nets {w_mid1 u_blk1/mid1 u_blk2/mid1}]]]
