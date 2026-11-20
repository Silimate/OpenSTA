# Latch arrays modeled with a statetable and no latch group are timed as latches
# without -infer_latches: each word's enable gets its own latch, and the read
# selects reach Q combinationally.
source stadb_helpers.tcl

read_liberty liberty_statetable_latch.lib
report_lib_cell statetable_latch/LATCH_ARRAY
# A latch can store more than one node, such as its complement.
report_lib_cell statetable_latch/COMPLEMENT_LATCH
# Statetable registers stay registers: edge rows, master/slave latches written with
# level rows only, and an LSSD register with separate master and slave clocks.
report_lib_cell statetable_latch/EDGE_FLOP
report_lib_cell statetable_latch/MASTER_SLAVE_FLOP
report_lib_cell statetable_latch/LSSD_REGISTER
# A latch group read through a select keeps its D -> Q arc.
report_lib_cell statetable_latch/GATED_LATCH

set design {read_verilog liberty_statetable_latch.v
link_design top
create_clock -name clk0 -period 1000 -waveform {0 500} [get_ports cp0]
create_clock -name clk1 -period 1000 -waveform {500 1000} [get_ports cp1]
create_clock -name vclk -period 1000
set_input_delay 300 -clock vclk [get_ports {d d0 d1 s0 s1}]
set_output_delay 0 -clock vclk [get_ports q*]}

# The shared data pin has a latch check per word; d0 arrives while word 0 is
# open, so it reaches q_sep through word 0; s0 reaches q combinationally.
set reports {report_checks -to [get_pins u1/D] -group_path_count 2 -format end
report_checks -through [get_pins u2/D0] -to [get_ports q_sep]
report_checks -from [get_ports s0] -to [get_ports q] -format end}

# A data arc with a when goes with the enable that when holds open (u3), whose
# arc has the same when (u4), or that it does not hold closed (u6). d arrives
# while u5 and word 0 of the others are closed and their word 1 is open: it
# waits at u5, where s0 still reaches the output, and gets through the others
# only while word 1 is read.
append reports {
report_edges -from u3/D -to u3/Q
report_edges -from u4/D -to u4/Q
report_edges -from u6/D -to u6/Q
report_checks -through [get_pins u5/D] -to [get_ports q_gated]
report_checks -from [get_ports s0] -to [get_ports q_gated] -format end
set_case_analysis 1 [get_ports s0]
set_case_analysis 0 [get_ports s1]
report_checks -through [get_pins {u3/D u4/D u6/D}]\
  -to [get_ports {q_when q_sel q_gated2}]
set_case_analysis 0 [get_ports s0]
set_case_analysis 1 [get_ports s1]
report_checks -through [get_pins u3/D] -to [get_ports q_when]
report_checks -through [get_pins {u4/D u6/D}] -to [get_ports {q_sel q_gated2}]\
  -group_path_count 2 -format end
set_case_analysis 1 [get_ports s0]
report_checks -through [get_pins {u3/D u4/D u6/D}]\
  -to [get_ports {q_when q_sel q_gated2}] -group_path_count 3 -format end}

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
