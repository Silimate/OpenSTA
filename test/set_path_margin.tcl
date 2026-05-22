# set_path_margin: per-path slack adjustment on the capture clock.

read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc setup_at { args } {
  report_checks {*}$args -path_delay max -digits 4 -fields {} -group_path_count 1
}
proc hold_at { args } {
  report_checks {*}$args -path_delay min -digits 4 -fields {} -group_path_count 1
}

# Test -to and that -setup and -hold are properly applied.
set_path_margin -setup 0.50 -comment {tighten setup time} -to [get_pins r3/D]
setup_at -to [get_pins r3/D]
set_path_margin -hold 0.50 -comment {tighten hold time} -to [get_pins r3/D]
hold_at -to [get_pins r3/D]
set_path_margin -setup -67 -comment {loosen setup time} -to [get_pins r3/D]
setup_at -to [get_pins r3/D]
set_path_margin -hold -0.50 -comment {loosen hold time} -to [get_pins r3/D]
hold_at -to [get_pins r3/D]

# Test -from
reset_path -through [get_pins u1/Z]
set_path_margin -setup 2.0 -from [get_pins r1/CK]
# Should see path margin.
setup_at -from [get_pins r1/CK] -to [get_pins r3/D]
# Should not see path margin.
setup_at -from [get_pins r2/CK] -to [get_pins r3/D]

# Test -from and -to.
reset_path -to [get_pins r3/D]
set_path_margin -setup 5.0 -from [get_pins r1/CK] -to [get_pins r3/D]
# Should see path margin.
setup_at -from [get_pins r1/CK] -to [get_pins r3/D]
# Should not see path margin.
setup_at -from [get_pins r2/CK] -to [get_pins r3/D]

# Test -through.
reset_path -to [get_pins r3/D]
set_path_margin -setup 3.0 -through [get_pins u1/Z]
# Should not see path margin.
setup_at -from [get_pins r1/CK] -to [get_pins r3/D]
# Should see path margin.
setup_at -from [get_pins r2/CK] -to [get_pins r3/D]
