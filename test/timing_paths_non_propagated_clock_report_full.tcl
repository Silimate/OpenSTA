read_liberty asap7_small.lib.gz
read_liberty timing_paths_non_propagated_clock.ok
read_verilog timing_paths_propagated_clock_report.v

link_design top
create_clock -name clk -period 500 {clk}
set_propagated_clock {clk}
set_clock_uncertainty 0.1 {clk}
set_clock_latency 1.0 {clk}
set_input_delay -clock clk 0 {in}

report_checks -format full_clock_expanded
