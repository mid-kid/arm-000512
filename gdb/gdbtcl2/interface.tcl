#
# Interface between Gdb and GdbTk.
#
#   Copyright (C) 1997, 1998 Cygnus Solutions

# This variable is reserved for this module.  Ensure it is an array.
global gdbtk_state
set gdbtk_state(busyCount) 0

# This is run when a breakpoint changes.  The arguments are the
# action, the breakpoint number, and the breakpoint info.
define_hook gdb_breakpoint_change_hook

####################################################################
#                                                                  #
#                        GUI STATE HOOKS                           #
#                                                                  #
####################################################################
# !!!!!   NOTE   !!!!!
# For debugging purposes, please put debug statements at the very
# beginning and ends of all GUI state hooks.

# GDB_BUSY_HOOK
#   This hook is used to register a callback when the UI should
#   be disabled because the debugger is either busy or talking
#   to the target.
#
#   All callbacks should disable ALL user input which could cause
#   any state changes in either the target or the debugger.
define_hook gdb_busy_hook

# GDB_IDLE_HOOK
#   This hook is used to register a callback when the UI should
#   be enabled because the debugger is no longer busy.
#
#   All callbacks should enable user input. These callbacks
#   should also be as fast as possible to avoid any significant
#   time delays when enabling the UI.
define_hook gdb_idle_hook

# GDB_UPDATE_HOOK
#   This hook is used to register a callback to update the widget
#   when debugger state has changed.
define_hook gdb_update_hook

# GDB_NO_INFERIOR_HOOK
#   This hook is used to register a callback which should be invoked
#   whenever there is no inferior (either at startup time or when
#   an inferior is killed).
#
#   All callbacks should reset their windows to a known, "startup"
#   state.
define_hook gdb_no_inferior_hook

# GDB_DISPLAY_CHANGE_HOOK
# This is run when a display changes.  The arguments are the action,
# the breakpoint number, and (optionally) the value.
define_hook gdb_display_change_hook

# GDB_TRACE_FIND_HOOK
#    This hook is run by the trace find command.  It is used to switch
#    from control to browse mode when the user runs tfind commands...
#
define_hook gdb_trace_find_hook

# ------------------------------------------------------------------
#  gdbtk_tcl_preloop - This function is called after gdb is initialized
#  but before the mainloop is started.  It sets the app name, and
#  opens the first source window.
# ------------------------------------------------------------------

proc gdbtk_tcl_preloop { } {
  global gdb_exe_name

  set_baud

  tk appname gdbtk
  # If there was an error loading an executible specified on the command line
  # then we will have called pre_add_symbol, which would set us to busy,
  # but not the corresponding post_add_symbol.  Do this here just in case...
  after idle gdbtk_idle 
  set src [ManagedWin::open SrcWin]
  debug "In preloop, with src: \"$src\" & error: \"$::errorInfo\""
  SrcWin::point_to_main
  set msg ""
  catch {gdb_cmd "info files"} msg
  set line1 [string range $msg 0 [string first \n $msg]]  
  if {[regexp {Symbols from "(.*)"\.} $line1 dummy name]} {
    set gdb_exe_name $name
  }
  gdbtk_update
}


# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_busy - run all busy hooks
#
#         Use this procedure from within GUI code to indicate that
#         the debugger is busy, either running the inferior or
#         talking to the target. This will call all the registered
#         gdb_busy_hook's.
# ------------------------------------------------------------------
proc gdbtk_busy {} {

  set err [catch {run_hooks gdb_busy_hook} txt]
  if {$err} { 
    debug "gdbtk_busy ERROR: $txt" 
  }

  # Force the screen to update
  update
}

# ------------------------------------------------------------------
#   PROCEDURE:  gdbtk_update - run all update hooks
#
#          Use this procedure to force all widgets to update
#          themselves. This hook is usually run after command
#          that could change target state.
# ------------------------------------------------------------------
proc gdbtk_update {} {
  set err [catch {run_hooks gdb_update_hook} txt]
  if {$err} { 
    debug "gdbtk_update ERROR: $txt" 
  }
  
  # Force the screen to update
  update
}

# ------------------------------------------------------------------
#   PROCEDURE: gdbtk_idle - run all idle hooks
#
#          Use this procedure to run all the gdb_idle_hook's,
#          which should free the UI for more user input. This
#          hook should only be run AFTER all communication with
#          the target has halted, otherwise the risk of two (or
#          more) widgets talking to the target arises.
# ------------------------------------------------------------------
proc gdbtk_idle {} {
  global gdb_running

  set err [catch {run_hooks gdb_idle_hook} txt]
  if {$err} { 
    debug "gdbtk_idle 1 ERROR: $txt" 
  }
  
  if {!$gdb_running} {
    set err [catch {run_hooks gdb_no_inferior_hook} txt]
    if {$err} { 
      debug "gdbtk_idle 2 ERROR: $txt" 
    }
  }    
  # Force the screen to update
  update
}

define_hook download_progress_hook

# Random hook of procs to call just before exiting.
define_hook gdb_quit_hook

# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_quit_check - Ask if the user really wants to quit.
# ------------------------------------------------------------------
proc gdbtk_quit_check {} {
  global gdb_downloading gdb_running
  
  if {$gdb_downloading} {
    set msg "Downloading to target,\n really close the debugger?"
    if {![gdbtk_tcl_query $msg no]} {
      return 0
    }
  } elseif {$gdb_running} {
    # While we are running the inferior, gdb_cmd is fenceposted and returns
    # immediately. Therefore, we need to ask here. Do we need to stop the target,
    # too?
    set msg "A debugging session is active.\n"
    append msg "Do you still want to close the debugger?"
    if {![gdbtk_tcl_query $msg no]} {
      return 0
    }
  }
  return 1
}

# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_quit - Quit the debugger
#         Call this procedure anywhere the user can request to quit.
#         This procedure will ask all the right questions and run
#         all the gdb_quit_hooks before exiting. 
# ------------------------------------------------------------------
proc gdbtk_quit {} {
  if {[gdbtk_quit_check]} {
    pref_save
    gdb_force_quit
  }
}

# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_cleanup - called by GDB immediately
#         before exiting.  Last chance to cleanup!
# ------------------------------------------------------------------
proc gdbtk_cleanup {} {
  # nothing to do right now.
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_query -
# ------------------------------------------------------------------
proc gdbtk_tcl_query {message {default yes}} {
  global gdb_checking_for_exit gdbtk_state tcl_platform

  # FIXME We really want a Help button here.  But Tk's brain-damaged
  # modal dialogs won't really allow it.  Should have async dialog
  # here.

  set title "GDB"
  set modal "task"

  # If we are checking whether to exit gdb, we want a system modal
  # box.  Otherwise it may be hidden by some other program, and the
  # user will have no idea what is going on.
  if {[info exists gdb_checking_for_exit] && $gdb_checking_for_exit} {
    set modal "system"
  }
  
  if {$tcl_platform(platform) == "windows"} {
    # On Windows, we want to only ask each question once.
    # If we're already asking the question, just wait for the answer
    # to come back.
    set ans [list answer $message]
    set pending [list pending $message]

    if {[info exists gdbtk_state($pending)]} {
      incr gdbtk_state($pending)
    } else {
      set gdbtk_state($pending) 1
      set gdbtk_state($ans) {}

      ide_messageBox [list set gdbtk_state($ans)] -icon warning \
	-default $default -message $message -title $title \
	-type yesno -modal $modal -parent .
    }

    vwait gdbtk_state($ans)
    set r $gdbtk_state($ans)
    if {[incr gdbtk_state($pending) -1] == 0} {
      # Last call waiting for this answer, so clear it.
      unset gdbtk_state($pending)
      unset gdbtk_state($ans)
    }
  } else {
    # On Unix, apparently it doesn't matter how many times we ask a
    # question.
    set r [tk_messageBox -icon warning -default $default \
	     -message $message -title $title \
	     -type yesno -modal $modal -parent .]
  }

  update idletasks
  return [expr {$r == "yes"}]
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_warning -
# ------------------------------------------------------------------
proc gdbtk_tcl_warning {message} {
  debug "$message"

# ADD a warning message here if the gui must NOT display it
# add the message at the beginning of the switch followed by - 

  switch -regexp $message {
        "Unable to find dynamic linker breakpoint function.*" {return}
        default {show_warning $message}
       }
}

# ------------------------------------------------------------------
# PROC: show_warning -
# ------------------------------------------------------------------
proc show_warning {message} {
  global tcl_platform

  # FIXME We really want a Help button here.  But Tk's brain-damaged
  # modal dialogs won't really allow it.  Should have async dialog
  # here.
  set title "GDB"
  set modal "task"
 
  if {$tcl_platform(platform) == "windows"} {
      ide_messageBox -icon warning \
        -default ok -message $message -title $title \
        -type ok -modal $modal -parent .
  } else {
    set r [tk_messageBox -icon warning -default ok \
             -message $message -title $title \
             -type ok -modal $modal -parent .]
  }
} 

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_ignorable_warning -
# ------------------------------------------------------------------
proc gdbtk_tcl_ignorable_warning {class message} {
  catch {ManagedWin::open WarningDlg -center -transient \
	   -message [list $message] -ignorable $class}
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_fputs -
# ------------------------------------------------------------------
proc gdbtk_tcl_fputs {message} {
  global gdbtk_state
  if {$gdbtk_state(console) != ""} {
    $gdbtk_state(console) insert $message
  }
}

# ------------------------------------------------------------------
# PROC: echo -
# ------------------------------------------------------------------
proc echo {args} {
  gdbtk_tcl_fputs [concat $args]\n
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_fputs_error -
# ------------------------------------------------------------------
proc gdbtk_tcl_fputs_error {message} {
  global gdbtk_state
  if {$gdbtk_state(console) != ""} {
    $gdbtk_state(console) einsert $message
    update
  }
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_flush -
# ------------------------------------------------------------------
proc gdbtk_tcl_flush {} {
  debug [info level 0]
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_start_variable_annotation -
# ------------------------------------------------------------------
proc gdbtk_tcl_start_variable_annotation {valaddr ref_type stor_cl
					  cum_expr field type_cast} {
  debug [info level 0]
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_end_variable_annotation -
# ------------------------------------------------------------------
proc gdbtk_tcl_end_variable_annotation {} {
  debug [info level 0]
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_breakpoint -
# ------------------------------------------------------------------
proc gdbtk_tcl_breakpoint {action bpnum addr line file bp_type enabled thread} {
  debug "BREAKPOINT: $action $bpnum $addr $line $file $bp_type $enabled $thread "
  run_hooks gdb_breakpoint_change_hook $action $bpnum $addr $line $file $bp_type $enabled $thread
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_tracepoint -
# ------------------------------------------------------------------
proc gdbtk_tcl_tracepoint {action tpnum addr line file pass_count} {
#  debug "TRACEPOINT: $action $tpnum $addr $line $file $pass_count"
  run_hooks gdb_breakpoint_change_hook $action $tpnum $addr $line $file tracepoint
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_trace_find_hook -
# ------------------------------------------------------------------
proc gdbtk_tcl_trace_find_hook {arg from_tty} {
  debug "Running trace find hook with $arg $from_tty"
  run_hooks gdb_trace_find_hook $arg $from_tty
}

################################################################
#
# Handle `readline' interface.
#

# Run a command that is known to use the "readline" interface.  We set
# up the appropriate buffer, and then run the actual command via
# gdb_cmd.  Calls into the "readline" callbacks just return values
# from our list.

# ------------------------------------------------------------------
# PROC: gdb_run_readline_command -
# ------------------------------------------------------------------
proc gdb_run_readline_command {command args} {
  global gdbtk_state
  debug "run readline_command $command $args"
  set gdbtk_state(readlineArgs) $args
  gdb_cmd $command
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_readline_begin -
# ------------------------------------------------------------------
proc gdbtk_tcl_readline_begin {message} {
  global gdbtk_state
  debug "readline begin"
  set gdbtk_state(readline) 0
  if {$gdbtk_state(console) != ""} {
    $gdbtk_state(console) insert $message
  }
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_readline -
# ------------------------------------------------------------------
proc gdbtk_tcl_readline {prompt} {
  global gdbtk_state
  debug "gdbtk_tcl_readline $prompt"
  if {[info exists gdbtk_state(readlineArgs)]} {
    # Not interactive, so pop the list, and print element.
    set cmd [lvarpop gdbtk_state(readlineArgs)]
    command::insert_command $cmd
  } else {
    # Interactive.
    debug "interactive"
    set gdbtk_state(readline) 1
    $gdbtk_state(console) activate $prompt
    vwait gdbtk_state(readline_response)
    set cmd $gdbtk_state(readline_response)
    debug "got response: $cmd"
    unset gdbtk_state(readline_response)
    set gdbtk_state(readline) 0
  }
  return $cmd
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_readline_end -
# ------------------------------------------------------------------
proc gdbtk_tcl_readline_end {} {
  global gdbtk_state
  debug "readline_end"
  catch {unset gdbtk_state(readlineArgs)}
  unset gdbtk_state(readlineActive)
  command::end_multi_line_input
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_busy - this is called immediately before gdb 
#    executes a command.
#
# ------------------------------------------------------------------
proc gdbtk_tcl_busy {} {
  global gdbtk_state
  if {[incr gdbtk_state(busyCount)] == 1} {
    gdbtk_busy
  }
}

################################################################
#
# 
#

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_idle - this is called immediately after gdb 
#    executes a command.
# ------------------------------------------------------------------
proc gdbtk_tcl_idle {} {
  global gdbtk_state
  if {$gdbtk_state(busyCount) > 0
      && [incr gdbtk_state(busyCount) -1] == 0} {
    gdbtk_update
    gdbtk_idle
  }
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_tstart -
# ------------------------------------------------------------------
proc gdbtk_tcl_tstart {} {
  set srcwin [lindex [manage find src] 0]
  $srcwin.toolbar do_tstop 0
  
}

# ------------------------------------------------------------------
# PROC: gdbtk_tcl_tstop -
# ------------------------------------------------------------------
proc gdbtk_tcl_tstop {} {
  set srcwin [lindex [manage find src] 0]
  $srcwin.toolbar do_tstop 0
  
}


# ------------------------------------------------------------------
# PROC: gdbtk_tcl_display -
#
# A display changed.  ACTION is `enable', `disable', `delete',
# `create', or `update'.  VALUE is only meaningful in the `update'
# case.
# ------------------------------------------------------------------
proc gdbtk_tcl_display {action number {value {}}} {
  # Handle create explicitly.
  if {$action == "create"} {
    manage create_if_never data
  }
  run_hooks gdb_display_change_hook $action $number $value
}

# ------------------------------------------------------------------
#  PROCEDURE: gdbtk_register_changed
#         This hook is called from value_assign to inform us that
#         the user has changed the contents of a register.
# ------------------------------------------------------------------
proc gdbtk_register_changed {} {
  after idle gdbtk_update
}

# ------------------------------------------------------------------
#  PROCEDURE: gdbtk_memory_changed
#         This hook is called from value_assign to inform us that
#         the user has changed the contents of memory (including
#         the program's variables).
# ------------------------------------------------------------------
proc gdbtk_memory_changed {} {
  after idle gdbtk_update
}

####################################################################
#                                                                  #
#                           FILE HOOKS                             #
#                                                                  #
#    There are a number of hooks that are installed in gdb to      #
#    aid with file-like commands (selecting an exectuable and      #
#    loading symbols):                                             #
#         - exec_file_display_hook                                 #
#            Called in exec_file_command. The tcl hook is          #
#            "gdbtk_tcl_exec_file_display"                         #
#         - file_changed_hook                                      #
#            Called in file_command. The tcl hook is               #
#            "gdbtk_tcl_file_changed"                              #
#         - pre_add_symbol_hook                                    #
#            Called in symbol_file_add before loading. The tcl     #
#            hook is "gdbtk_tcl_pre_add_symbol"                    #
#         - post_add_symbol_hook                                   #
#            Called in symbol_file_add when finished loading       #
#            a symbol file. The tcl hook is                        #
#            "gdbtk_tcl_post_add_symbol"                           #
#                                                                  #
#  Together, these hooks should give the gui enough information    #
#  to cover the two most common uses of file commands:             #
#  1. executable with symbols                                      #
#  2. separate executable and symbol file(s)                       #
#                                                                  #
####################################################################
define_hook file_changed_hook

# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_tcl_pre_add_symbol
#         This hook is called before any symbol files
#         are loaded so that we can inform the user.
# ------------------------------------------------------------------
proc gdbtk_tcl_pre_add_symbol {file} {

  gdbtk_busy

  # Display some feedback to the user
  set srcs [ManagedWin::find SrcWin]
  foreach w $srcs {
    $w set_status "Reading symbols from $file..."
  }
  update idletasks
}

# ------------------------------------------------------------------
#   PROCEDURE: gdbtk_tcl_post_add_symbol
#          This hook is called after we finish reading a symbol
#          file, so the source windows' combo boxes need filling.
# ------------------------------------------------------------------
proc gdbtk_tcl_post_add_symbol {} {

  set srcs [ManagedWin::find SrcWin]
  foreach w $srcs {
    $w fillNameCB
  }
  gdbtk_idle
}

# ------------------------------------------------------------------
#  PROCEDURE: gdbtk_tcl_file_changed
#         This hook is called whenever the exec file changes.
#         This is called AFTER symbol reading, so it is
#         ok to point to main when we get called.
# ------------------------------------------------------------------
proc gdbtk_tcl_file_changed {filename} {

  SrcWin::point_to_main
  run_hooks file_changed_hook
}

# ------------------------------------------------------------------
#  PROCEDURE: gdbtk_tcl_exec_file_display 
#         This hook is called from exec_file_command. It's purpose
#         is to setup the gui for a new file. Note that we cannot
#         look for main, since this hook is called BEFORE we
#         read symbols. If the user used the "file" command,
#         gdbtk_tcl_file_changed will set the source window to
#         look at main. If the user used "exec-file" and "add-symbol"
#         commands, then we cannot look for main.
# ------------------------------------------------------------------
proc gdbtk_tcl_exec_file_display {filename} {
  global gdb_loaded gdb_running gdb_exe_name gdb_target_changed

  set gdb_loaded 0
  set gdb_running 0
  set gdb_target_changed 1

  # DO NOT CALL set_exe here! 
  # set_exe calls file command with the filename in
  # quotes, so we need to strip them here.
  set gdb_exe_name [string trim $filename \']

  # Reset the source windows to handle the new executable and
  # try to find main (for cases when we load new symbols and then
  # specify an exec file).
  set srcs [ManagedWin::find SrcWin]
  foreach w $srcs {
    $w reset
  }

  SrcWin::point_to_main
}

##############################################
#  The rest of this file is an assortment of Tcl wrappers
#  for various bits of gdb functionality.
#
#############################################

# ------------------------------------------------------------------
# PROC: set_exe_name - Update the executable name
# ------------------------------------------------------------------
proc set_exe_name {exe} {
  global gdb_exe_name gdb_exe_changed
  #debug "set_exe_name: exe=$exe  gdb_exe_name=$gdb_exe_name"
  if {$gdb_exe_name != $exe} {
    set gdb_exe_name $exe
    set gdb_exe_changed 1    
  }
}


# ------------------------------------------------------------------
# PROC: set_exe -
# ------------------------------------------------------------------ 
proc set_exe {} {
  global gdb_exe_name gdb_exe_changed gdb_target_changed gdb_loaded file_done
  debug "set_exe gdb_exe_changed=$gdb_exe_changed gdb_exe_name=$gdb_exe_name"
  if {$gdb_exe_changed} {
    set gdb_exe_changed 0
    if {$gdb_exe_name == ""} { return }
    gdb_clear_file
    set err [catch {gdb_cmd "file '$gdb_exe_name'" 1} msg]
    if {$err} {
      debug "set_exe ERROR: $msg"
      set l [split $msg :]
      set errtxt [join [lrange $l 1 end] :]
      set msg "Error loading \"$gdb_exe_name\":\n"
      append msg $errtxt
      tk_messageBox -title "Error" -message $msg -icon error \
	-type ok
      set gdb_exe_name {}
      set file_done 0
      return
    } elseif {[string match {*no debugging symbols found*} $msg]} {
      tk_messageBox -icon error -default ok \
	-title "GDB" -type ok -modal system \
	-message "This executable has no debugging information."
      set gdb_exe_name {}
      set file_done 0
      return
    }

    # force new target command
    set gdb_target_changed 1
    set gdb_loaded 0
    set file_done 1
  }
}

# ------------------------------------------------------------------
#  _open_file - open a file dialog to select a file for debugging.
# ------------------------------------------------------------------

proc _open_file {} {
  global gdb_running gdb_downloading tcl_platform
  
  if {$gdb_running || $gdb_downloading} {
    # We are already running/downloading something..
    if {$gdb_running} {
      set msg "A debugging session is active.\nAbort session and load new file?"
    } else {
      set msg "A download is in progress.\nAbort download and load new file?"
    }
    if {![gdbtk_tcl_query $msg no]} {
      return 0
    }
  }

  set curFocus [focus]

  # Make sure that this is really a modal dialog...
  # FIXME: Add a disable_all to ide_grab_support.

  ide_grab_support disable_except {}
  set file [tk_getOpenFile -parent . -title "Load New Executable"]
  ide_grab_support enable_all

  # If no one had the focus before, leave it that way (since I
  # am not sure how this could happen...

  if {$curFocus != ""} {
    raise [winfo toplevel $curFocus]
    focus $curFocus
  }

  if {$file == ""} {
    return 0
  }
  # Add the base dir for this file to the source search path.
  set root [file dirname $file]
  if {$tcl_platform(platform) == "windows"} {
    set root [ide_cygwin_path to_posix $root]
    set file [ide_cygwin_path to_posix $file]
  }
  
  gdb_clear_file
  catch {gdb_cmd "cd $root"}

  # The gui needs to set this...
  set_exe_name $file
  
  # set_exe needs to be called anywhere the gui does a file_command...
  if {[set_exe] == "cancel"} {
    gdbtk_update
    gdbtk_idle
    return 0
  }
  
  return 1
}

# ------------------------------------------------------------------
# PROC: set_target_name - Update the target name.  
#
# If $prompt is 0, no dialog is displayed and the target command is 
# created from preferences.
#
# RETURN:
#     1 if successful, 
#     0 if the not (the user canceled the target selection dialog)
# ------------------------------------------------------------------
proc set_target_name {targ {prompt 1}} {
  global gdb_target_name gdb_target_changed gdb_exe_changed
  global gdb_target_cmd gdb_pretty_name

  set cmd_tmp $gdb_target_cmd
  set name_tmp $gdb_target_name
  if {$prompt} {
    set win [ManagedWin::open TargetSelection -exportcancel 1 -center \
	       -transient]
    # need to call update here so the target selection dialog can take over
    update idletasks
  }

  if {$gdb_target_name == "CANCEL"} {
    set gdb_target_cmd $cmd_tmp
    set gdb_target_name $name_tmp
    return 0
  }
  set target $gdb_target_name
  set targ [TargetSelection::getname $target cmd]
  set gdb_target_cmd $cmd_tmp
  set gdb_pretty_name [TargetSelection::getname $target pretty-name]
    
  set targ_opts ""
  debug "target=$target pretty_name=$gdb_pretty_name"
  switch -regexp -- $gdb_target_name {
    sim|ice {
      set targ $gdb_target_name
      set targ_opts [pref getd gdb/load/${gdb_target_name}-opts]
    }     
    default {
      set port [pref getd gdb/load/$target-port]
      if {$port == ""} {
	set port [pref get gdb/load/default-port]
      }
      set portnum [pref getd gdb/load/$target-portname]
      if {$portnum == ""} {
	set portnum [pref get gdb/load/default-portname]
      }
      set hostname [pref getd gdb/load/$target-hostname]
      if {$hostname == ""} {
	set hostname [pref get gdb/load/default-hostname]
      }
      # replace "com1" with the real port name
      set targ [lrep $targ "com1" $port]
      # replace "tcpX" with hostname:portnum
      set targ [lrep $targ "tcpX" ${hostname}:${portnum}]
    }
  }
    
  #debug "set_target_name: $targ gdb_target_cmd=$gdb_target_cmd"
  if {$gdb_target_cmd != $targ || $gdb_target_changed} {
    set gdb_target_changed 1
    set gdb_target_cmd "$targ $targ_opts"
  }

  return 1
}

# ------------------------------------------------------------------
# PROC: set_target - Change the target
# ------------------------------------------------------------------
proc set_target {} {
  global gdb_target_cmd gdb_target_changed gdb_pretty_name gdb_target_name
  debug "set_target \"$gdb_target_changed\" \"$gdb_target_cmd\""
  if {$gdb_target_cmd == ""} {
    if {![set_target_name "" 1]} {
      return 2
    }
  }
  
  if {$gdb_target_changed} {
    set srcWin [lindex [ManagedWin::find SrcWin] 0]

    $srcWin set_status "Trying to communicate with target $gdb_pretty_name" 1
    update
    catch {gdb_cmd "detach"}
    debug "CONNECTING TO TARGET: $gdb_target_cmd"
    set err [catch {gdb_immediate "target $gdb_target_cmd"} msg ]
    $srcWin set_status

    if {$err} {
      if {[string first "Program not killed" $msg] != -1} {
	return 2
      }
      update
      set dialog_title "GDB"
      set debugger_name "GDB"
      tk_messageBox -icon error -title $dialog_title -type ok \
	-modal task -message "$msg\n\n$debugger_name cannot connect to the target board\
using [lindex $gdb_target_cmd 1].\nVerify that the board is securely connected and, if\
necessary,\nmodify the port setting with the debugger preferences."
      return 0
    }
    
    if {![catch {pref get gdb/load/$gdb_target_name-after_attaching} aa] && $aa != ""} {
      if {[catch {gdb_cmd $aa} err]} {
	catch {[ManagedWin::find Console] einsert $err}
      }
    }
    set gdb_target_changed 0
    return 1
  }
  return 3
}

# ------------------------------------------------------------------
# PROC: run_executable -
#
# This procedure is used to run an executable.  It is called when the 
# run button is used.
# ------------------------------------------------------------------
proc run_executable { {auto_start 1} } {
  global gdb_loaded gdb_downloading gdb_target_name
  global gdb_exe_changed gdb_target_changed gdb_program_has_run
  global gdb_loaded

  debug "run_executable $auto_start $gdb_target_name"

  # This is a hack.  If the target is "sim" the opts are appended
  # to the target command. Otherwise they are assumed to be command line
  # args.  What about simulators that accept command line args?
  if {$gdb_target_name != "sim"} {
    # set args
    set gdb_args [pref getd gdb/load/$gdb_target_name-opts]
    if { $gdb_args != ""} {
      debug "set args $gdb_args"
      catch {gdb_cmd "set args $gdb_args"}
    }
  }

  if {$gdb_downloading} { return }
  if {[pref get gdb/control_target]} {
    # Breakpoint mode

    set_exe

    # Attach
    if {[pref get gdb/src/run_attach]} {
      debug "Attaching...."
      while {1} {
	switch [set_target] {
	  0 {
	    # target command failed, ask for a new target name
	      debug "run_executable 1"
	      if {![set_target_name ""]} {
	      # canceled again
	      catch {gdb_cmd "delete $bp_at_main"}
	      catch {gdb_cmd "delete $bp_at_exit"}
	      debug "run_executable 2"
	      return
	    }
	  }
	  1 {
	    # success -- target changed
	      debug "run_executable 3"
	    set gdb_loaded 0
	    break 
	  }
	  2 {
	    # command cancelled by user
	      debug "run_executable 4"
	    catch {gdb_cmd "delete $bp_at_main"}
	    catch {gdb_cmd "delete $bp_at_exit"}
	    return
	  }

	  3 {
	      # success -- target NOT changed (i.e., rerun)
	      break
	  }
	}
      }
    }

    # Download
    if {[pref get gdb/src/run_load]} {
      debug "Downloading..."
      # If we have a new run request and the executable or target has changed
      # then we need to download the new executable to the new target.
      if {$gdb_exe_changed || $gdb_target_changed} {
	set gdb_loaded 0
      }
      debug "run_executable: gdb_loaded=$gdb_loaded"
      set saved_loaded $gdb_loaded
      # if the app has not been downloaded or the app has already
      # started, we need to redownload before running
      if {!$gdb_loaded} {
	if {[Download::download_it]} {
	  # user cancelled the command
	  set gdb_loaded $saved_loaded
	  gdbtk_update
	  gdbtk_idle
	}
	if {!$gdb_loaded} {
	  # The user cancelled the download after it started
	  gdbtk_update
	  gdbtk_idle
	  return
	}
      }
    }

    # _Now_ set/clear breakpoints
    if {[pref get gdb/load/exit] && ![TargetSelection::native_debugging]} {
      debug "Setting new BP at exit"
      catch {gdb_cmd "clear exit"}
      catch {gdb_cmd "break exit"}
    }
      
    if {[pref get gdb/load/main]} {
      debug "Setting new BP at main"
      catch {gdb_cmd "clear main"}
      catch {gdb_cmd "break main"}
    }

    # set BP at user-specified function
    if {[pref get gdb/load/bp_at_func]} {
      foreach bp [pref get gdb/load/bp_func] {
	debug "Setting BP at $bp"
	catch {gdb_cmd "clear $bp"}
	catch {gdb_cmd "break $bp"}
      }
    }
    
    # Run
    if {[pref get gdb/src/run_run] || [pref get gdb/src/run_cont]} {
      if {$auto_start} {
	if {[pref get gdb/src/run_run]} {
	  debug "Runnning target..."
	  set run run
	} else {
	  debug "Continuing target..."
	  set run cont
	}
	if {$gdb_target_name == "exec"} {
	  set run run
	}
	catch {gdb_immediate $run} msg
	if {[string match "No executable*" $msg]} {
	  # No executable was specified.  Prompt the user for one.
	  gdbtk_idle
	  if {[_open_file]} {
	    run_executable $auto_start
	  } else {debug "CANCELLED"}
	  return
	}
      } else {
	SrcWin::point_to_main
      }
    }
    gdbtk_update
    gdbtk_idle
  } elseif {[pref get gdb/mode]} {
    # tracepoint -- need to tstart
    set gdb_running 1
    tstart
  }
  return
}

# ------------------------------------------------------------------
#  PROC: gdbtk_step - step the target
# ------------------------------------------------------------------
proc gdbtk_step {} {
  catch {gdb_immediate step}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_next
# ------------------------------------------------------------------
proc gdbtk_next {} {
  catch {gdb_immediate next}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_finish
# ------------------------------------------------------------------
proc gdbtk_finish {} {
  catch {gdb_immediate finish}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_continue
# ------------------------------------------------------------------
proc gdbtk_continue {} {
  catch {gdb_immediate continue}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_stepi
# ------------------------------------------------------------------
proc gdbtk_stepi {} {
  catch {gdb_immediate stepi}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_nexti
# ------------------------------------------------------------------
proc gdbtk_nexti {} {
  catch {gdb_immediate nexti}
}

# ------------------------------------------------------------------
#  PROC: gdbtk_stop
# ------------------------------------------------------------------
#
# The stop button is tricky. In order to use the stop button,
# the debugger must be able to keep gui alive while target_wait is
# blocking (so that the user can interrupt or detach from it).
# 
# The best solution for this is to capture gdb deep down where it
# can block. For _any_ target board, this will be in either
# serial or socket code. These places call ui_loop_hook to 
# keep us alive. For native unix, we use an interval timer.
# Simulators either call ui_loop_hook directly (older sims, at least)
# or they call gdb's os_poll_quit callback, where we insert a call
# to ui_loop_hook. Some targets (like v850ice and windows native)
# require a call to ui_loop_hook directly in target_wait. See comments
# before gdb_stop and x_event to find out more about how this is accomplished.
#
# The stop button's behavior:
# Pressing the stop button should attempt to stop the target. If, after
# some time (like 3 seconds), gdb fails to fall out of target_wait (i.e.,
# the gui's idle hooks are run), then open a dialog asking the user if
# he'd like to detach.
proc gdbtk_stop {} {
  global _gdbtk_stop

  if {$_gdbtk_stop(timer) == ""} {
    add_hook gdb_idle_hook gdbtk_stop_idle_callback
    set _gdbtk_stop(timer) [after 3000 gdbtk_detach]
    catch {gdb_stop}
  }
}

# ------------------------------------------------------------------
#  PROC: gdbtk_stop_idle_callback
# ------------------------------------------------------------------
# This callback normally does nothing. When the stop button has
# been pressed, though, and gdb has successfully stopped the target,
# this callback will clean up after gdbtk_stop, removing the "Detach"
# dialog (if it's open) and gettingg rid of any outstanding timers
# and hooks.
proc gdbtk_stop_idle_callback {} {
  global _gdbtk_stop gdbtk_state

  # Check if the dialog asking if user wants to detach is open
  # and unpost it if it exists.
  if {$_gdbtk_stop(msg) != ""} {
    set ans [list answer $_gdbtk_stop(msg)]
    set gdbtk_state($ans) no
  }

  if {$_gdbtk_stop(timer) != ""} {
    # Cancel the timer callback
    after cancel $_gdbtk_stop(timer)
    set _gdbtk_stop(timer) ""
    remove_hook gdb_idle_hook gdbtk_stop_idle_callback
  }
}

# ------------------------------------------------------------------
#  PROC: gdbtk_detach
# ------------------------------------------------------------------
# This proc is installed as a timer event when the stop button
# is pressed. If target_wait doesn't return (we were unable to stop
# the target), then this proc is called.
#
# Open a dialog box asking if the user would like to detach. If so,
# try to detach. If not, do nothing and go away.
proc gdbtk_detach {} {
  global _gdbtk_stop

  set _gdbtk_stop(msg) "No response from target. Detach from target\n(and stop debugging it)?"
  if {[gdbtk_tcl_query  $_gdbtk_stop(msg) no]} {
    catch {gdb_stop detach}
  }

  set _gdbtk_stop(timer) ""
  set _gdbtk_stop(msg) ""
  remove_hook gdb_idle_hook gdbtk_stop_idle_callback
}

# ------------------------------------------------------------------
#  PROC: gdbtk_run
# ------------------------------------------------------------------
proc gdbtk_run {} {
  run_executable
}

# ------------------------------------------------------------------
# PROC: set_baud -  Tell GDB the baud rate.
# ------------------------------------------------------------------
proc set_baud {} {
  global gdb_target_name
  #set target [ide_property get target-internal-name]
  set baud [pref getd gdb/load/${gdb_target_name}-baud]
  if {$baud == ""} {
    set baud [pref get gdb/load/baud]
  }
  debug "setting baud to $baud"
  catch {gdb_cmd "set remotebaud $baud"}
}

# ------------------------------------------------------------------
# PROC: do_state_hook -
# ------------------------------------------------------------------
proc do_state_hook {varname ind op} {
  run_hooks state_hook $varname
}

# ------------------------------------------------------------------
# PROC: connect -
# ------------------------------------------------------------------
proc connect {{async 0}} {
    global gdb_loaded file_done gdb_running
    debug "in connect"
    gdbtk_busy
    set result [set_target]
    debug "result is $result"
    switch $result {
     0 {
       gdbtk_idle
       set gdb_target_changed 1
       return 0
       }
     1 {
       set gdb_loaded 1
       if {[pref get gdb/load/check] && $file_done} {
             set err [catch {gdb_cmd "compare-sections"} errTxt]
             if {$err} {
               tk_messageBox -title "Error" -message $errTxt -icon error \
                -type ok
             }
       }
       gdbtk_idle
       tk_messageBox -title "GDB" -message "Successfully connected" \
	 -icon info -type ok
       return 1
       } 
     2 {
        # cancelled by user
        gdbtk_idle
        tk_messageBox -title "GDB" -message "Connection Cancelled" -icon info \
         -type ok
        return 0
       }
    }     
 }

# ------------------------------------------------------------------
# PROC: disconnect -
# ------------------------------------------------------------------
proc disconnect {{async 0}} {
   global gdb_loaded gdb_target_changed
   catch {gdb_cmd "detach"}
   # force a new target command to do something
   set gdb_loaded 0
   set gdb_target_changed 1
   set gdb_running 0 
 }

# ------------------------------------------------------------------
# PROC: tstart -
# ------------------------------------------------------------------
proc tstart {} {
   if {[catch {gdb_cmd "tstart"} errTxt]} {
     tk_messageBox -title "Error" -message $errTxt -icon error \
       -type ok
    gdbtk_idle
     return 0
   }
  return 1
}

# ------------------------------------------------------------------
# PROC: tstop -
# ------------------------------------------------------------------
proc tstop {} {

   if {[catch {gdb_cmd "tstop"} errTxt]} {
     tk_messageBox -title "Error" -message $errTxt -icon error \
       -type ok
     gdbtk_idle
     return 0
   }
   return 1
 }

# ------------------------------------------------------------------
# PROC: source_file -
# ------------------------------------------------------------------
proc source_file {} {
  set file_name [tk_getOpenFile -title "Choose GDB Command file"]
  if {$file_name != ""} {
    gdb_cmd "source $file_name"
  }
}
