# Liberty min_period round trip: register data pins, path groups built from
# them, and min period checks.

source stadb_helpers.tcl

# Data pins skip ports with a min_period, so a port must only have one after
# restore if its library sets it.
set reg_file [make_result_file "stadb_reg_data_pins.stadb"]
set reg_build {read_liberty ../examples/nangate45_slow.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 out}
set reg_dump {puts "data_pins [join [stadb_names [all_registers -data_pins]] ,]"
group_path -name in2reg -from [all_inputs] -to [all_registers -data_pins]
group_path -name reg2reg -from [all_registers -clock_pins] \
  -to [all_registers -data_pins]
report_checks -group_path_count 10 -digits 6
report_check_types -min_period -verbose -digits 6}

stadb_run "$reg_build
sta::find_timing -full_update
write_sta_db $reg_file" r_w
set cold_reg [stadb_run "$reg_build
$reg_dump" r_c]
set warm_reg [stadb_run "read_sta_db $reg_file
$reg_dump" r_r]
puts "reg2reg group reported: [regexp {Path Group: reg2reg} $cold_reg]"
# The data pins line first, so a mismatch names it instead of report lines.
stadb_check "liberty register data pins" [lindex [split $cold_reg "\n"] 0] \
  [lindex [split $warm_reg "\n"] 0]
stadb_check "liberty register path groups" $cold_reg $warm_reg

# The SRAM CLK port sets min_period, which must survive the restore.
set sram_file [make_result_file "stadb_reg_data_pins.sram.stadb"]
set sram_build {read_liberty gf180mcu_sram.lib.gz
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog inst_props.v
link_design inst_props
create_clock -name clk -period 10 CLK}
set sram_dump {report_check_types -min_period -verbose -digits 6}

stadb_run "$sram_build
sta::find_timing -full_update
write_sta_db $sram_file" r_sw
set cold_sram [stadb_run "$sram_build
$sram_dump" r_sc]
set warm_sram [stadb_run "read_sta_db $sram_file
$sram_dump" r_sr]
puts "sram min period checked: [regexp {min period} $cold_sram]"
stadb_check "liberty sram min period" $cold_sram $warm_sram
