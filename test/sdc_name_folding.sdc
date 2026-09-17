# Constraints written against a synthesis netlist that flattened generate
# scopes and ungrouped wrappers, named 1-bit registers without a bit index,
# and used a library whose register clock pin is CP.
create_clock -name CLK -period 10 [get_ports clk]

# Generated clocks on a clock gate output. An error in any iteration
# aborts the whole loop, dropping every clock it would have created.
foreach ch {0} {
  create_generated_clock -name GCLK_CH${ch} -source [get_ports clk] \
    -divide_by 1 \
    [get_pins clkgen_u0/genblk1_${ch}_gen_ch_0_cg_wrap_u0_icg_u0/Q]
}

# Implicit pin argument on a register named without its bit index.
foreach ch {0} {
  create_generated_clock -name SYNC_CLK_CH${ch} \
    -source clkgen_u0/genblk1_0_gen_ch_0_cg_wrap_u0_icg_u0/Q \
    -divide_by 2 \
    ctrl_u0/gen_lane_${ch}_sync_u/ready_meta_0_reg/Q
}

# Output port globs written for bus bit names.
foreach bit {0 1} {
  set_output_delay -clock CLK 1 [get_ports status*${bit}*]
}

# Multi-dimensional register bits named the synthesis tool's way.
set_multicycle_path 2 -from {arrays_u0/mem_reg[1][2]} \
  -through [get_pins {arrays_u0/tap_reg[0][3][5]/D}]

# Register clock pins under another library's pin name.
set_false_path -from [get_pins csr_u0/*mode_sel_reg*/CP]

set_clock_groups -asynchronous -group [get_clocks GCLK_CH*] \
  -group [get_clocks SYNC_CLK_CH*]
