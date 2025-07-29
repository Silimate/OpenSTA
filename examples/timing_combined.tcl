read_liberty nangate45_slow.lib.gz
read_verilog timing_cell.v
read_verilog timing_top.v
link_design timing_top
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
report_checks