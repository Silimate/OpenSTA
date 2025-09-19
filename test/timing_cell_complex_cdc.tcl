# timing cell complex example
read_liberty asap7_small.lib.gz
read_liberty timing_cell_cdc.ok
read_verilog timing_cell_cdc.v
link_design timing_cell_cdc
create_clock -name clk -period 500 {clk_a clk_b}
set_input_delay -clock clk 0 {in}
report_checks
