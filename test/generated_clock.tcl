read_liberty generated_clock.lib
read_verilog generated_clock.v
link_design generated_clock
create_clock -name clk -period 10 [get_ports CLK_IN_1] 
create_clock -name clk2 -period 100 [get_ports CLK_IN_2] 

# Should see 9 clocks
puts "Number of clocks: [ llength [get_clocks]]"

# Report all clock periods
set clock_list {}
foreach_in_collection clk [get_clocks] {
  lappend clock_list [get_object_name $clk]
}
foreach clk_name [lsort -dictionary $clock_list] {
  set clk [get_clocks $clk_name]
  puts "$clk_name period: [get_attribute $clk period]"
}

# Use TCL command to create a generated clock from generated clock
create_generated_clock \
  -name clk_manual \
  -source [get_pins clk_edge_shift/CLK_OUT] \
  -master_clock clk_edge_shift/CLK_OUT \
  -divide_by 2 \
  [get_ports CLK_OUT_2]

# Should see 10 clocks
puts "Number of clocks: [ llength [get_clocks]]"

# Use command to validate waveforms
set clk_properties [report_clock_properties]

# Split into lines, sort data rows while preserving header
set lines [split $clk_properties "\n"]
set header [lrange $lines 0 1]
set data_lines [lrange $lines 2 end]
set sorted_data [lsort -dictionary $data_lines]
set sorted_output [join [concat $header $sorted_data] "\n"]
puts $sorted_output
