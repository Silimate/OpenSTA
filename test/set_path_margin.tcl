# set_path_margin: per-path slack adjustment on the capture clock.

read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc setup_at { pin } {
  report_checks -to $pin -path_delay max -digits 4 -fields {} -group_path_count 1
}
proc hold_at { pin } {
  report_checks -to $pin -path_delay min -digits 4 -fields {} -group_path_count 1
}

set_path_margin -setup 0.50 -comment {tighten setup time} -to [get_pins r3/D]
setup_at [get_pins r3/D]

set_path_margin -hold 0.50 -comment {tighten hold time} -to [get_pins r3/D]
hold_at [get_pins r3/D]

set_path_margin -setup -67 -comment {loosen setup time} -to [get_pins r3/D]
setup_at [get_pins r3/D]

set_path_margin -hold -0.50 -comment {loosen hold time} -to [get_pins r3/D]
hold_at [get_pins r3/D]
