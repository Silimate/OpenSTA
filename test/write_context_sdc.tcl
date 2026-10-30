# write_context_sdc lets the block alone see the timing its instances see in top
source helpers.tcl
set sta_input_port_default_clock 1
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

# in2 has no clock, and u_blk1's output net a wire load
link_design top
create_clock -name clk -period 1 [get_ports clk]
set_input_delay 0.1 -clock clk [get_ports in]
set_input_delay 0.3 [get_ports in2]
set_output_delay 0.1 -clock clk [get_ports {out1 out2 out3 out4}]
set_load 2 [get_nets q1]
foreach inst {u_blk1 u_blk2 u_blk3} {
  foreach path_delay {max min} {
    puts "$inst $path_delay in [slack_thru $inst/d $path_delay] out [slack_thru $inst/q $path_delay]"
  }
}

set blk1_sdc [make_result_file write_context_sdc_blk1.sdc]
set blk3_sdc [make_result_file write_context_sdc_blk3.sdc]
set blk_sdc [make_result_file write_context_sdc_blk.sdc]
write_context_sdc -instances u_blk1 $blk1_sdc
write_context_sdc -instances u_blk3 $blk3_sdc
write_context_sdc -instances {u_blk1 u_blk2} $blk_sdc
report_file $blk1_sdc
report_file $blk3_sdc
report_file $blk_sdc

set bad_sdc [make_result_file write_context_sdc_bad.sdc]
foreach insts {r_in {}} {
  if { [catch { write_context_sdc -instances $insts $bad_sdc } msg] } {
    puts $msg
  }
}

# Each instance's file reproduces it; the merged one is at least as tight as
# either instance on each side.
foreach sdc [list $blk1_sdc $blk3_sdc $blk_sdc] {
  read_verilog write_context_sdc.v
  link_design blk
  read_sdc $sdc
  foreach path_delay {max min} {
    puts "blk $path_delay in [slack_at -to r1/D $path_delay] out [slack_at -from r1/CK $path_delay]"
  }
}
