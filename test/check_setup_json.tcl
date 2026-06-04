# check_setup -json
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

# Only constrain clk1; leave clk2/clk3, inputs, and outputs unconstrained
create_clock -name clk1 -period 500 clk1

set json_file [make_result_file "check_setup_json.json"]

# Dump JSON, should warn that loops aren't supported.
check_setup -unconstrained_endpoints -multiple_clock -no_clock \
            -no_input_delay -generated_clocks \
            -loop \
            -json $json_file

report_file $json_file
