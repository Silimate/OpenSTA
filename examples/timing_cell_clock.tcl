# timing cell clock example
read_liberty nangate45_slow.lib.gz
read_verilog timing_cell_clock.v
link_design timing_cell_clock
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
report_checks -format json
write_timing_model timing_cell_clock.lib