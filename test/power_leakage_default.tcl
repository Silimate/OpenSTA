# Leakage states no when condition covers take the default leakage_power group
# unless the cell has cell_leakage_power.
read_liberty power_leakage_default.lib
read_verilog power_leakage_default.v
link_design power_leakage_default
create_clock -name clk -period 10
set_power_activity -input_ports a -activity 0.1 -duty 0.25
# u1: 2e-6 * 0.25 + 1e-6 * 0.75 = 1.25e-6
# u2: 2e-6 * 0.25 + 3e-6 * 0.75 = 2.75e-6
report_power -format json -instances {u1 u2}
