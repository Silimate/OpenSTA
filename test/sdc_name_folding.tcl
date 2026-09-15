# SDC name folding (sta_sdc_name_folding): get_* queries that match nothing
# retry with / . [ ] _ runs folded to '_', a register bit index, a clock
# pin alias, and bus bit globs.
read_liberty sdc_name_folding.lib
read_verilog sdc_name_folding.v
link_design sdc_name_folding

proc show { cmd } {
  set names {}
  foreach_in_collection obj [eval $cmd] {
    lappend names [get_full_name $obj]
  }
  puts "$cmd -> {[lsort $names]}"
}

proc show_clocks {} {
  set names {}
  foreach_in_collection clk [all_clocks] {
    lappend names [get_name $clk]
  }
  puts "clocks -> {[lsort $names]}"
}

set rescued {
  {get_pins clkgen_u0/genblk1_0_gen_ch_0_cg_wrap_u0_icg_u0/Q}
  {get_pins ctrl_u0/gen_lane_0_sync_u/ready_sync_reg/Q}
  {get_pins ctrl_u0/gen_lane_0_sync_u/ready_meta_0_reg/Q}
  {get_pins csr_u0/*mode_sel_reg*/CP}
  {get_pins csr_u0/hold_lat_reg/G}
  {get_pins clkgen_u0/genblk1_*_gen_ch_0_cg_wrap_u0_icg_u0/*}
  {get_cells clkgen_u0/genblk1_0_gen_ch_0_cg_wrap_u0}
  {get_cells ctrl_u0/gen_lane_*_sync_u/*sync*}
  {get_nets clkgen_u0/genblk1_0_gen_ch_0_cg_wrap_u0/gclk}
  {get_nets ctrl_u0/gen_lane_0_sync_u/n*}
  {get_ports status*0*}
  {get_ports status_1}
  {get_pins uniq.0.y/Z}
  {get_pins uniq_0_y/Z}
  {get_pins uniq.0.y/Q}
}

set unresolved {
  {get_pins amb.0.x/Z}
  {get_pins amb.0.x/*}
  {get_pins ctrl_u0/gen_lane_0_sync_u/two_bit_reg/Q}
  {get_pins csr_u0/buf_u0/CP}
  {get_pins csr_u0/dual_reg/CP}
  {get_pins ctrl_u0/gen_lane_0_sync_u/ready_sync_reg/E}
  {get_pins clkgen_u0/genblk1_0_gen_ch_0_cg_wrap_u0_icg_u0/QN}
  {get_pins no_such_inst/Q}
  {get_cells -regexp {clkgen_u0/genblk1_0_gen.*}}
}

puts "default sta_sdc_name_folding: $sta_sdc_name_folding"

puts "######## read_sdc, sta_sdc_name_folding 0 ########"
set sta_continue_on_error 1
set sta_sdc_name_folding 0
read_sdc sdc_name_folding.sdc
show_clocks

puts "######## read_sdc, sta_sdc_name_folding 1 ########"
# Relinking clears the constraints and invalidates the folded name index.
read_verilog sdc_name_folding.v
link_design sdc_name_folding
set sta_sdc_name_folding 1
read_sdc sdc_name_folding.sdc
show_clocks
report_clock_properties

# Queries already resolved while reading the SDC are not reported again.
puts "######## get_*, sta_sdc_name_folding 0 ########"
set sta_sdc_name_folding 0
foreach cmd $rescued {
  show $cmd
}

puts "######## get_*, sta_sdc_name_folding 1 ########"
set sta_sdc_name_folding 1
foreach cmd $rescued {
  show $cmd
}
puts "######## unresolved ########"
foreach cmd $unresolved {
  show $cmd
}
puts "######## commands outside SDC do not fold pin arguments ########"
if { [catch {report_arrival ctrl_u0/gen_lane_0_sync_u/ready_sync_reg/Q} msg] } {
  puts $msg
}
puts "######## exact names win ########"
show {get_pins amb_0_x/Z}
show {get_pins {amb[0].x/Z}}

puts "######## netlist edits rebuild the index ########"
show {get_cells gen2_0_extra}
make_instance {gen2[0].extra} sdc_name_folding/BUFX
show {get_cells gen2_0_extra}
delete_instance {gen2[0].extra}
show {get_cells -quiet gen2_0_extra}
make_instance {gen2[1].extra} sdc_name_folding/BUFX
show {get_cells gen2_1_extra}
