# write_context_sdc lets the block alone see the timing its instances see in top
source helpers.tcl
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog write_context_sdc.v

# Worst slack of the paths through pin.
proc slack_thru { pin path_delay } {
  set path_end [find_timing_paths -through $pin -path_delay $path_delay]
  return [format %.3f [get_property $path_end slack]]
}

# Worst slack of the paths to or from pin.
proc slack_at { key pin path_delay } {
  set path_end [find_timing_paths $key $pin -path_delay $path_delay]
  return [format %.3f [get_property $path_end slack]]
}

link_design top
create_clock -name clk -period 1 [get_ports clk]
set_input_delay 0.1 -clock clk [get_ports in]
set_output_delay 0.1 -clock clk [get_ports {out1 out2 out3}]
foreach inst {u_blk1 u_blk2} {
  foreach path_delay {max min} {
    puts "$inst $path_delay in [slack_thru $inst/d $path_delay] out [slack_thru $inst/q $path_delay]"
  }
}

set blk1_sdc [make_result_file write_context_sdc_blk1.sdc]
set blk_sdc [make_result_file write_context_sdc_blk.sdc]
write_context_sdc -instances u_blk1 $blk1_sdc
write_context_sdc -instances {u_blk1 u_blk2} $blk_sdc
report_file $blk1_sdc
report_file $blk_sdc

if { [catch { write_context_sdc -instances r_in [make_result_file write_context_sdc_leaf.sdc] } msg] } {
  puts $msg
}

# u_blk1's file reproduces u_blk1; the merged one is at least as tight as
# either instance on each side.
foreach sdc [list $blk1_sdc $blk_sdc] {
  read_verilog write_context_sdc.v
  link_design blk
  read_sdc $sdc
  foreach path_delay {max min} {
    puts "blk $path_delay in [slack_at -to r1/D $path_delay] out [slack_at -from r1/CK $path_delay]"
  }
}
