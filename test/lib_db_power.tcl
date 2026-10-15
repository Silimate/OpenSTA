# report_power from a .libdb cache matches the Liberty source.

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
  # Two reports that failed alike or read no power would match, so the source must report power
  if { ![regexp {\nTotal +\S+ +\S+ +\S+ +(\S+)} [stadb_contents $cold_rpt] -> total]
       || $total == 0 } {
    puts "lib_db power $tag: no power from Liberty source"
  }
  stadb_check_files "lib_db power $tag" $cold_rpt $warm_rpt
}

# related_pg_pin and when conditions on internal power.
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

# Memory macro power gated by bus bits; asap7 rails come from voltage_map, not
# the gf180 operating conditions.
lib_db_power_check bus {gf180mcu_sram.lib.gz asap7_small.lib.gz} {
read_verilog get_is_memory.v
link_design get_is_memory
create_clock -name clk -period 10 CLK
set_operating_conditions ff_1p98v_125C
set_power_activity -input -activity 0.1 -duty 0.5
}

# A flop modeled only in its test_cell, a cell's named scaling_factors, scaled
# cell and retain slews, and a wire load, under operating conditions set by name
# that supply the rail voltage.
lib_db_power_check lib_groups lib_db_power.lib {
read_verilog lib_db_power.v
link_design lib_db_power
create_clock -name clk -period 10 ck
set_operating_conditions hot
set_wire_load_model -name wl
set_power_activity -input -activity 1.0 -duty 0.5
}
