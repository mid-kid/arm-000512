#
# KodWin
# ----------------------------------------------------------------------
# Implements the Kernel Object Display Window.
#
# ----------------------------------------------------------------------
#   AUTHOR:  Fernando Nasser <fnasser@cygnus.com>
#   Copyright (C) 1998 Cygnus Solutions
#


itcl_class KodWin {
#  inherit EmbeddedWin GDBWin

  # ------------------------------------------------------------------
  #  CONSTRUCTOR - create new process window
  # ------------------------------------------------------------------
  constructor {config} {
    #
    #  Create a window with the same name as this object
    #
    global gdb_kod_cmd

    set class [$this info class]
    set hull [namespace tail $this]
    set old_name $this
    ::rename $this $this-tmp-
    ::frame $hull -class $class 
    ::rename $hull $old_name-win-
    ::rename $this $old_name

    set top [winfo toplevel $hull]
    wm withdraw $top
    wm title $top "Kernel Objects"

    # initialize local variables
    set LevelCmd(0) "info $gdb_kod_cmd "
    debug "Level 0 kod command is $LevelCmd(0)"

    gdbtk_busy

    build_win

    gdbtk_idle
    # Add hooks for this object
    add_hook gdb_update_hook [list $this update]
    add_hook gdb_busy_hook [list $this busy]
    add_hook gdb_idle_hook [list $this idle]
    after idle [list wm deiconify $top]
  }


  # ------------------------------------------------------------------
  #  METHOD:  build_win - build the main KOD window
  # ------------------------------------------------------------------
  method build_win {} {
    global tixOption tcl_platform

    debug "Will build KOD window"

    # for holding TOP, UP, Pane 1, Pane 2, CLEAR and DISPLAY buttons
    set bf [frame [namespace tail $this].bf];

    # Now a frame for what is being listed, headers and list
    tixLabelFrame [namespace tail $this].tf -label "No Kernel Objects Known"
    set titl [namespace tail $this].tf
    set lf [[namespace tail $this].tf subwidget frame]

    # A paned window for details of objects
    # with two LabelFrames, each of these with a ScrolledText
    tixPanedWindow [namespace tail $this].pw -orientation vertical -panerelief flat
    [namespace tail $this].pw add pane1
    [namespace tail $this].pw add pane2
    set p1 [[namespace tail $this].pw subwidget pane1]
    set p2 [[namespace tail $this].pw subwidget pane2]
    $p1 configure -height 120
    $p2 configure -height 120
    tixLabelFrame $p1.d1 -label "No Details"
    tixLabelFrame $p2.d2 -label "No Details"
    set d1 [$p1.d1 subwidget frame]
    set d2 [$p2.d2 subwidget frame]
    pack $p1.d1 $p2.d2 -side top -expand yes -fill both
    set pl1 $p1.d1
    set pl2 $p2.d2

    # Setup the button box
    set BTop [button $bf.top -height 1 -text TOP -command "$this top"]
    set BUp  [button $bf.up   -height 1   -text UP -command "$this up"]
    set BClear [button $bf.clear  -height 1 -text CLEAR -command "$this clear"]
    set BDisplay [button $bf.display -height 1 -text DISPLAY \
		-command "$this display"]
    uplevel #0 {set kodActivePane "none"}
    set BPane1 [radiobutton $bf.pane1 -variable kodActivePane \
		-height 1 -text "Pane 1" -value pane1]
    set BPane2 [radiobutton $bf.pane2 -variable kodActivePane \
		-height 1 -text "Pane 2" -value pane2]
    balloon register $bf.top "Return to List of Kernel Objects"
    balloon register $bf.up "Return to previous List of Objects"
    balloon register $bf.clear "Clear Object Detail Panes\nand Active setting"
    balloon register $bf.display \
			"Display Object or\nList of Objects of this type"
    balloon register $bf.pane1 "Make Pane 1 Active"
    balloon register $bf.pane2 "Make Pane 2 Active"
    pack $bf.top $bf.up -side left -padx 5
    pack $bf.display $bf.clear -side right -padx 5
    pack $bf.pane2 $bf.pane1 -side bottom -padx 5 -fill both

    # Headers line
    set labh [label $lf.h -height 1 -bd 2 -relief groove \
		-anchor w -text "Loading..."]
    pack $lf.h -side top -padx 3 -pady 2 -fill x

    # The list of objects
    if {$tcl_platform(platform) == "windows"} {
      tixScrolledListBox $lf.s -scrollbar both -sizebox 1
    } else {
      tixScrolledListBox $lf.s -scrollbar auto
    }
    $lf.s configure -command "$this display"
    set lb [$lf.s subwidget listbox]
    $lb configure -selectmode single -height 8 \
      -bg $tixOption(input1_bg) \
      -selectbackground green \
      -selectforeground black \
      -font src-font
    balloon register $lb "Click on a line to choose an object and press DISPLAY"
    pack $lf.s -side left -expand yes -fill both

    # Areas to display object details
    if {$tcl_platform(platform) == "windows"} {
      tixScrolledText $d1.t1 -scrollbar both -sizebox 1
      tixScrolledText $d2.t2 -scrollbar both -sizebox 1
    } else {
      tixScrolledText $d1.t1 -scrollbar auto
      tixScrolledText $d2.t2 -scrollbar auto
    }
    set t1 [$d1.t1 subwidget text]
    set t2 [$d2.t2 subwidget text]
    $t1 configure -height 5 -bg $tixOption(input1_bg) \
		-state disabled -font src-font
    $t2 configure -height 5 -bg $tixOption(input1_bg) \
		-state disabled -font src-font
    balloon register $t1 "Pane 1"
    balloon register $t2 "Pane 2"
    pack $d1.t1 $d2.t2 -side top -expand yes -fill both

debug "Will pack KOD window"
    pack [namespace tail $this].tf -side top -expand yes -fill both
    pack [namespace tail $this].bf -side top -expand no -fill x
    pack [namespace tail $this].pw -side bottom -expand yes -fill both
    wm minsize $top 450 500

    # Initialize button state variables for idle (called before update)
    set BState(BDisplay) disabled
    set BState(BClear) disabled
    set BState(BTop) disabled
    set BState(BUp) disabled

#    window_name "Kernel Objects"

    update
  }


  # ------------------------------------------------------------------
  #  METHOD:  update - update widget when something changes
  # ------------------------------------------------------------------
  method update {} {

    debug "updating kod window"

    _disable_buttons

    display_list
    upvar #0 kodActivePane curpane
    if {$curpane != "none"} {
      display_object
    }

    _restore_buttons

  }

  # ------------------------------------------------------------------
  #  METHOD:  display - update the display based on the selection
  #           it can be a list or an actual object
  #           We get here from a press on the DISPLAY button of
  #           from a <Double-1> on a line of the list of objects
  # ------------------------------------------------------------------
  method display {} {
    if {!$Running && [$lb size] != 0} {
      gdbtk_busy
      set linenum [$lb curselection]
      set object [$lb get $linenum]
      debug "display selection on line $linenum $object"
      incr level
      set LevelCmd($level) $LevelCmd([expr $level-1])
      append LevelCmd($level) \
	[string range $object 0 [string first " " $object]]
      debug "kod command for level $level is now: $LevelCmd($level)"
      update
      # Run idle hooks and cause all other widgets to update
      gdbtk_idle
    }
  }

  # ------------------------------------------------------------------
  #  METHOD:  display_list - display list of objects
  #           If it is not a list but details call display_details
  #           (this never happens from the update hook, only from
  #            the DISPLAY button)
  # ------------------------------------------------------------------
  method display_list {} {

    debug "displaying list of objects"

    set cmd $LevelCmd($level)
    debug "new kod command is $cmd"
    if {[catch "gdb_cmd \"$cmd\"" objects]} {
      # failed.  leave window blank
      $titl configure -label "Kernel Object Display Failed"
      $labh configure -text $objects
      $lb delete 0 end
      set BState(BDisplay) disabled
      return
    }

    debug "KodWin update: \n$objects"
    if {[llength $objects] == 0} {
      $titl configure -label "No Kernel Objects Known"
      # no objects listed.
      $lb delete 0 end
      $labh configure -text " "
      set BState(BDisplay) disabled
      return
    }

    # insert each line one at a time
    set num_objects 0
    set num_lines 0
    foreach line [split $objects \n] {
      switch $num_lines {
	0 {
	    if {![string match List* $line]} {
	      if {($level > 0)} {
		display_object $cmd objects
                incr level -1
		return
              } else {
                # if level 0 first line does not start with List ignore it
	        $titl configure -label "List of Kernel Objects"
              }
	    } else {
	      $titl configure -label $line
	    }
	    # clear listbox and headers to get new stuff
	    $lb delete 0 end
	    $labh configure -text " "
	  }
	1 {
	    # set up headers
	    $labh configure -text $line
          }
        default {
	    if {$line == ""} {break} else {
	      $lb insert end $line
	      incr num_objects
	    }
          }
	}
      incr num_lines
    }

    if {$num_objects > 0} {
      set BState(BDisplay) active
    }
    
    if {$level == 0} {
      set BState(BTop) disabled
      set BState(BUp) disabled
    } else {
      set BState(BTop) active
      set BState(BUp) active
    }
      
    # highlight the active object (the first listed)
    $lb selection set 0
    $lb see 0
  }

  # ------------------------------------------------------------------
  #  METHOD:  display_object - display information about an object
  #           When called from update we have to reissue the gdb
  #           command to get fresh data
  # ------------------------------------------------------------------
  method display_object {{cmd ""} {obj ""}} {
    debug  "Displaying object details..."
    upvar $obj objects
    upvar #0 kodActivePane active
    debug "Active Pane is $active"

    # Determine which frame to use
    if {$active == "pane2"} {
      set curpan $t2
      set curlab $pl2
      if {$cmd != ""} {
        # save command for update
        set pane2command $cmd
      } else {
        # reuse saved command
        set cmd $pane2command
      }
    } else {
      if {$active == "none"} {
        uplevel #0 set kodActivePane pane1
      }
      set curpan $t1
      set curlab $pl1
      if {$cmd != ""} {
        # save command for update
        set pane1command $cmd
      } else {
        # reuse saved command
        set cmd $pane1command
      }
    }
    debug "curlab $curlab curpan $curpan"

    # here we must take care of the case where the user has activated a window
    # but it does not have been filled yet.  We just return.
    if {$cmd == ""} return

    if {$obj == ""} {
      debug "pane kod command is $cmd"
      if {[catch "gdb_cmd \"$cmd\"" objects] || \
          ($objects == "")} {
        # failed.  tell user object no longer there
        $curpan delete 1.0 end
        $curpan insert end "gdb command $cmd returned: $objects\n"
        $curpan insert end "Probably object no longer exists\n"
        uplevel #0 set kodActivePane "none"
        return
      }
    }

    set num_lin 0
    $curpan configure -state normal
    $curpan delete 1.0 end
    foreach line [split $objects \n] {
      if {$num_lin == 0} {
        $curlab configure -label "Details of Object: $line"
      } else {
        $curpan insert end $line
        $curpan insert end "\n"
      }
      incr num_lin
    }
    $curpan configure -state disabled
  }

  # ------------------------------------------------------------------
  #  METHOD:  clear - clear detail panes and reset pane selection
  # ------------------------------------------------------------------
  method clear {} {
    debug "going to clear detail panes and pane selection"
    $pl1 configure -label "No Details"
    $pl2 configure -label "No Details"
    $t1 configure -state normal
    $t2 configure -state normal
    $t1 delete 1.0 end
    $t2 delete 1.0 end
    $t1 configure -state disabled
    $t2 configure -state disabled
    # force both radiobuttons out
    uplevel #0 set kodActivePane "none"
    set pane1command ""
    set pane2command ""
  }

  # ------------------------------------------------------------------
  #  METHOD:  top - go to the list of types of objects (top level)
  # ------------------------------------------------------------------
  method top {} {
    debug "going to top from level $level"
    if {$level > 0} {
      set level 0
      update
    }
  }

  # ------------------------------------------------------------------
  #  METHOD:  up - go to the list of objects which led to the current one
  # ------------------------------------------------------------------
  method up {} {
    debug "going up from level $level..."
    if {$level > 0} {
      incr level -1
      debug "...to level $level"
      update
    }
  }

  # ------------------------------------------------------------------
  #  DESTRUCTOR - destroy window containing widget
  # ------------------------------------------------------------------
  destructor {
    remove_hook gdb_update_hook [list $this update]
    remove_hook gdb_idle_hook [list $this idle]
    remove_hook gdb_busy_hook [list $this busy]
    destroy  [namespace tail $this]
    destroy $top
  }

  # ------------------------------------------------------------------
  #  METHOD:  config - used to change public attributes
  # ------------------------------------------------------------------
  method config {config} {}
    
  # ------------------------------------------------------------------
  #  METHOD:  reconfig - used when preferences change
  # ------------------------------------------------------------------
  method reconfig {} {
    destroy [namespace tail $this].bf
    destroy $titl
    build_win
  }

  # ------------------------------------------------------------------
  #  METHOD:  busy - gdb_busy_hook
  #
  #        This method should accomplish blocking
  #        - clicks in the window
  #        - change mouse pointer
  # ------------------------------------------------------------------
  method busy {} {
    set Running 1
    _disable_buttons
    cursor watch
  }

  # ------------------------------------------------------------------
  #  METHOD:  idle - idle hook.  Run when the target is not running
  # ------------------------------------------------------------------
  method idle {} {
    set Running 0
    _restore_buttons
    cursor {}
  }

  # ------------------------------------------------------------------
  #  METHOD:  cursor - set the window cursor
  #        This is a convenience method which simply sets the mouse
  #        pointer to the given glyph.
  # ------------------------------------------------------------------
  method cursor {glyph} {
    $top configure -cursor $glyph
  }

  # ------------------------------------------------------------------
  #  PRIVATE METHOD:  _disable_buttons - disable all buttons
  #       Used when we are busy and can't take another event
  # ------------------------------------------------------------------
  method _disable_buttons {} {
    $BTop configure -state disabled
    $BUp configure -state disabled
    $BDisplay configure -state disabled
    $BClear configure -state disabled
  }

  # ------------------------------------------------------------------
  #  PRIVATE METHOD:  _restore_buttons - restore all buttons to their
  #       previous states.
  #       Used when we are busy and can't take another event
  # ------------------------------------------------------------------
  method _restore_buttons {} {
    $BTop configure -state $BState(BTop)
    $BUp configure -state $BState(BUp)
    $BDisplay configure -state $BState(BDisplay)
    # CLEAR is always active, except when busy
    $BClear configure -state active
  }

  #
  #  PROTECTED DATA
  #
  # top frame
  protected top
  # frame which contains buttons
  protected bf
  # frame which contains List of Objects and Headers (in a tixLabelFrame)
  protected lf
  # tixLabelFrame which contains List of Objects and Headers
  protected titl
  # the three buttons TOP, UP and DISPLAY
  protected BTop
  protected BUp
  protected BClear
  protected BDisplay
  # The listbox with the List of Objects
  protected lb
  # The label with the headers for object lists
  protected labh
  # Text areas for details of Objects
  protected t1
  protected t2
  protected pl1
  protected pl2
  # saved commands for displaying details of objects
  protected pane1command ""
  protected pane2command ""
  # Buttons for text areas
  protected BPane1
  protected BPane2
  # Which level the current list is (0 = info ecos)
  protected level 0
  # Array of commands for each level, so we can do up and top
  protected LevelCmd
  # State of buttons, so they can be disabled when busy and 
  # reset properly when back to idle
  protected BState
  # running state
  protected Running 0
}

