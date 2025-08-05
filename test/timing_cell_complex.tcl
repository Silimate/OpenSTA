# timing cell complex example
read_liberty nangate45_slow.lib.gz
read_liberty timing_cell_dff.ok
read_liberty timing_cell_comb.ok
read_verilog timing_cell_complex.v
link_design timing_cell_complex
create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}
report_checks
