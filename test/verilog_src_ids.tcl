# src entries starting with the set_verilog_src_id_prefix prefix are read into
# src_ids instead of src; JSON path reports name them only when asked
read_liberty ../examples/asap7_small_ss.lib.gz
sta::set_verilog_src_id_prefix pqid:
read_verilog verilog_src_ids.v
link_design counter
create_clock -name clk [get_ports clk] -period 50

set top_cell [[sta::top_instance] cell]
puts "counter: src = \"[get_property $top_cell src]\" src_ids = \"[get_property $top_cell src_ids]\""
foreach name {_1415_ _1416_ _1417_} {
  set inst [get_cell $name]
  puts "$name: src = \"[get_property $inst src]\" src_ids = \"[get_property $inst src_ids]\""
}
puts "_1415_: attr1 = \"[get_property [get_cell _1415_] attr1]\""

proc json_src_ids { } {
  with_output_to_variable json { report_checks -from _1415_/CLK -to _1416_/D -format json }
  return [regexp -all -inline {"src_ids": "[^"]*"} $json]
}
puts "json: [json_src_ids]"
sta::set_report_path_src_ids 1
puts "json with src_ids: [json_src_ids]"
sta::set_report_path_src_ids 0

# Without a prefix src is read as written
sta::set_verilog_src_id_prefix ""
read_verilog verilog_src_ids.v
link_design counter
puts "_1415_: src = \"[get_property [get_cell _1415_] src]\""
