read_liberty nangate45_slow.lib.gz
read_verilog internal_paths_cell.v
link_design internal_paths_cell

create_clock -name clk -period 10 {clk}
set_input_delay -clock clk 0 {in}

group_path -name custom -to {r3 r4}
group_path -name long -to {r5 r6}

write_timing_model -paths internal_paths_cell.lib -internal_path_count 3
