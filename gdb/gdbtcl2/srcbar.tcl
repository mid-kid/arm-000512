#
# GDBSrcBar
# ----------------------------------------------------------------------
# Implements a toolbar that is attached to a source window.
#
#   PUBLIC ATTRIBUTES:
#
#
#   METHODS:
#
#     config ....... used to change public attributes
#
#   PRIVATE METHODS
#
#   X11 OPTION DATABASE ATTRIBUTES
#
#
# ----------------------------------------------------------------------
#   AUTHOR:  Tom Tromey                tromey@cygnus.com   
#            Copyright (C) 1997, 1998, 1999  Cygnus Solutions   
#

class GDBSrcBar {
  inherit GDBToolBar

  # ------------------------------------------------------------------
  #  CONSTRUCTOR - create widget
  # ------------------------------------------------------------------
  constructor {src args} {
    eval GDBToolBar::constructor $src
  } {
    eval itk_initialize $args
    add_hook gdb_trace_find_hook "$this trace_find_hook"
  }

  # ------------------------------------------------------------------
  #  DESTRUCTOR - destroy window containing widget
  # ------------------------------------------------------------------
  destructor {
    global GDBSrcBar_state
    unset GDBSrcBar_state($this)
    remove_hook gdb_trace_find_hook "$this trace_find_hook"
  }

  #
  #  PUBLIC DATA
  #

  # This is the command that should be run when the `update'
  # checkbutton is toggled.  The current value of the checkbutton is
  # appended to the command.
  public variable updatecommand {}

  # This controls whether the `update' checkbutton is turned on or
  # off.
  public variable updatevalue 0 {
    global GDBSrcBar_state
    ::set GDBSrcBar_state($this) $updatevalue
  }

  # This holds the text that is shown in the address label.
  public variable address {} {
    if {$ButtonFrame != "" && [winfo exists $ButtonFrame.addr]} {
      $ButtonFrame.addr configure -text $address -font src-font
    }
  }

  # This holds the text that is shown in the line label.
  public variable line {} {
    if {$ButtonFrame != "" && [winfo exists $ButtonFrame.line]} {
      $ButtonFrame.line configure -text $line
    }
  }

  # This holds the source window's display mode.  Valid values are
  # SOURCE, ASSEMBLY, SRC+ASM, and MIXED.
  public variable displaymode SOURCE {
    if {$ButtonFrame != ""} {
      _set_stepi
    }
  }

  # This is true if the inferior is running, or false if it is
  # stopped.
  public variable runstop normal {
    if {$ButtonFrame != ""} {
	_set_runstop
    }
  }

  # The next three determine the state of the application when Tracing is enabled.

  public variable Tracing 0     ;# Is tracing enabled for this gdb?
  public variable Browsing   0  ;# Are we currently browsing a trace experiment?
  public variable Collecting 0  ;# Are we currently collecting a trace experiment?

  # ------------------------------------------------------------------
  #  METHOD:  create_menu_items - Add some menu items to the menubar.
  #                               Returns 1 if any items added.
  #  This overrides the method in GDBToolBar.
  # ------------------------------------------------------------------
  public method create_menu_items {} {
    global enable_external_editor tcl_platform

    set m [new_menu file "File" 0]

    if {[info exists enable_external_editor] && $enable_external_editor} {
      add_menu_command None "Edit Source" \
	[list $this _apply_source edit]
    }
    add_menu_command Other "Open..."  \
      "_open_file" -underline 0 -accelerator "Ctrl+O"

    if {$tcl_platform(platform) == "windows"} {
      add_menu_command None "Print Source..." \
	"$this _apply_source print" \
	-underline 0 -accelerator "Ctrl+P"
      add_menu_command None "Page Setup..." \
	[format {
	  set top %s
	  ide_winprint page_setup -parent $top
	} [winfo toplevel [namespace tail $this]]] \
	-underline 8
    }
    
    add_menu_command Other "Source..." \
      "source_file" -underline 0

    add_menu_separator

    add_menu_command Other "Target Settings..." \
      {set_target_name ""} -underline 0
    add_menu_separator
    add_menu_command None "Exit" gdbtk_quit -underline 1
    
    create_run_menu

    create_view_menu

    if {[pref get gdb/control_target]} {
      create_control_menu

    }

    if {[pref get gdb/mode]} {
      create_trace_menu
    }

    new_menu pref "Preferences" 0
    
    add_menu_command Other "Global..." \
      "ManagedWin::open GlobalPref -transient" -underline 0
    
    add_menu_command Other "Source..." \
      "ManagedWin::open SrcPref -transient" -underline 0
    
    create_help_menu
    return 1
  }
  
  # ------------------------------------------------------------------
  #  METHOD:  create_buttons - Add some buttons to the toolbar.  Returns
  #                         list of buttons in form acceptable to
  #                         standard_toolbar.
  #  This overrides the method in GDBToolBar.
  # ------------------------------------------------------------------
  public method create_buttons {} {
    global enable_external_editor

    add_button stop None {} {}
    _set_runstop

    if {[pref get gdb/mode]} {
      add_button tstop Control [list $this do_tstop] "Start Collection" \
	-image Movie_on_img

      add_button view Other [list $this set_control_mode 1] \
	"Switch to Browse Mode" -image watch_movie_img

      add_button_separator

    }

    if {[pref get gdb/control_target]} {
      create_control_buttons
      if {[pref get gdb/mode]} {
	create_trace_buttons 0
      }
    } elseif {[get pref gdb/mode]} {

      #
      # If we don't control the target, then we might as well
      # put a copy of the trace controls on the source window.
      #
      create_trace_buttons 1
   }

    add_button_separator

    create_window_buttons

    # Random bits of obscurity...
    bind $Buttons(reg)   <Button-3> "ManagedWin::open RegWin -force"
    bind $Buttons(mem)   <Button-3> "ManagedWin::open MemWin -force"
    bind $Buttons(watch) <Button-3> "ManagedWin::open WatchWin -force"
    bind $Buttons(vars)  <Button-3> "ManagedWin::open LocalsWin -force"

    add_button_separator

    if {[info exists enable_external_editor] && $enable_external_editor} {
      add_button edit Other [list $this _apply_source edit] "Edit Source" \
	-image edit_img

      add_button_separator
    }

    add_label addr $address "Address" -width 10 -relief sunken -bd 1 -anchor e \
      -font  src-font

    add_label line $line "Line Number" -width 6 -relief sunken -bd 1 -anchor e \
      -font  src-font

    button_right_justify

    create_stack_buttons

    # This feature has been disabled for now.
    # checkbutton $ButtonFrame.upd -command "$this _toggle_updates" \
    #   -variable GDBSrcBar_state($this)
    # lappend button_list $ButtonFrame.upd
    # global GDBSrcBar_state
    # ::set GDBSrcBar_state($this) $updatevalue
    # balloon register $ButtonFrame.upd "Toggle Window Updates"

  }

  # ------------------------------------------------------------------
  #  METHOD:  _toggle_updates - Run when the update checkbutton is
  #                             toggled.  Private method.
  # ------------------------------------------------------------------
  public method _toggle_updates {} {
    global GDBSrcBar_state
    if {$updatecommand != ""} {
      uplevel \#0 $updatecommand $GDBSrcBar_state($this)
    }
  }

  # ------------------------------------------------------------------
  #  METHOD:  cancel_download
  # ------------------------------------------------------------------
  public method cancel_download {} {
    global download_dialog download_cancel_ok

    if {"$download_dialog" != ""} {
      $download_dialog cancel
    } else {
      set download_cancel_ok 1
    }
  }

  # ------------------------------------------------------------------
  #  METHOD:  create_run_menu - Creates the standard run menu
  # ------------------------------------------------------------------
  
  method create_run_menu {} {
    set run_menu [new_menu run "Run" 0]

    add_menu_command Other "Connect to target" \
      "$this do_connect $run_menu" -underline 0

    if {[pref get gdb/control_target]} {
      add_menu_command Other "Download" Download::download_it \
	-underline 0 -accelerator "Ctrl+D"
      add_menu_command Other "Run" [code $source inferior run] -underline 0 \
	-accelerator R
    }

    add_menu_command Other "Disconnect"  "$this do_disconnect $run_menu" \
      -underline 0 -state disabled


    if { [pref get gdb/mode] } {
      add_menu_separator 
      add_menu_command Other "Start collection" "$this do_tstop" \
	-underline 0 -accelerator "Ctrl+B"
         
      add_menu_command Other "Stop collection" "$this do_tstop" \
	-underline 0  -accelerator "Ctrl+E" -state disabled
    }

  }

  # ------------------------------------------------------------------
  #  METHOD:  create_stack_buttons - Creates the up down bottom stack buttons
  # ------------------------------------------------------------------
  
  method create_stack_buttons {} {

    add_button down {Trace Control} [list $this _apply_source stack down] \
      "Down Stack Frame" -image down_img

    add_button up {Trace Control} [list $this _apply_source stack up] \
      "Up Stack Frame" -image up_img

    add_button bottom {Trace Control} [list $this _apply_source stack bottom] \
      "Go to Bottom of Stack" -image bottom_img

  }

  # ------------------------------------------------------------------
  #  METHOD:  _set_runstop - Set state of run/stop button.
  # ------------------------------------------------------------------
  public method _set_runstop {} {
    switch $runstop {
      busy {
	$ButtonFrame.stop configure -state disabled
      }
      downloading {
	$ButtonFrame.stop configure -state normal -image stop_img \
	  -command [code $this cancel_download]
	balloon register $ButtonFrame.stop "Stop"
      }
      running {
	$ButtonFrame.stop configure -state normal -image stop_img \
	  -command "catch {gdb_stop}"
	balloon register $ButtonFrame.stop "Stop"
	
      }
      normal {
	$ButtonFrame.stop configure -state normal -image run_img \
	  -command run_executable
	balloon register $ButtonFrame.stop "Run (R)"
      }
      default {
	debug "SrcBar::_set_runstop - unknown state $runstop ($running)"
      }
    }
  }


  # ------------------------------------------------------------------
  #  METHOD:  _set_stepi - Set state of stepi/nexti buttons.
  # ------------------------------------------------------------------
  public method _set_stepi {} {
    
    # Only do this in synchronous mode
    if {!$Tracing} {
      # In source-only mode, disable these buttons.  Otherwise, enable
      # them.
      if {$displaymode == "SOURCE"} {
	set state disabled
      } else {
	set state normal
      }
      $ButtonFrame.stepi configure -state $state
      $ButtonFrame.nexti configure -state $state
    }
  }

  # ------------------------------------------------------------------
  #  METHOD:  _apply_source - Forward some method call to the source window.
  # ------------------------------------------------------------------
  public method _apply_source {args} {
    if {$source != ""} {
      eval $source $args
    }
  }

  # ------------------------------------------------------------------
  # METHOD:  enable_ui - enable/disable the appropriate buttons and menus
  # Called from the busy, idle, and no_inferior hooks.
  #
  # on must be:
  # value      Control    Other    Trace    State
  #   0          off       off      off     gdb is busy
  #   1          on        on       off     gdb has inferior, and is idle
  #   2          off       on       off     gdb has no inferior, and is idle
  # ------------------------------------------------------------------
  public method enable_ui {on} {
    global tcl_platform

    # Do the enabling so that all the disabling happens first, this way if a
    # button belongs to two groups, enabling takes precedence, which is probably right.

    switch $on {
      0 {
	set enable_list {Control disabled Other disabled Trace disabled}
      }
      1 {
	if {!$Browsing} {
	  set enable_list {Trace disabled Control normal Other normal}
	  set ostate normal
	  set cstate normal
	  set tstate disabled
	  # set the states of stepi and nexti correctly
	  _set_stepi
	} else {
	  set enable_list {Control disabled Other normal Trace normal}
	}

      }
      2 {
	set enable_list {Control disabled Trace disabled Other normal}
	set ostate normal
	set cstate disabled
	set tstate disabled
      }
      default {
	debug "Unknown type: $on in enable_ui"
	return
      }
    }

    foreach {type state} $enable_list {
      foreach button [set ${type}Buttons] {
	$button configure -state $state
      }
      eval change_menu_state \$${type}Menus \$state
    }

  }

  # ------------------------------------------------------------------
  #  METHOD:  trace_find_hook - response to the tfind command.  If the
  #  command puts us in a new mode, then switch modes...
  # ------------------------------------------------------------------
  method trace_find_hook {mode from_tty} {
    debug "in trace_find_hook, mode: $mode, from_tty: $from_tty, Browsing: $Browsing"
    if {[string compare $mode -1] == 0} {
      if {$Browsing} {
	set_control_mode 0
      }
    } else {
      if {!$Browsing} {
	set_control_mode 1
      }
    }
  }
  # ------------------------------------------------------------------
  #  METHOD:  set_control_mode - sets up the srcbar for browsing 
  #  a trace experiment.
  #   mode: 1 => browse mode
  #         0 => control mode
  # ------------------------------------------------------------------
  method set_control_mode  {mode} {
    debug "set_control_mode called with mode $mode"
    if {$mode} {
      set Browsing 1
      $Buttons(view) configure -image run_expt_img -command "$this set_control_mode 0"
      balloon register $Buttons(view) "Switch to Control mode"
      # Now swap out the buttons...
      swap_button_lists $Trace_control_buttons $Run_control_buttons
      enable_ui 1
    } else {
      if {$Browsing} {
	tfind_cmd {tfind none}
      }
      set Browsing 0
      $Buttons(view) configure -image watch_movie_img -command "$this set_control_mode 1"
      balloon register $Buttons(view) "Switch to Browse mode"
      # Now swap out the buttons...
      swap_button_lists $Run_control_buttons $Trace_control_buttons
      enable_ui 1
    }
    run_hooks control_mode_hook $Browsing
  }


  # ------------------------------------------------------------------
  #  METHOD:  reconfig - reconfigure the srcbar
  # ------------------------------------------------------------------
  public method reconfig {} {
    _load_src_images 1
    GDBToolBar::reconfig
  }

  # ------------------------------------------------------------------
  # METHOD:  do_connect: connect to a remote target 
  #                           in asynch mode if async is 1.
  #                      
  # ------------------------------------------------------------------
  method do_connect {menu {async 0}} {
    debug "$menu $async"

    set result [connect $async]

    if {$result} {
	$menu entryconfigure "Connect to target" -state disabled
	$menu entryconfigure "Disconnect" -state normal
      }
  }


  # ------------------------------------------------------------------
  # METHOD:  do_disconnect: disconnect from a remote target 
  #                               in asynch mode if async is 1.   
  #   
  # ------------------------------------------------------------------
  method do_disconnect {menu {async 0}} {
    debug "$menu $async"
    #
    # For now, these are the same, but they might be different...
    # 

    disconnect $async

    $menu entryconfigure "Connect to target" -state normal
    $menu entryconfigure "Disconnect" -state disabled
  }

  # ------------------------------------------------------------------
  # METHOD:  do_tstop: Change the GUI state, then do the tstop or
  #                    tstart command, whichever is appropriate.   
  #   
  # ------------------------------------------------------------------
  method do_tstop {} {
    debug "do_tstop called... Collecting is $Collecting"

    if {!$Collecting} {
      #
      # Start the trace experiment
      #

      if {$Browsing} {
	set ret [tk_MessageBox -title "Warning" -message \
"You are currently browsing a trace experiment. 
This command will clear the results of that experiment.
Do you want to continue?" \
		   -icon warning -type okcancel -default ok]
	if {[string compare $ret cancel] == 0} {
	  return
	}
	set_control_mode 1
      }
      if {[tstart]} {
	$Buttons(tstop) configure -image Movie_off_img
	balloon register $Buttons(tstop) "End Collection"
	set Collecting 1
      } else {
	tk_messageBox -title Error -message "Error downloading tracepoint info" \
	  -icon error -type ok
      }
    } else {
      #
      # Stop the trace experiment
      #

      if {[tstop]} {	
	$Buttons(tstop) configure -image Movie_on_img
	balloon register $Buttons(tstop) "Start Collection"
	set Collecting 0
     }
    }
  }
  
  #
  #  PROTECTED DATA
  #
  common menu_titles
}
