read_liberty asap7_small.lib.gz
read_liberty internal_paths_cell.ok
read_liberty internal_paths_cell_another.lib
read_liberty internal_paths_cell_hidden.lib
read_verilog internal_paths_top_complex.v
link_design internal_paths_top_complex

create_clock -name clk -period 500 {clk}
set_input_delay -clock clk 0 {in}

group_path -name custom -to {t1 t2}
group_path -name long -from {in}

report_checks -group_path_count 4 -path_delay max_rise -format full_clock_expanded -sort_by_slack
