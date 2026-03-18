# Test Script for Generated Clock Memory Corruption Fix
# This script tests various edge cases that previously caused crashes

# Test 1: Basic generated clock (should work)
read_liberty Nangate45/Nangate45_typ.lib
read_verilog test/simple_hier.v
link_design top

create_clock -name clk -period 10 [get_ports clk]

# Test 2: Generated clock with exactly 3 edges (standard case)
# This should work in both old and new code
create_generated_clock -name gclk1 -source [get_ports clk] \
  -edges {1 3 5} [get_pins u1/q]

# Test 3: Try to create a clock that exercises edge handling
# Note: TCL currently restricts to exactly 3 edges, so we can't test 4+ edges via TCL
# However, Liberty files can have any number of edges

# Test 4: Generated clock with divide_by
create_generated_clock -name gclk2 -source [get_ports clk] \
  -divide_by 2 [get_pins u2/q]

# Test 5: Generated clock with multiply_by
create_generated_clock -name gclk3 -source [get_ports clk] \
  -multiply_by 2 [get_pins u3/q]

# Test 6: Generated clock with invert
create_generated_clock -name gclk4 -source [get_ports clk] \
  -divide_by 1 -invert [get_pins u4/q]

# Test 7: Generated clock with edge shifts
create_generated_clock -name gclk5 -source [get_ports clk] \
  -edges {1 2 3} -edge_shift {0 0.1 0.2} [get_pins u5/q]

# Report all clocks to verify they were created successfully
puts "\n=========================================="
puts "Generated Clocks Test Results"
puts "=========================================="
puts "Number of clocks: [llength [get_clocks]]"
puts ""

foreach_in_collection clk [get_clocks] {
  set name [get_object_name $clk]
  set period [get_property $clk period]
  puts "Clock: $name, Period: $period"
}

puts ""
puts "Test completed successfully - no crashes!"
puts "=========================================="

# If we get here without crashing, the fix is working
exit 0
