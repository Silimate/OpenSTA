# The command file and the ~/.sta init file are sourced as one file name
# when their path has spaces or Tcl syntax in it.
source helpers.tcl

proc write_script { file body } {
  set stream [open $file "w"]
  puts $stream $body
  close $stream
}

proc run_sta { args } {
  set status [catch { exec [info nameofexecutable] -no_splash {*}$args 2>@1 } output]
  puts "exit status $status"
  puts $output
}

set script_body {puts "sourced '[file tail [info script]]' in '[file tail [file dirname [info script]]]'"}

foreach dir_name [list "source_path_spaces.dir with space" \
                    "source_path_spaces.dir {braces} \[brackets\] \$dollar"] {
  set dir [make_result_file $dir_name]
  file mkdir $dir
  set cmd_file [file join $dir "cmd file.tcl"]
  write_script $cmd_file $script_body
  run_sta -no_init -exit $cmd_file
}

# The init file is found through HOME.
set home [make_result_file "source_path_spaces.home dir"]
file mkdir $home
write_script [file join $home ".sta"] {puts "init file sourced"}
set saved_home $env(HOME)
set env(HOME) $home
run_sta -exit $cmd_file
set env(HOME) $saved_home
