# Latch arrays modeled with a statetable and no latch group are timed as latches
# without -infer_latches: each word's enable gets its own latch, and the read
# selects reach Q combinationally.
source stadb_helpers.tcl

read_liberty liberty_statetable_latch.lib
report_lib_cell statetable_latch/LATCH_ARRAY
# Edge-triggered statetables stay registers, including one written as master/slave
# latches with level rows only.
report_lib_cell statetable_latch/EDGE_FLOP
report_lib_cell statetable_latch/MASTER_SLAVE_FLOP

set design {read_verilog liberty_statetable_latch.v
link_design top
create_clock -name clk0 -period 1000 -waveform {0 500} [get_ports cp0]
create_clock -name clk1 -period 1000 -waveform {500 1000} [get_ports cp1]
create_clock -name vclk -period 1000
set_input_delay 300 -clock vclk [get_ports {d d0 d1 s0 s1}]
set_output_delay 0 -clock vclk [get_ports {q q_sep}]}

# The shared data pin has a latch check per word; d0 arrives while word 0 is
# open, so it reaches q_sep through word 0; s0 reaches q combinationally.
set reports {report_checks -to [get_pins u1/D] -group_path_count 2 -format end
report_checks -through [get_pins u2/D0] -to [get_ports q_sep]
report_checks -from [get_ports s0] -to [get_ports q] -format end}

eval $design
eval $reports

# Latch enables rebuilt from a .libdb match the ones inferred from the .lib.
set lib_file [make_result_file "liberty_statetable_latch.libdb"]
stadb_run "read_liberty liberty_statetable_latch.lib
write_lib_db \[get_libs statetable_latch\] $lib_file" libdb_w
set cold [stadb_run "read_liberty liberty_statetable_latch.lib
puts REPORTS
$design
$reports" cold]
set warm [stadb_run "read_lib_db $lib_file
puts REPORTS
$design
$reports" warm]
stadb_check "liberty_statetable_latch lib_db" \
  [string range $cold [string first REPORTS $cold] end] \
  [string range $warm [string first REPORTS $warm] end]
