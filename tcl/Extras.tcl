# OpenSTA, Static Timing Analyzer
# Copyright (c) 2024-2026, Silimate, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
# 
# The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software.
# 
# Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# This notice may not be removed or altered from any source distribution.

################################################################
# Helpers for path end reporting
################################################################

namespace eval sta {

proc get_paths { args } {
  global sta_report_unconstrained_paths
  parse_report_path_options "get_paths" args "full" 0
  return [find_timing_paths_cmd "get_paths" args]
}

proc_redirect report_paths {
  report_path_ends {*}$args
}

define_cmd_args "report_echo" {message} \
  -help {The `report_echo` command prints a message using the report output path, so it is captured by `redirect` and `> filename`.} \
  -arg_help {
    message {The text to print.}
  }

proc_redirect report_echo {
  parse_key_args "report_echo" args \
    keys {} flags {}
  check_argc_eq1 "report_echo" $args

  set message [lindex $args 0]
  report_line "$message"
}

# Dump target PPA to JSON file
proc target_ppa_json { filepath } {
  set ppa_json [open "$filepath" "w"]

  # Retrieve max_logic_levels from global scope (set by user constraints)
  global max_logic_levels
  if { [info exists max_logic_levels] } {
    set max_logic_levels_json $max_logic_levels
  } else {
    set max_logic_levels_json "null"
  }

  # Dump target PPA to JSON file
  puts $ppa_json "{"
  puts $ppa_json "  \"max_area\": [sta::max_area],"
  puts $ppa_json "  \"max_dynamic_power\": [sta::max_dynamic_power],"
  puts $ppa_json "  \"max_leakage_power\": [sta::max_leakage_power],"
  puts $ppa_json "  \"max_logic_levels\": $max_logic_levels_json"
  puts $ppa_json "}"
  close $ppa_json
}

}

################################################################
# Miscellaneous commands
################################################################

sta::define_cmd_args "set_dont_use" {lib_cell_name_pattern} \
  -help {The `set_dont_use` command marks liberty cells whose names match `lib_cell_name_pattern` as dont-use. Matching uses `get_lib_cells -filter`.} \
  -arg_help {
    lib_cell_name_pattern {A liberty cell name glob pattern, as used in a `name=~` filter.}
  }

proc set_dont_use {lib_cell_name_pattern} {
  set targets [get_lib_cells -filter "name=~$lib_cell_name_pattern"]
  foreach_in_collection target $targets {
    $target set_dont_use
  }
}

sta::define_cmd_args "unset_dont_use" {lib_cell_name_pattern} \
  -help {The `unset_dont_use` command clears the dont-use flag on liberty cells whose names match `lib_cell_name_pattern`. Matching uses `get_lib_cells -filter`.} \
  -arg_help {
    lib_cell_name_pattern {A liberty cell name glob pattern, as used in a `name=~` filter.}
  }

proc unset_dont_use {lib_cell_name_pattern} {
  set targets [get_lib_cells -filter "name=~$lib_cell_name_pattern"]
  foreach_in_collection target $targets {
    $target unset_dont_use
  }
}

sta::define_cmd_args "get_flat_pins" {arg} \
  -help {The `get_flat_pins` command returns leaf (non-hierarchical) pins whose full names match `arg`. It is equivalent to `get_pins -hier -filter "is_hierarchical==false && full_name=~arg"`.} \
  -arg_help {
    arg {A pin full-name glob pattern.}
  }

proc get_flat_pins {arg} {
  return [get_pins -hier -filter "is_hierarchical==false && full_name=~$arg"]
}

sta::define_cmd_args "get_flat_cells" {arg} \
  -help {The `get_flat_cells` command returns leaf (non-hierarchical) instances whose full names match `arg`. It is equivalent to `get_cells -hier -filter "is_hierarchical==false && full_name=~arg"`.} \
  -arg_help {
    arg {An instance full-name glob pattern.}
  }

proc get_flat_cells {arg} {
  return [get_cells -hier -filter "is_hierarchical==false && full_name=~$arg"]
}

# Set dont_touch attribute (ignore/to be implemented)
interp alias {} set_dont_touch {} return -level 0
interp alias {} unset_dont_touch {} return -level 0

# Set dont_touch_network attribute (ignore/to be implemented)
interp alias {} set_dont_touch_network {} return -level 0
interp alias {} unset_dont_touch_network {} return -level 0

# Get object name
interp alias {} get_object_name {} get_full_name

# Query objects (ignore/to be implemented)
interp alias {} query_objects {} return -level 0

# remove/reset aliases for "unset" commands in sdc.tcl
interp alias {} remove_output_delay {} unset_output_delay
interp alias {} reset_output_delay {} unset_output_delay

interp alias {} remove_input_delay {} unset_input_delay
interp alias {} reset_input_delay {} unset_input_delay

interp alias {} remove_propagated_clock {} unset_propagated_clock
interp alias {} reset_propagated_clock {} unset_propagated_clock

interp alias {} remove_clock_groups {} unset_clock_groups
interp alias {} reset_clock_groups {} unset_clock_groups

interp alias {} remove_case_analysis {} unset_case_analysis
interp alias {} reset_case_analysis {} unset_case_analysis

interp alias {} remove_timing_derate {} unset_timing_derate
interp alias {} reset_timing_derate {} unset_timing_derate

interp alias {} remove_path_exceptions {} unset_path_exceptions
interp alias {} reset_path_exceptions {} unset_path_exceptions

interp alias {} remove_data_check {} unset_data_check
interp alias {} reset_data_check {} unset_data_check

interp alias {} remove_clock_transition {} unset_clock_transition
interp alias {} reset_clock_transition {} unset_clock_transition

interp alias {} remove_clock_uncertainty {} unset_clock_uncertainty
interp alias {} reset_clock_uncertainty {} unset_clock_uncertainty

interp alias {} remove_clock_latency {} unset_clock_latency
interp alias {} reset_clock_latency {} unset_clock_latency

interp alias {} remove_disable_timing {} unset_disable_timing
interp alias {} reset_disable_timing {} unset_disable_timing

interp alias {} remove_disable_timing_cell {} unset_disable_timing_cell
interp alias {} reset_disable_timing_cell {} unset_disable_timing_cell

interp alias {} remove_disable_timing_instance {} unset_disable_timing_instance
interp alias {} reset_disable_timing_instance {} unset_disable_timing_instance

# Get attribute
sta::define_cmd_args "get_attribute" {[-quiet] object property} \
  -help {The `get_attribute` command returns a property of an object. The object and property arguments may appear in either order. See `get_property` for the list of properties.}

proc get_attribute {args} {
  sta::parse_key_args "get_attribute" args keys {} flags {-quiet}
  set quiet [info exists flags(-quiet)]
  set arg1 [lindex $args 0]
  set arg2 [lindex $args 1]

  # Suppress unknown property warning
  if { $quiet } {
    suppress_msg 9000
  }
  if { [sta::is_object $arg1] } {
    if { [sta::is_collection $arg1] } {
      set arg1 [collection_at_index $arg1 0]
    }
    set result [get_property $arg1 $arg2]
  } elseif { [sta::is_object $arg2] } {
    if { [sta::is_collection $arg2] } {
      set arg2 [collection_at_index $arg2 0]
    }
    set result [get_property $arg2 $arg1]
  } else {
    if { $quiet } {
      unsuppress_msg 9000
    }
    error "get_attribute: invalid object $arg1 or $arg2"
  }
  # Re-enable warning after the call
  if { $quiet } {
    unsuppress_msg 9000
  }
  return $result
}

# Fanin/fanout commands all_fanin and all_fanout. get_fanin/get_fanout accept a
# superset of the sdc flags, but stop at hierarchy crossings unless -flat is
# given, while sdc defines the traversal over the flattened netlist.
interp alias {} all_fanin {} get_fanin -flat
interp alias {} all_fanout {} get_fanout -flat

################################################################
# Unsupported commands (for now)
################################################################

# Set clock jitter
proc set_clock_jitter { args } {
  puts "Warning: set_clock_jitter not supported"
}

# Get liberty timing arcs
proc get_lib_timing_arcs { args } {
  puts "Warning: get_lib_timing_arcs not supported, will return empty list"
  return [list]
}

# Suppress message (ignore/to be implemented)
proc suppress_message { args } {
  puts "Warning: suppress_message not supported, command ignored"
}

################################################################
# TCL extras
################################################################

# Add echo alias
interp alias {} echo {} puts

namespace eval sta {

# alias name definition
define_cmd_args "alias" {name definition} \
  -help {The `alias` command creates a Tcl interpreter alias. `name` becomes a command that expands to `definition`.} \
  -arg_help {
    name {The new command name.}
    definition {The command and arguments to invoke when `name` is called.}
  }
proc alias { args } {
  if { [llength $args] < 2 } {
    return
  }
  set name [lindex $args 0]
  set def [lrange $args 1 end]
  if { [llength $def] == 1 } {
    set def [lindex $def 0]
  }
  interp alias {} $name {} {*}$def
}

# redirect filename {script}
# Capture report/console output, not the script's Tcl return value.
define_cmd_args "redirect" \
  {[-append] [-tee] [-variable var] [-file filename] [filename] script} \
  -help {The `redirect` command captures report and console output of `script` to a file and/or a Tcl variable. It does not capture the script's Tcl return value. Use `-tee` to also print to the console. Use `-append` to append to an existing file.} \
  -arg_help {
    -append {Append to the output file instead of overwriting it.}
    -tee {Also write the captured output to the console.}
    -variable {`var`: Tcl variable name to store the captured output.}
    -file {`filename`: File to write the captured output. Equivalent to a positional filename.}
  }
proc redirect { args } {
  parse_key_args "redirect" args keys {-variable -file} flags {-append -tee}
  set filename ""
  if { [info exists keys(-file)] } {
    set filename $keys(-file)
  }
  if { [llength $args] == 2 } {
    set filename [lindex $args 0]
    set script [lindex $args 1]
  } elseif { [llength $args] == 1 } {
    set script [lindex $args 0]
  } else {
    cmd_usage_error "redirect"
  }

  set to_var [info exists keys(-variable)]
  set tee [info exists flags(-tee)]
  set append [info exists flags(-append)]
  # Stream to a file unless the output also has to land in a variable or
  # on the console. Those cases capture to a string first.
  set file_stream [expr {$filename != "" && !$to_var && !$tee}]
  set capturing [expr {$to_var || $tee}]

  if { $file_stream } {
    if { $append } {
      redirect_file_append_begin $filename
    } else {
      redirect_file_begin $filename
    }
  } elseif { $capturing } {
    redirect_string_begin
  }

  set code [catch { uplevel 1 $script } ret opts]

  if { $file_stream } {
    redirect_file_end
  }
  if { $capturing } {
    set output [redirect_string_end]
    if { $filename != "" } {
      if { $append } {
        set stream [open $filename a]
      } else {
        set stream [open $filename w]
      }
      puts -nonewline $stream $output
      close $stream
    }
    if { $tee } {
      puts -nonewline $output
    }
    if { $to_var } {
      if { $append } {
        uplevel 1 [list append $keys(-variable) $output]
      } else {
        uplevel 1 [list set $keys(-variable) $output]
      }
    }
  }
  if { $code } {
    return -options $opts $ret
  }
  return $ret
}

}

# Add date getter
proc date {} {
  return [clock format [clock seconds] -format "%Y-%m-%d %H:%M:%S"]
}

# Add memory usage getter
proc mem {} {
  return [exec ps -o rss= -p [pid]]
}

# Set difference function
proc ldiff {a b} {
  set result {}
  foreach x $a {
    if {[lsearch -exact $b $x] == -1} {
      lappend result $x
    }
  }
  return $result
}
