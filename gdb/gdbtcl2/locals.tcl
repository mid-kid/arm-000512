# local variable window
#   Copyright (C) 1997, 1998 Cygnus Solutions

class LocalsWin {
  inherit VariableWin

  # ------------------------------------------------------------------
  #  CONSTRUCTOR - create new locals window
  # ------------------------------------------------------------------
  constructor {args} {
    update
  }

  # ------------------------------------------------------------------
  # DESTRUCTOR - delete locals window
  # ------------------------------------------------------------------
  destructor {
  }

  method build_win {f} {
    global tcl_platform
    build_menu_helper Variable
    if {$tcl_platform(platform) == "windows"} {
      frame $f.f
      VariableWin::build_win $f.f
      pack $f.f -expand yes -fill both -side top
      frame $f.stat
      pack $f.stat -side bottom -fill x
    } else {
      VariableWin::build_win $f
    }
  }


  # ------------------------------------------------------------------
  # METHOD: reconfig
  # Overrides VarialbeWin::reconfig method.  Have to make sure the locals
  #  will get redrawn after everything is destroyed...
  # ------------------------------------------------------------------
  method reconfig {} {
    VariableWin::reconfig
    populate {}
  }

  # ------------------------------------------------------------------
  # METHOD: getVariablesBlankPath
  # Overrides VarialbeWin::getVariablesBlankPath. For a Locals Window,
  # this method returns a list of local variables.
  # ------------------------------------------------------------------
  method getVariablesBlankPath {} {
    global Update
    debug "LocalsWin::getVariablesBlankPath"
    set Locals [getLocals]

    foreach var $Locals {
      # Create a variable for each local
      set Variables($var) [gdb_variable create -expr $var -frame [gdb_selected_frame]]
      set Update($this,$Variables($var)) 1
      lappend vars $Variables($var)
    }

    return $vars
  }

  method update {} {
    global Update Display
    debug "LocalsWin::update"
    debug "START LOCALS UPDATE CALLBACK"
    # Check that a context switch has not occured
    debug "old=$Locals"
    if {[context_switch]} {
      debug "CONTEXT SWITCH"
      set Locals [getLocals]
      debug "new=$Locals"

      # our context has changed... repopulate with new variables
      # destroy the old tree and create a new one
      deleteTree
      set ChangeList {}
      
      if {$Locals != ""} {
	populate {}
      }
    }
    
    VariableWin::update
    debug "END LOCALS UPDATE CALLBACK"
  }     
}

