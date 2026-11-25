# get_pins -leaf follows nets through hierarchy to leaf pins
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog get_cell_hierarchy.v
link_design dut

puts {[get_pins -of_objects [get_nets w_mid1]]}
report_object_full_names [get_pins -of_objects [get_nets w_mid1]]

puts {[get_pins -leaf -of_objects [get_nets w_mid1]]}
report_object_full_names [get_pins -leaf -of_objects [get_nets w_mid1]]

puts {[get_pins -leaf -of_objects [get_nets {w_mid1 u_blk1/mid1 u_blk2/mid1}]]}
report_object_full_names [get_pins -leaf -of_objects [get_nets {w_mid1 u_blk1/mid1 u_blk2/mid1}]]

puts {[get_pins -leaf -of_objects [get_nets clk1]]}
report_object_full_names [get_pins -leaf -of_objects [get_nets clk1]]

puts {[get_pins -leaf -of_objects [get_ports clk1]]}
report_object_full_names [get_pins -leaf -of_objects [get_ports clk1]]

puts {[get_pins -leaf -of_objects [get_cells {u_blk1 u_blk1/blk_r1}]]}
report_object_full_names [get_pins -leaf -of_objects [get_cells {u_blk1 u_blk1/blk_r1}]]

puts {[get_pins -leaf -of_objects [get_nets w_mid1] -filter "direction==output"]}
report_object_full_names \
  [get_pins -leaf -of_objects [get_nets w_mid1] -filter "direction==output"]

puts {[get_pins -hier -filter "direction==input" *1]}
report_object_full_names [get_pins -hier -filter "direction==input" *1]

puts {[get_pins -leaf -hier -filter "direction==input" *1]}
report_object_full_names [get_pins -leaf -hier -filter "direction==input" *1]

create_clock -name clk1 -period 10 [get_ports clk1]
create_generated_clock -name div2 -divide_by 2 \
  -source [get_pins -leaf -of_objects [get_nets clk1]] \
  [get_pins -leaf -of_objects [get_nets w_mid1] -filter "direction==output"]
report_clock_properties
