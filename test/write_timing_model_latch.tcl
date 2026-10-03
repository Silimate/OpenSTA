# write_timing_model -internal_paths models of blocks with latches and
# generated clocks match flat timing at top level clock periods shorter
# and longer than the period used to make the models.
# Each step runs in a child sta like a hierarchical flow would.
source helpers.tcl

proc run_child { tag body } {
  set script [make_result_file "write_timing_model_latch.$tag.tcl"]
  set stream [open $script "w"]
  puts $stream $body
  puts $stream "exit"
  close $stream
  catch { exec [info nameofexecutable] -no_init -no_splash -exit $script 2>@1 } output
  return $output
}

proc model_file { module } {
  return [make_result_file "write_timing_model_latch_$module.lib"]
}

proc read_cmds { models verilog_files } {
  set cmds "read_liberty -infer_latches write_timing_model_hier.lib\n"
  foreach model $models {
    append cmds "read_liberty [model_file $model]\n"
  }
  foreach file $verilog_files {
    append cmds "read_verilog $file\n"
  }
  return $cmds
}

proc propagated_cmd { propagated } {
  return [expr { $propagated ? "set_propagated_clock \[all_clocks\]" : "" }]
}

proc write_model { module models verilog_files propagated sdc {tag ""} } {
  set body [read_cmds $models $verilog_files]
  append body "link_design $module
create_clock -name clk -period 10 \[get_ports clk\]
$sdc
[propagated_cmd $propagated]
write_timing_model -scalar -internal_paths [model_file $module]
"
  set output [run_child "$module$propagated$tag" $body]
  if { $output != "" } {
    puts $output
  }
}

# Endpoint slacks as a dict of {check group endpoint} -> slack.
proc top_slacks { tag models verilog_files propagated period flat } {
  set body [read_cmds $models $verilog_files]
  append body "link_design latch_top
create_clock -name ck -period $period \[get_ports ck\]
"
  if { $flat } {
    append body "$::flat_gen_clks\n"
  }
  append body "set_input_delay 0.5 -clock ck \[get_ports {in0 in1}\]
set_output_delay 0.5 -clock ck \[get_ports {out0 out1 out2 out3}\]
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

proc compare_slacks { title flat hier } {
  set matched 0
  dict for {key slack} $flat {
    if { ![dict exists $hier $key] } {
      puts "  MISMATCH $key missing from model"
    } elseif { [dict get $hier $key] != $slack } {
      puts "  MISMATCH $key flat $slack model [dict get $hier $key]"
    } else {
      incr matched
    }
  }
  dict for {key slack} $hier {
    if { ![dict exists $flat $key] } {
      puts "  MISMATCH $key not a flat endpoint"
    }
  }
  puts "  $title: $matched of [dict size $flat] flat endpoints match"
}

set blk_v write_timing_model_latch_blk.v
set mid_v write_timing_model_latch_mid.v
set top_v write_timing_model_latch_top.v
set gen_clk_sdc "create_generated_clock -name gclk -source \[get_ports clk\] -divide_by 2 \[get_pins div/Q\]"
# Named like the generated clocks made from the model generated_clock groups.
set flat_gen_clks "create_generated_clock -name i0/div/Q -source \[get_ports ck\] -divide_by 2 \[get_pins i0/div/Q\]
create_generated_clock -name i1/k0/div/Q -source \[get_ports ck\] -divide_by 2 \[get_pins i1/k0/div/Q\]"
foreach propagated {0 1} {
  write_model latch_blk {} [list $blk_v] $propagated $gen_clk_sdc
  write_model latch_mid latch_blk [list $mid_v] $propagated ""
  foreach period {10 3 1.6} {
    puts "propagated clocks $propagated period $period"
    set flat [top_slacks flat {} [list $blk_v $mid_v $top_v] $propagated $period 1]
    set blk_model [top_slacks blk_model latch_blk [list $mid_v $top_v] \
                     $propagated $period 0]
    set mid_model [top_slacks mid_model {latch_blk latch_mid} [list $top_v] \
                     $propagated $period 0]
    compare_slacks "latch_blk model" $flat $blk_model
    compare_slacks "latch_blk and latch_mid models" $flat $mid_model
    if { $propagated && $period == 1.6 } {
      puts "latch_blk model endpoints"
      foreach key [lsort [dict keys $blk_model]] {
        puts "  $key [dict get $blk_model $key]"
      }
    }
  }
  if { !$propagated } {
    report_file [model_file latch_blk]
  }
}

puts "path delays and data checks inside the block"
write_model latch_blk {} [list $blk_v] 0 "$gen_clk_sdc
set_max_delay 2.0 -from \[get_pins r1/CK\] -to \[get_pins r2/D\]
set_data_check -from \[get_pins u9/A\] -to \[get_pins u10/A\] -setup 0.1" _path_delay

puts "two generated clocks on one pin"
set gen_clk_sdc "create_generated_clock -name gclk -source \[get_ports clk\] -divide_by 2 \[get_pins div/Q\]
create_generated_clock -name gclk4 -add -master_clock clk -source \[get_ports clk\] -divide_by 4 \[get_pins div/Q\]"
# Generated clocks that share a pin are named by their generated_clock group.
set flat_gen_clks ""
foreach inst {i0 i1/k0} {
  append flat_gen_clks "create_generated_clock -name $inst/gclk -source \[get_ports ck\] -divide_by 2 \[get_pins $inst/div/Q\]
create_generated_clock -name $inst/gclk4 -add -master_clock ck -source \[get_ports ck\] -divide_by 4 \[get_pins $inst/div/Q\]
"
}
write_model latch_blk {} [list $blk_v] 1 $gen_clk_sdc _add
write_model latch_mid latch_blk [list $mid_v] 1 "" _add
set flat [top_slacks flat_add {} [list $blk_v $mid_v $top_v] 1 3 1]
set blk_model [top_slacks blk_model_add latch_blk [list $mid_v $top_v] 1 3 0]
set mid_model [top_slacks mid_model_add {latch_blk latch_mid} [list $top_v] 1 3 0]
compare_slacks "latch_blk model" $flat $blk_model
compare_slacks "latch_blk and latch_mid models" $flat $mid_model
