read_liberty asap7_small.lib.gz
read_liberty internal_paths_cell.ok
read_verilog internal_paths_top.v
link_design internal_paths_top

create_clock -name clk -period 500 {clk}
set_input_delay -clock clk 0 {in}

group_path -name custom -to {t2 t3}
group_path -name long -to {t4 t5}

report_checks -group_path_count 2 -sort_by_slack
