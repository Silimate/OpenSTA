# delay calc example
read_liberty examples/nangate45_slow.lib
# read_verilog examples/example1.v
read_verilog tmp/test/sample2.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks

# NEW: show cells in top
set top_cells [get_cells -hier *]
puts "Cell count: [llength $top_cells]"
puts "First 5 cells: [lrange $top_cells 0 4]"

puts "---------------------------------------"

foreach c $top_cells {
  puts "full_name:      [get_property $c full_name]"
}
puts "---------------------------------------"
puts "PRINTING u_blk* CELLS"

set blk1_cells [get_cells -hier u_blk*]


foreach c $blk1_cells {
  puts "full_name:      [get_property $c full_name]"
}
puts "---------------------------------------"
puts "PRINTING u_blk1/blk* CELLS"

set blk1_sub_cells [get_cells -hier u_blk1/blk*]

foreach c $blk1_sub_cells {
  puts "full_name:      [get_property $c full_name]"
}
