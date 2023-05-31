#
# main.tcl
# ----------------------------------------------------------------------
# GDB tcl entry point
#
# Just initialize everything then set up main state engine
# ----------------------------------------------------------------------
#   Copyright (C) 1997, 1998, 1999 Cygnus Solutions
#

# State is controlled by 5 global boolean variables.
#
# gdb_target_changed
# gdb_exe_changed
# gdb_running
# gdb_downloading
# gdb_loaded

################### Initialization code #########################

# If GDBtk fails to start at all, you might want to uncomment one or 
# both of these.
#set tcl_traceExec 2
#set tcl_traceCompile 1

# Add gdb's Tcl library directory to the end of the auto-load search path, if 
# it isn't already on the path:
# Note: GDBTK_LIBRARY will be set in tcl_findLibrary before main.tcl is called.

if {[info exists auto_path]} {
  if {[lsearch -exact $auto_path $GDBTK_LIBRARY] < 0} {
    lappend auto_path $GDBTK_LIBRARY
  }
}

# Require the packages we need.  Most are loaded already, but this will catch 
# any odd errors... :
package require Tcl 8.0
package require Tk 8.0
package require Itcl 3.0
package require Itk 3.0
package require Gdbtk 1.0
package require combobox 1.0

namespace import itcl::*

namespace import debug::*

if {![find_iwidgets_library]} {
  tk_messageBox -title Error -message "Could not find the Iwidgets libraries.
Got nameofexec: [info nameofexecutable]
Error(s) were: \n$errMsg" \
      -icon error -type ok
  exit
}

# Environment variables controlling debugging:
# GDBTK_TRACE
#	unset or 0	no tracing
#	1		tracing initialized but not started
#	2		tracing initialized and started
#
# GDBTK_DEBUGFILE - filename to write debugging messages and
#	trace information (if tracing is enabled).
#
if {[info exists env(GDBTK_TRACE)] && $env(GDBTK_TRACE) != 0} {
  # WARNING: the tracing code must not trace into itself or
  # infinite recursion will result. As currently configured
  # the tracing code will not trace basic tcl functions or anything defined
  # before debug::init.  For this reason we must source the DebugWin
  # code before debug::init is called.
  source [file join $GDBTK_LIBRARY debugwin.ith]
  source [file join $GDBTK_LIBRARY debugwin.itb]

  # Calling this installs our hooks for tracing and profiling.
  # This WILL slow things down.
  ::debug::init

  if {$env(GDBTK_TRACE) == 2} {
    ::debug::trace_start
  }
}

if {[info exists env(GDBTK_DEBUGFILE)]} {
  ::debug::logfile $env(GDBTK_DEBUGFILE)
}

if {$tcl_platform(platform) == "unix"} {
#  tix resetoptions TK TK
#  tk_setPalette tan
  tix resetoptions TixGray [tix cget -fontset]
}

# initialize state variables

set gdb_target_changed 0
set gdb_exe_changed 0
set gdb_running 0
set gdb_downloading 0
set gdb_loaded 0
set gdb_program_has_run 0
set file_done 0
set gdb_target_name {}
set gdb_pretty_name {}
set gdb_exec {}
set gdb_target_cmd ""
    

# set traces on state variables
trace variable gdb_running w do_state_hook
trace variable gdb_downloading w do_state_hook
trace variable gdb_loaded w do_state_hook
define_hook state_hook

set download_dialog ""

# gdb_pretty_name is the name of the GDB target as it should be
# displayed to the user.
set gdb_pretty_name ""

# gdb_exe_name is the name of the executable we are debugging.  
set gdb_exe_name ""

# Initialize readline
set gdbtk_state(readline) 0
set gdbtk_state(console) ""

# set up preferences
pref init

# let libgui tell us how to feel
standard_look_and_feel

# now let GDB set its default preferences
pref_set_defaults

#start-sanitize-kod
# check for existence of a kod command and get it's name and
# text for menu entry
set gdb_kod_cmd ""
set gdb_kod_name ""
set msg ""
if {![catch {gdb_cmd "show kod"} msg] && ($msg != "")} {
  set line1 [string range $msg 0 [expr [string first \n $msg] -1]]
  if {[regexp {([^-]+) - (.+)} $line1 dummy cmd name]} {
    set gdb_kod_cmd $cmd
    set gdb_kod_name $name
  }
}
debug "kod_cmd=$gdb_kod_cmd  kod_name=$gdb_kod_name"
#end-sanitize-kod

# setup stop button
set _gdbtk_stop(timer) ""
set _gdbtk_stop(msg) ""

# read in preferences
pref_read

init_disassembly_flavor

ManagedWin::init

# This stuff will help us play nice with WindowMaker's AppIcons.
# Can't do the first bit yet, since we don't get this from gdb...
# wm command . [concat $argv0 $argv] 
wm group . . 

# Open debug window if testsuite is not running and GDBTK_DEBUG is set
if {![info exists env(GDBTK_TEST_RUNNING)] || !$env(GDBTK_TEST_RUNNING)} {
  if {[info exists env(GDBTK_DEBUG)] && $env(GDBTK_DEBUG) > 1} {
    ManagedWin::open DebugWin
  }
}

# some initial commands to get gdb in the right mode
gdb_cmd {set height 0}
gdb_cmd {set width 0}

# gdb_target_name is the name of the GDB target; that is, the argument
# to the GDB target command.
set gdb_target_name [pref get gdb/load/target]
set gdb_target_cmd ""
set gdb_target_changed 1
#set_target_name "" 0

update
gdbtk_idle

