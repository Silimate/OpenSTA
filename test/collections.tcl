read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v
link_design gcd

puts "add_to_collection"
report_object_full_names [add_to_collection [get_ports req_*] [get_ports resp_*]]

puts "append_to_collection"
set resp_ports [get_ports req_*]
append_to_collection resp_ports [get_ports resp_*]
report_object_full_names $resp_ports

puts "copy_collection"
report_object_full_names [copy_collection [get_ports req_*]]

puts "filter_collection"
report_object_full_names [filter_collection [get_ports resp_*] "direction==output " -quiet]

puts "foreach_in_collection"
foreach_in_collection port [get_ports req_*] {
  puts "foreach_in_collection port: [get_full_name $port]"
}

puts "get_collection_size"
puts [get_collection_size [get_ports req_*]]

puts "index_collection"
report_object_full_names [index_collection [get_ports req_*] 1]

puts "remove_from_collection"
report_object_full_names [remove_from_collection [get_ports re*] [get_ports req_*]]

puts "remove_from_collection -intersect"
report_object_full_names [remove_from_collection -intersect [get_ports re*] [get_ports req_*]]

puts "sizeof_collection"
puts [sizeof_collection [get_ports req_*]]

puts "query_collection"
report_object_full_names [query_collection [get_ports req_*]]

puts "sort_collection ascending by full_name"
foreach_in_collection p [sort_collection [get_ports resp_msg*] full_name] {
  puts [get_full_name $p]
}

puts "sort_collection descending by full_name"
foreach_in_collection p [sort_collection -descending [get_ports resp_msg*] full_name] {
  puts [get_full_name $p]
}

puts "sort_collection ascending (explicit) by full_name"
foreach_in_collection p [sort_collection -ascending [get_ports resp_msg*] full_name] {
  puts [get_full_name $p]
}

puts "sort_collection on empty collection"
foreach_in_collection p [sort_collection [get_ports -quiet nonexistent*] full_name] {
  puts [get_full_name $p]
}

puts "append_to_collection -unique"
set resp_ports [get_ports req_*]
append_to_collection -unique resp_ports [get_ports req_*]
append_to_collection resp_ports [get_ports req_*]
append_to_collection resp_ports [get_ports req_*] -unique

# There should be two of each port
report_object_full_names $resp_ports
