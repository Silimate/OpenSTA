# write_timing_model -internal_paths -min_filename models used in place of
# blocks (one and two levels of hierarchy) match flat timing.
# Each step runs in a child sta like a hierarchical flow would.
source helpers.tcl

proc run_child { tag body } {
  set script [make_result_file "write_timing_model_hier.$tag.tcl"]
  set stream [open $script "w"]
  puts $stream $body
  puts $stream "exit"
  close $stream
  catch { exec [info nameofexecutable] -no_init -no_splash -exit $script 2>@1 } output
  return $output
}

proc model_file { module suffix } {
  return [make_result_file "write_timing_model_hier_$module$suffix.lib"]
}

proc read_models_cmds { models } {
  set cmds "read_liberty write_timing_model_hier.lib\n"
  foreach model $models {
    append cmds "read_liberty -max [model_file $model {}]\n"
    append cmds "read_liberty -min [model_file $model _min]\n"
  }
  return $cmds
}

proc propagated_cmd { propagated } {
  return [expr { $propagated ? "set_propagated_clock \[all_clocks\]" : "" }]
}

# Block clocks only need the clock ports; the period does not matter.
proc write_model { module models verilog_files propagated } {
  set body [read_models_cmds $models]
  foreach file $verilog_files {
    append body "read_verilog $file\n"
  }
  append body "link_design $module
create_clock -name clka -period 10 \[get_ports clka\]
create_clock -name clkb -period 10 \[get_ports clkb\]
[propagated_cmd $propagated]
write_timing_model -scalar -internal_paths -min_filename [model_file $module _min] [model_file $module {}]
"
  puts -nonewline [run_child "$module$propagated" $body]
}

# Endpoint slacks as a dict of {check group endpoint} -> slack.
proc top_slacks { tag models verilog_files propagated } {
  set body [read_models_cmds $models]
  foreach file $verilog_files {
    append body "read_verilog $file\n"
  }
  append body "link_design top
create_clock -name clk1 -period 4 \[get_ports ck1\]
create_clock -name clk2 -period 3 -waveform {0.5 2} \[get_ports ck2\]
set_clock_uncertainty -setup 0.1 \[all_clocks\]
set_clock_uncertainty -hold 0.03 \[all_clocks\]
set_input_delay 0.5 -clock clk1 \[get_ports in0\]
set_output_delay 0.5 -clock clk2 \[get_ports {out0 out1 out2}\]
[propagated_cmd $propagated]
foreach {path_delay check} {max setup min hold} {
  foreach path_end \[find_timing_paths -path_delay \$path_delay -group_path_count 1000 -endpoint_path_count 1\] {
    puts \"slack \$check \[get_property \$path_end path_group\] \[get_full_name \[get_property \$path_end endpoint\]\] \[format %.3f \[get_property \$path_end slack\]\]\"
  }
}
"
  set slacks [dict create]
  foreach line [split [run_child "$tag$propagated" $body] "\n"] {
    if { [string match "slack *" $line] } {
      set fields [split $line " "]
      dict set slacks [lrange $fields 1 3] [lindex $fields 4]
    } elseif { $line != "" } {
      puts $line
    }
  }
  return $slacks
}

# The worst slack of each check/path group must match and every model
# endpoint slack must be the slack of a flat path.
proc compare_slacks { title flat hier } {
  puts "$title"
  set groups {}
  dict for {key slack} $flat {
    lappend groups [lrange $key 0 1]
  }
  foreach group [lsort -unique $groups] {
    lassign $group check path_group
    set flat_wns ""
    set hier_wns ""
    set flat_count 0
    set hier_count 0
    dict for {key slack} $flat {
      if { [lrange $key 0 1] == $group } {
        if { $flat_wns == "" || $slack < $flat_wns } { set flat_wns $slack }
        incr flat_count
      }
    }
    dict for {key slack} $hier {
      if { [lrange $key 0 1] == $group } {
        if { $hier_wns == "" || $slack < $hier_wns } { set hier_wns $slack }
        incr hier_count
      }
    }
    set status [expr { $flat_wns == $hier_wns ? "match" : "MISMATCH" }]
    puts [format "  %-5s %-4s flat %7s (%2d endpoints) model %7s (%2d endpoints) %s" \
            $check $path_group $flat_wns $flat_count $hier_wns $hier_count $status]
  }
  set flat_slacks [dict values $flat]
  dict for {key slack} $hier {
    if { [lsearch -exact $flat_slacks $slack] == -1 } {
      puts "  MISMATCH $key slack $slack is not a flat path slack"
    }
    if { [dict exists $flat $key] && [dict get $flat $key] != $slack } {
      puts "  MISMATCH $key flat [dict get $flat $key] model $slack"
    }
  }
}

set blk_v write_timing_model_hier_blk.v
set mid_v write_timing_model_hier_mid.v
set top_v write_timing_model_hier_top.v
foreach propagated {0 1} {
  puts "propagated clocks $propagated"
  set flat [top_slacks flat {} [list $blk_v $mid_v $top_v] $propagated]
  write_model blk {} [list $blk_v] $propagated
  set blk_model [top_slacks blk_model blk [list $mid_v $top_v] $propagated]
  write_model mid blk [list $mid_v] $propagated
  set mid_model [top_slacks mid_model {blk mid} [list $top_v] $propagated]
  compare_slacks "blk model" $flat $blk_model
  compare_slacks "blk and mid models" $flat $mid_model
  if { !$propagated } {
    puts "blk model endpoints"
    foreach key [lsort [dict keys $blk_model]] {
      puts "  $key [dict get $blk_model $key]"
    }
    report_file [model_file blk {}]
  }
}
