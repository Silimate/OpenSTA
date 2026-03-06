# sta_no_inv_power_calc variable should ignore inverter power

# sta_no_inv_power_calc is off
read_liberty asap7_invbuf.lib.gz
read_verilog power_calc_no_inv.v
link_design power_calc_no_inv
create_clock -name clk -period 1
set_input_delay -clock clk 0 [all_inputs -no_clocks]
set_output_delay -clock clk 0 [all_outputs]
set_load 10 [all_outputs]
report_power -format json -instances {I1 I2 I3}

# sta_no_inv_power_calc is on
set sta_no_inv_power_calc 1
read_verilog power_calc_no_inv.v
link_design power_calc_no_inv
create_clock -name clk -period 1
set_input_delay -clock clk 0 [all_inputs -no_clocks]
set_output_delay -clock clk 0 [all_outputs]
report_power -format json -instances {I1 I2 I3}
