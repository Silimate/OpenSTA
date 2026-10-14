# Power from a .libdb cache matches the Liberty source: internal and leakage
# power, when conditions, related_pg_pin, voltage_map and bus pins.

source stadb_helpers.tcl

# Report power from the Liberty source and from per-file .libdb caches, and compare.
proc lib_db_power_check { tag lib_files design } {
  set cold_reads ""
  set warm_reads ""
  set i 0
  foreach lib_file $lib_files {
    set db_file [make_result_file "lib_db_power.$tag.$i.libdb"]
    stadb_run "read_liberty $lib_file
write_lib_db \[get_libs *\] $db_file" libdb_pw_${tag}_$i
    append cold_reads "read_liberty $lib_file\n"
    append warm_reads "read_lib_db $db_file\n"
    incr i
  }
  set cold_rpt [make_result_file "lib_db_power.$tag.cold.rpt"]
  set warm_rpt [make_result_file "lib_db_power.$tag.warm.rpt"]
  file delete -force $cold_rpt $warm_rpt
  stadb_run "$cold_reads$design
report_power -digits 6 > $cold_rpt
report_power -digits 6 -instances \[get_cells *\] >> $cold_rpt" libdb_pc_$tag
  stadb_run "$warm_reads$design
report_power -digits 6 > $warm_rpt
report_power -digits 6 -instances \[get_cells *\] >> $warm_rpt" libdb_pr_$tag
  # Two reports that failed alike would match, so the source run must have reported power
  if { ![string match "*\nTotal *" [stadb_contents $cold_rpt]] } {
    puts "lib_db power $tag: no power report from Liberty source"
  }
  stadb_check_files "lib_db power $tag" $cold_rpt $warm_rpt
}

# voltage_map, pg_pin and conditional leakage, with SAIF activity.
lib_db_power_check sky130hd ../examples/sky130hd_tt.lib.gz {
read_verilog ../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../examples/gcd_sky130hd.sdc
read_saif -scope gcd_tb/gcd1 ../examples/gcd_sky130hd.saif.gz
}

# related_pg_pin on internal and leakage power.
lib_db_power_check asap7 {asap7_seq.lib.gz asap7_invbuf.lib.gz asap7_simple.lib.gz} {
read_verilog ../examples/reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_power_activity -input -activity 0.1 -duty 0.5
}

# cell_leakage_power alongside when-conditioned leakage_power groups.
lib_db_power_check leakage_default power_leakage_default.lib {
read_verilog power_leakage_default.v
link_design power_leakage_default
create_clock -name clk -period 10
set_power_activity -input_ports a -activity 0.1 -duty 0.25
}

# Internal power on bus pins of a memory macro.
lib_db_power_check bus {gf180mcu_sram.lib.gz asap7_small.lib.gz} {
read_verilog get_is_memory.v
link_design get_is_memory
create_clock -name clk -period 10 [get_ports -quiet *clk*]
set_power_activity -input -activity 0.1 -duty 0.5
}
