# A shared D pin can write either word. Give the both-open case a distinct delay.
source helpers.tcl

proc output_arc {pin type sense when delay} {
  set condition ""
  if {$when ne ""} { set condition [format {when : "%s";} $when] }
  return [format {timing () {
    related_pin : "%s"; timing_type : %s; timing_sense : %s; %s
    cell_rise (scalar) { values ("%s"); } cell_fall (scalar) { values ("%s"); }
    rise_transition (scalar) { values ("2"); } fall_transition (scalar) { values ("2"); }
  }} $pin $type $sense $condition $delay $delay]
}

set f [open liberty_latch_write_condition.lib.in]
set template [read $f]
close $f
set storage(table) {
  statetable ("CP0 D", "BIT") { table : "H L : - : L, H H : - : H, L - : - : N"; }
  pin (W0) { direction : internal; internal_node : BIT; input_map : "CP0 D"; }
  pin (W1) { direction : internal; internal_node : BIT; input_map : "CP1 D"; }
}
set storage(latch) {
  latch (W0, W0N) { enable : "CP0"; data_in : "D"; }
  latch (W1, W1N) { enable : "CP1"; data_in : "D"; }
}
set read_mux {"!((W0 S0)|(W1 S1))"}
set data_arcs ""
foreach {when delay} {
  CP0&!CP1&S0&!S1 19.8   CP0&!CP1&S0&S1 19.8   CP0&CP1&S0&!S1 19.8
  CP0&CP1&S0&S1 100      CP0&CP1&!S0&S1 19.8   !CP0&CP1&!S0&S1 19.8
  !CP0&CP1&S0&S1 19.8
} {
  append data_arcs [output_arc D combinational negative_unate $when $delay]
}

foreach {instance form first second} {
  table_01 table CP0 CP1   table_10 table CP1 CP0
  latch_01 latch CP0 CP1   latch_10 latch CP1 CP0
  table_default table CP0 CP1
} {
  set timing $data_arcs
  if {$instance eq "table_default"} {
    set timing [output_arc D combinational negative_unate "" 100]
    append timing [output_arc D combinational negative_unate "CP0&CP1&S0&S1" 180]
  }
  foreach pin [list $first $second] {
    foreach when {D !D} {
      append timing [output_arc $pin rising_edge non_unate $when 12.2]
    }
  }
  append timing [output_arc S0 combinational negative_unate "" 5]
  set bundle_function ""
  set pin_function ""
  if {$form eq "table"} {
    set bundle_function "state_function : $read_mux;"
  } else {
    set pin_function "function : $read_mux;"
  }
  set lib [make_result_file "liberty_latch_write_condition.$instance.lib"]
  set f [open $lib w]
  puts $f [string map [list @cell@ [string toupper $instance] @storage@ $storage($form) \
    @bundle_function@ $bundle_function @pin_function@ $pin_function @timing@ $timing] $template]
  close $f
  read_liberty $lib
}

read_verilog liberty_latch_write_condition.v
link_design top
create_clock -name c0 -period 1000 -waveform {0 650} [get_ports cp0]
create_clock -name c1 -period 1000 -waveform {400 900} [get_ports cp1]
create_clock -name ref -period 1000
set_output_delay 0 -clock ref [get_ports q*]

proc check_paths {through output clocks expected_ps} {
  set paths [find_timing_paths -through [get_pins $through] -to [get_ports $output] \
               -group_path_count 16 -endpoint_path_count 16]
  set observed {}
  foreach path $paths {
    set clock [get_property $path startpoint_clock]
    set arrival [expr {[$path data_arrival_time] * 1e12}]
    if {![dict exists $observed $clock] || $arrival > [dict get $observed $clock]} {
      dict set observed $clock $arrival
    }
  }
  if {[lsort [dict keys $observed]] ne $clocks} {
    error "$through to $output: expected clocks {$clocks}, got $observed"
  }
  dict for {clock arrival} $observed {
    if {abs($arrival - $expected_ps) > 0.01} {
      error "$through to $output on $clock: expected $expected_ps ps, got $arrival ps"
    }
  }
}

set_case_analysis 1 [get_ports {s0 s1}]
foreach {at clocks expected default_expected} {
  300 c0      319.8 400
  700 c1      719.8 800
  450 {c0 c1} 550.0 630
  950 {}      {}    {}
} {
  set_input_delay $at -clock ref [get_ports d]
  foreach instance {table_01 table_10 latch_01 latch_10} {
    check_paths $instance/D q_$instance $clocks $expected
  }
  check_paths default_arc/D q_default $clocks $default_expected
}
puts "Both latch declarations and both clock group orders give the expected write paths."

# With both clocks open, only the selected word can pass the write to Q.
set_input_delay 450 -clock ref [get_ports d]
foreach {s0 s1 clock} {1 0 c0  0 1 c1} {
  set_case_analysis $s0 [get_ports s0]
  set_case_analysis $s1 [get_ports s1]
  foreach instance {table_01 table_10 latch_01 latch_10} {
    check_paths $instance/D q_$instance $clock 469.8
  }
}
puts "A single read select keeps the selected word's write path."

unset_case_analysis [get_ports s0]
set_case_analysis 0 [get_ports s1]
set_input_delay 950 -clock ref [get_ports s0]
foreach instance {table_01 table_10 latch_01 latch_10} {
  check_paths $instance/S0 q_$instance ref 955
}
puts "Read select reaches Q while both words are closed."
