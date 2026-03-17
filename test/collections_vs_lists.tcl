read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v
link_design gcd

foreach mode {0 1} {
  set ::sta_enable_collections $mode
  if {$mode == 0} {
    puts "== TCL List Mode =="
  } else {
    puts "== Collection Mode =="
  }
  
  set collection [get_ports req_*]

  puts "\tis_collection: [sta::is_collection $collection]"
  puts "\tget_collection_size: [get_collection_size $collection]"
  puts "\tsizeof_collection: [sizeof_collection $collection]"

  puts "\tforeach_in_collection"
  foreach_in_collection port $collection {
    puts "foreach_in_collection port: [get_full_name $port]"
  }

  puts -nonewline "\tadd_to_collection: result is collection? "
  set add_result [add_to_collection $collection [get_ports resp_*]]
  puts "[sta::is_collection $add_result]"
  report_object_full_names $add_result
  
  puts -nonewline "\tcopy_collection: result is collection? "
  set copy_result [copy_collection $collection]
  puts "[sta::is_collection $copy_result]"
  report_object_full_names $copy_result

  # llength should be 1 for the collection, but sizeof_collection should be the same
  puts -nonewline "\tappend_to_collection: llength? "
  set appendable [copy_collection $collection]
  append_to_collection appendable [get_ports resp_*]
  puts "[llength $appendable]; sizeof_collection [sizeof_collection $appendable]"
  report_object_full_names $appendable

  # size should not change
  puts -nonewline "\tappend_to_collection -unique: size changed? "
  set before [sizeof_collection $appendable]
  append_to_collection -unique appendable [get_ports resp_*]
  puts "[expr $before != [sizeof_collection $appendable]]"

  puts -nonewline "\tindex_collection (singleton): result is collection? "
  set index_singleton_result [index_collection $collection 1]
  puts "[sta::is_collection $index_singleton_result]"
  report_object_full_names $index_singleton_result

  puts -nonewline "\tindex_collection (slice): result is collection? "
  set index_slice_result [index_collection $collection 1 5]
  puts "[sta::is_collection $index_slice_result]"
  report_object_full_names $index_slice_result

  puts "\tremove_from_collection: "
  report_object_full_names [remove_from_collection [get_ports re*] $collection]

  puts "\tquery_collection: "
  report_object_full_names [query_collection [get_ports req_*]]

  puts -nonewline "\tfilter_collection: result is collection? "
  set filter_result [filter_collection [get_ports res*] "direction==output " -quiet]
  puts "[sta::is_collection $filter_result]"
  report_object_full_names $filter_result
}
