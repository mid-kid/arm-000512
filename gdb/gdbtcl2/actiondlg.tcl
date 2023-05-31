#
# ActionDlg
#

itcl_class ActionDlg {
  # ------------------------------------------------------------------
  # CONSTRUCTOR
  # ------------------------------------------------------------------
  constructor {config} {
    global _TStepCount _TOtherVariable

    set class [$this info class]
    set hull [namespace tail $this]
    set old_name $this
    ::rename $this $this-tmp-
    ::frame $hull -class $class
    ::rename $hull $old_name-win-
    ::rename $this $old_name
    
    set top [winfo toplevel [namespace tail $this]]
    wm withdraw $top

    set Registers [gdb_regnames]
    if {$Line != ""} {
      set Locals  [gdb_get_locals "$File:$Line"]
      set Args    [gdb_get_args "$File:$Line"]
    } else {
      set Locals  [gdb_get_locals "*$Address"]
      set Args    [gdb_get_args "*$Address"]
    }
    set Variables [concat $Locals $Args]
    foreach a $Registers {
      lappend Variables "\$$a"
    }
    
    if {[llength $Args] > 0} {
      lappend Variables "All Arguments"
    }
    if {[llength $Locals] > 0} {
      lappend Variables  "All Locals" 
    } 
    lappend Variables "All Registers"
    lappend Variables "Collect Stack"

    build_win $this    

    # Set a default return status, in case we are destroyed
    set _TOtherVariable {}

    # Fill the listboxes with any default data
    if {"$Data" != {}} {
      change 1 $Data
    }

    after idle [list wm deiconify $top]
    # after idle grab $this
  }

  # ------------------------------------------------------------------
  #  DESTRUCTOR - destroy window containing widget
  # ------------------------------------------------------------------
  destructor {

    # Remove this window and all hooks
    # grab release $this

    # Note that this is okay: the callback (TraceDlg::done, usually) will
    # ignore stray "cancel" callbacks
    eval $Callback cancel

    set top [winfo toplevel [namespace tail $this]]
    destroy $this
    destroy $top
  }

  # ------------------------------------------------------------------
  # METHOD: build_win - build the Trace dialog box (cache this?)
  # ------------------------------------------------------------------
  method build_win {f} {
    global _TStepCount _TOtherVariable

    # The two frames of this dialog
    set bbox [frame $f.bbox];            # for holding OK,CANCEL buttons
    set data [frame $f.data];            # for everything else

    # Setup the button box
    button $bbox.ok     -text OK -command "$this ok"
    button $bbox.cancel -text CANCEL -command "$this cancel"
    pack $bbox.ok $bbox.cancel -side left -padx 10 -expand yes

    # The "Data Collection" Frame
    set top [frame $data.top]
    set bot [frame $data.bot]

    set boxes  [frame $top.boxes]
    set cFrame [frame $boxes.cFrame]
    set vFrame [frame $boxes.vFrame]
    set bFrame [frame $boxes.bframe]
    set oFrame [frame $top.uFrame]
    pack $cFrame $bFrame $vFrame -side left -expand yes -padx 5

    # While stepping
    if {$WhileStepping} {
      set step_frame [frame $top.stepf]
      label $step_frame.whilelbl -text {While Stepping,   Steps:}
      set WhileSteppingEntry [entry $step_frame.steps          \
				-textvariable _TStepCount      \
				-width 5]
      pack $step_frame.whilelbl $WhileSteppingEntry -side left  
    }

    # The Collect listbox
    label $cFrame.lbl -text {Collect:}
    tixScrolledListBox $cFrame.lb -scrollbar auto \
      -browsecmd "$this toggle_button_state 0"    \
      -command "$this change 0"
    set CollectLB [$cFrame.lb subwidget listbox]
    $CollectLB configure -selectmode extended
    pack $cFrame.lbl $cFrame.lb -side top -expand yes -pady 2

    # The Variables listbox
    label $vFrame.lbl -text {Variables:}
    tixScrolledListBox $vFrame.lb -scrollbar auto \
      -browsecmd "$this toggle_button_state 1"    \
      -command "$this change 1"
    set VariablesLB [$vFrame.lb subwidget listbox]
    $VariablesLB configure -selectmode extended
    pack $vFrame.lbl $vFrame.lb -side top -expand yes -pady 2

    # The button frame
    set AddButton [button $bFrame.add -text {<<< Collect}   \
		     -command "$this change 1" -state disabled]
    set RemoveButton [button $bFrame.del -text {Ignore >>>} \
			-command "$this change 0" -state disabled]
    pack $bFrame.add $bFrame.del -side top -expand yes -pady 5

    # The other frame (type-in)
    label $oFrame.lbl -text {Other:}
    set OtherEntry [entry $oFrame.ent -textvariable _TOtherVariable]
    pack $oFrame.lbl $OtherEntry -side left
    bind $OtherEntry <Return> "$this change_other"

    # Pack these frames
    if {$WhileStepping} {
      pack $step_frame -side top
    }

    pack $boxes $oFrame -side top -padx 5 -pady 5
    pack $top $bot -side top

    # Fill the list boxes
    fill_listboxes

    # Pack the main frames
    # after idle
    pack $f.data $bbox -side top -padx 4 -pady 2 \
      -expand yes -fill x
    
    # !!???
    if {$WhileStepping} {
      $WhileSteppingEntry delete 0 end
      $WhileSteppingEntry insert 0 $Steps
    }
  }

  method toggle_button_state {add} {

    # BUG in Tix.. This is invoked whenever a <1> event is generated in
    # the listbox...
    if {$add} {
      set a [$VariablesLB curselection]
      if {"$a" != ""} {
	$AddButton configure -state normal
	$RemoveButton configure -state disabled
      }
    } else {
      set a [$CollectLB curselection]
      if {"$a" != ""} {
	$AddButton configure -state disabled
	$RemoveButton configure -state normal
      }
    }
  }


  # ------------------------------------------------------------------
  # METHOD: fill_listboxes - fills the two listboxes
  # ------------------------------------------------------------------
  method fill_listboxes {{last {}}} {

    # Fill the Collect listbox with the variables being collected
    if {[info exists Collect]} {
      fill_collect $last
    }

    fill_variables $last
  }      

  # ------------------------------------------------------------------
  # METHOD: change - change a selected variable
  # ------------------------------------------------------------------
  method change {add {select {}}} {
    if {"$select" == {}} {
      set selections [get_selections $add]
      set lb        [lindex $selections 0]
      set last      [lindex $selections 1]
      set selection [lindex $selections 2]
      set noname 1
    } else {
      # This usually (only) occurs when we open this dialog for editing
      # some existing action.
      set lb   {}
      set last {}
      set noname 0
      set selection $select
    }
    
    $RemoveButton configure -state disabled
    $AddButton configure -state disabled

    # Remove all the selections from one list
    # and add them to the other list
    if {$add} {
      set list1 $Variables
      set list2 $Collect
    } else {
      set list1 $Collect
      set list2 $Variables
    }

    foreach a $selection {
      if {$noname} {
	set name [$lb get $a]
      } else {
	set name $a
      }

      if {"$name" == "All Locals" || "$name" == {$loc}} {
	set name "All Locals"
	set lists [all_locals $add]
	set list1 [lindex $lists 0]
	set list2 [lindex $lists 1]
      } elseif {"$name" == "All Registers" || "$name" == {$reg}} {
	set name "All Registers"
	set lists [all_regs $add]
	set list1 [lindex $lists 0]
	set list2 [lindex $lists 1]
      } elseif {"$name" == "All Arguments" || "$name" == {$arg}} {
	set name "All Arguments"
	set lists [all_args $add]
	set list1 [lindex $lists 0]
	set list2 [lindex $lists 1]
      } else {
	set i [lsearch -exact $list1 $name]
	set list1 [lreplace $list1 $i $i]

	# Check if this is something we want to keep on a list
	if {[lsearch $Args $name] != -1 || [lsearch $Registers [string trim $name \$]] != -1 || [lsearch $Locals $name] != -1 || $add} {
	  lappend list2 $name
	}
      }

      if {$add} {
	set Collect $list2
	set Variables $list1
      } else {
	set Collect $list1
	set Variables $list2
      }
    }

    # Update boxes (!! SLOW !!)
    fill_collect $last
    fill_variables $last
  }

  # ------------------------------------------------------------------
  # METHOD: fill_collect - fill the collect box
  # ------------------------------------------------------------------
  method fill_collect {{last {}}} {

    $CollectLB delete 0 end
    set Collect [sort $Collect]
    foreach a $Collect {
      $CollectLB insert end $a
    }
    if {"$last" != ""} {
      $CollectLB see $last
    }
  }

  # ------------------------------------------------------------------
  # METHOD: fill_variables - fill the variables box
  # ------------------------------------------------------------------
  method fill_variables {{last {}}} {

    $VariablesLB delete 0 end
    set Variables [sort $Variables]
    foreach a $Variables {
      $VariablesLB insert end $a
    }

    if {"$last" != ""} {
      $VariablesLB see $last
    }
  }

  # ------------------------------------------------------------------
  # METHOD: sort - sort a list of variables, placing regs and
  #                special identifiers (like "All Locals") at end
  # ------------------------------------------------------------------
  method sort {list} {
    
    set special_names {
      "All Arguments" args \
	"All Locals" locs \
	"All Registers" regs \
	"Collect Stack" stack
    }

    foreach {name var} $special_names {
      set i [lsearch $list $name]
      if {$i != -1} {
	set $var 1
	set list [lreplace $list $i $i]
      } else {
	set $var 0
      }
    }

    # Extract all the locals, regs, args, globals
    set types_list {Args Locals Registers } 
    foreach type $types_list {
      set used_$type {}

      foreach a [set $type] {
	set i [lsearch $list $a]
	if {$i != -1} {
	  lappend used_$type $a
	  set list [lreplace $list $i $i]
	}
      }
      set used_$type [lsort [set used_$type]]
    }

    set globals [lsort $list]

    # Sort the remaining list in order: args, locals, globals, regs
    set list [concat $used_Args $used_Locals $globals $used_Registers]

    set list2 {}

    foreach {name var} $special_names {
      if {[set $var]} {
	lappend list2 $name
      }
    }

    set list [concat $list2 $list]
    return $list
  }
  
  # ------------------------------------------------------------------
  # METHOD: all_args - add/remove all args
  # ------------------------------------------------------------------
  method all_args {add} {

    if {$add} {
      set list1 $Variables
      set list2 $Collect
    } else {
      set list1 $Collect
      set list2 $Variables
    }

#    foreach var $Args {
#      set i [lsearch $list1 $var]
#      if {$i != -1} {
#	set list1 [lreplace $list1 $i $i]
#	lappend list2 $var
#      }
#    }

    lappend list2 "All Arguments"
    set i [lsearch $list1 "All Arguments"]
    if {$i != -1} {
      set list1 [lreplace $list1 $i $i]
    }

    return [list $list1 $list2]
  }

  # ------------------------------------------------------------------
  # METHOD: all_locals - add/remove all locals
  # ------------------------------------------------------------------
  method all_locals {add} {

    if {$add} {
      set list1 $Variables
      set list2 $Collect
    } else {
      set list1 $Collect
      set list2 $Variables
    }

#    foreach var $Locals {
#      set i [lsearch $list1 $var]
#      if {$i != -1} {
#	set list1 [lreplace $list1 $i $i]
#	lappend list2 $var
#      }
#    }

    lappend list2 "All Locals"
    set i [lsearch $list1 "All Locals"]
    if {$i != -1} {
      set list1 [lreplace $list1 $i $i]
    }

    return [list $list1 $list2]
  }

  # ------------------------------------------------------------------
  # METHOD: all_regs - add/remove all registers
  # ------------------------------------------------------------------
  method all_regs {add} {

    if {$add} {
      set list1 $Variables
      set list2 $Collect
    } else {
      set list1 $Collect
      set list2 $Variables
    }

#    foreach var $Registers {
#      set i [lsearch $list1 "\$$var"]
#      if {$i != -1} {
#	set list1 [lreplace $list1 $i $i]
#	lappend list2 "\$$var"
#      }
#    }

    lappend list2 "All Registers"
    set i [lsearch $list1 "All Registers"]
    if {$i != -1} {
      set list1 [lreplace $list1 $i $i]
    }

    return [list $list1 $list2]
  }

  # ------------------------------------------------------------------
  # METHOD: change_other - add/remove a user defined type
  # ------------------------------------------------------------------
  method change_other {} {
    set other [$OtherEntry get]
    
    if {"$other" != ""} {
      set added 0

      # Check if this is a local/register/arg
      set i [lsearch $Locals "$other"]
      if {$i != -1} {
	set i [lsearch $Collect "$other"]
	set added 1
	if {$i != -1} {
	  # It's a local on the collection list
	  debug "local on collection list"
	  set add 0
	  set list1 [lreplace $Collect $i $i]
	  set list2 [concat $Variables "$other"]
	} else {
	  # It's a local on the variables list
	  debug "local on variable list"
	  set add 1
	  set i [lsearch $Variables "$other"]
	  set list1 [lreplace $Variables $i $i]
	  set list2 [concat $Collect "$other"]
	}
      }

      set i [lsearch $Registers [string trim "$other" \$]]
      if {$i != -1} {
	set i [lsearch $Collect "$other"]
	set added 1
	if {$i != -1} {
	  # It's a register on the collection list
	  debug "register on collection list"
	  set add 0
	  set list1 [lreplace $Collect $i $i]
	  set list2 [concat $Variables "$other"]
	} else {
	  # It's a register on the variables list
	  debug "register on variable list"
	  set add 1
	  set i [lsearch $Variables "$other"]
	  set list1 [lreplace $Variables $i $i]
	  set list2 [concat $Collect "$other"]
	}
      }

      set i [lsearch $Args $other]
      if {$i != -1} {
	set i [lsearch $Collect "$other"]
	set added 1
	if {$i != -1} {
	  # It's an arg on the collection list
	  debug "arg on collection list"
	  set add 0
	  set list1 [lreplace $Collect $i $i]
	  set list2 [concat $Variables "$other"]
	} else {
	  # It's an arg on the variables list
	  debug "arg on variable list"
	  set add 1
	  set i [lsearch $Variables "$other"]
	  set list1 [lreplace $Variables $i $i]
	  set list2 [concat $Collect "$other"]
	}
      }
      
      # Check for special tags
      if {!$added} {
	if {"[string tolower $other]" == "all locals"} {
	  set i [lsearch $Variables "All Locals"]
	  if {$i != -1} {
	    # It's "All Locals" on the variables list
	    set add 1
	    set lists [all_locals 1]
	    set list1 [lindex $lists 0]
	    set list2   [lindex $lists 1]
	  } else {
	    # It's "All Locals" on the Collect list
	    set add 0
	    set lists [all_locals 0]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  }
	} elseif {"[string tolower $other]" == "all registers"} {
	  set i [lsearch $Variables "All Registers"]
	  if {$i != -1} {
	    # It's "All Registers" on the Variables list
	    set add 1
	    set lists [all_regs 1]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  } else {
	    set add 0
	    set lists [all_regs 0]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  }
	} elseif {"[string tolower $other]" == "all arguments"} {
	  set i [lsearch $Variables "All Arguments"]
	  if {$i != -1} {
	    # It's "All Arguments" on the Variables list
	    set add 1
	    set lists [all_args 1]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  } else {
	    set add 0
	    set lists [all_args 0]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  }
	} elseif {"[string tolower $other]" == "collect stack"} {
	  set i [lsearch $Variables "Collect Stack"]
	  if {$i != -1} {
	    # It's "All Arguments" on the Variables list
	    set add 1
	    set lists [all_args 1]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  } else {
	    set add 0
	    set lists [all_args 0]
	    set list1 [lindex $lists 0]
	    set list2 [lindex $lists 1]
	  }
	} else {
	  # Check if this entry is on the Collect list
	  set i [lsearch $Collect $other]
	  if {$i != -1} {
	    # It's on the list -- remove it
	    set add 0
	    set list1 [lreplace $Collect $i $i]
	    set list2 $Variables
	  } else {
	    # It's not on the list -- add it

	    set other [string trim $other \ \r\t\n]

            # accept everything, send to gdb to validate
	    set ok 1

            # memranges will be rejected right here

	    if {[string range $other 0 1] == "\$("} {
              tk_messageBox -type ok -icon error \
                  -message "Expression syntax not supported"
              set ok 0
	    }
	      
            # do all syntax checking later
	     if {$ok} {
	      #debug "Keeping \"$other\""
	      # We MUST string out all spaces...
	      if {[regsub -all { } $other {} expression]} {
		set other $expression
	      }
	      set add 1
	      set list1 $Variables
	      set list2 [concat $Collect "$other"]
	    } else {
	      #debug "Discarding \"$other\""
	    }
	  }
	}
      }

      # Clear the entry
      $OtherEntry delete 0 end

      if {$add} {
	set Variables $list1
	set Collect $list2
      } else {
	set Variables $list2
	set Collect $list1
      }
      fill_listboxes
    }
  }


  # ------------------------------------------------------------------
  # METHOD: get_selections - get all the selected variables
  #         pass 0 to get the selections from the collect box
  #         Returns a list of: listbox in which the selections were
  #         obtained, last element selected on the list, and all the
  #         selected elements
  # ------------------------------------------------------------------
  method get_selections {vars} {
    
    if {$vars} {
      set widget $VariablesLB
    } else {
      set widget $CollectLB
    }

    set elements [$widget curselection]
    set list {}
    set i 0
    foreach i $elements {
      lappend list [$widget get $i]
    }

    return [list $widget $i $elements]
  }

  # ------------------------------------------------------------------
  # METHOD: cancel - cancel the dialog and do not set the trace
  # ------------------------------------------------------------------
  method cancel {} {
    delete
  }

  method remove_special {list items} {
    
    foreach item $items {
      set i [lsearch $list $item]
      if {$i != -1} {
	set list [lreplace $list $i $i]
      } else {
	set i [lsearch $list \$$item]
	if {$i != -1} {
	  set list [lreplace $list $i $i]
	}
      }
    }

    return $list
  }

  # ------------------------------------------------------------------
  # METHOD: ok - validate the tracepoint and install it
  # ------------------------------------------------------------------
  method ok {} {
    global _TStepCount

    # Add anything in the OtherEntry
    change_other

    # Check that we are collecting data
    if {[llength $Collect] == 0} {
      # No data!
      set msg "No data specified for the given action."
      set answer [tk_messageBox -type ok -title "Tracepoint Error" \
		    -icon error \
		    -message $msg]
      case $answer {
	cancel {
	  cancel
	}
	ok {
	  return
	}
      }
    }

    set i [lsearch $Collect "All Locals"]
    if {$i != -1} {
      set data [lreplace $Collect $i $i]
      set data [concat $data {$loc}]

      # Remove all the locals from the list
      set data [remove_special $data $Locals]
    } else {
      set data $Collect
    }

    set i [lsearch $data "All Registers"]
    if {$i != -1} {
      set data [lreplace $data $i $i]
      set data [concat $data {$reg}]

      # Remove all the locals from the list
      set data [remove_special $data $Registers]
    }

    set i [lsearch $data "All Arguments"]
    if {$i != -1} {
      set data [lreplace $data $i $i]
      set data [concat $data {$arg}]

      # Remove all the locals from the list
      set data [remove_special $data $Args]
    }

   set i [lsearch $data "Collect Stack"]
    if {$i != -1} {
      set data [lreplace $data $i $i]
      set data [concat $data [collect_stack]]

    }

    # Remove repeats
    set d {}
    foreach i $data {
      if {![info exists check($i)]} {
	set check($i) 1
	lappend d $i
      }
    }

    if {$WhileStepping} {
      set steps $_TStepCount
    } else {
      set steps 0
    }

    if {"$Data" != {}} {
      set command "modify"
    } else {
      set command "add"
    }

    debug "DATA = $data"
    eval $Callback $command $steps [list $data]
    delete
  }


  method collect_stack {} {
    return $StackCollect
  }
	  
  method cmd {line} {
    $line
  }

  # PUBLIC DATA
  public File
  public Line {}
  public WhileStepping 0
  public Number
  public Callback
  public Data {}
  public Steps {}
  public Address {}

  # PROTECTED DATA
  protected WhileSteppingEntry
  protected CollectLB
  protected VariablesLB
  protected Variables {}
  protected Collect {}
  protected Locals
  protected Args
  protected Registers
  protected Others {}
  protected AddButton
  protected RemoveButton
  protected OtherEntry
  protected StackCollect {*(char*)$sp@64}
}
