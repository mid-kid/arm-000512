#
# VariableWin
# ----------------------------------------------------------------------
# Implements variable windows for gdb. LocalsWin and WatchWin both
# inherit from this class. You need only override the method 
# 'getVariablesBlankPath' and a few other things...
# ----------------------------------------------------------------------
#   AUTHOR:  Keith Seitz <keiths@cygnus.com>
#   Copyright (C) 1997, 1998 Cygnus Solutions
#

class VariableWin {
  inherit EmbeddedWin GDBWin
  protected variable Sizebox 1

  # ------------------------------------------------------------------
  #  CONSTRUCTOR - create new watch window
  # ------------------------------------------------------------------
  constructor {args} {
    #
    #  Create a window with the same name as this object
    #
    gdbtk_busy
    build_win $itk_interior
    gdbtk_idle

    add_hook gdb_update_hook "$this update"
    add_hook gdb_busy_hook "$this disable_ui"
    add_hook gdb_no_inferior_hook "$this no_inferior"
    add_hook gdb_idle_hook [list $this idle]
  }

  # ------------------------------------------------------------------
  #  METHOD:  build_win - build the watch window
  # ------------------------------------------------------------------
  method build_win {f} {
    global tixOption tcl_platform Display
    #    debug "VariableWin::build_win"
    set width [font measure src-font "W"]
    # Choose the default width to be...
    set width [expr {40 * $width}]
    if {$tcl_platform(platform) == "windows"} {
      set scrollmode both
    } else {
      set scrollmode auto
    }

    debug "tree=$f.tree"
    set Tree [tixTree $f.tree        \
		-opencmd  "$this open"  \
		-closecmd "$this close" \
		-ignoreinvoke 1         \
		-width $width           \
		-browsecmd [list $this selectionChanged] \
		-scrollbar $scrollmode \
		-sizebox $Sizebox]
    if {![pref get gdb/mode]} {
      $Tree configure -command [list $this editEntry]
    }
    set Hlist [$Tree subwidget hlist]

    # FIXME: probably should use columns instead.
    $Hlist configure -header 1

    set l [expr {$EntryLength - $Length - [string length "Name"]}]
    # Ok, this is as hack as it gets
    set blank "                                                                                                                                                             "
    $Hlist header create 0 -itemtype text \
      -text "Name[string range $blank 0 $l]Value"

    # Configure the look of the tree
    set sbg [$Hlist cget -bg]
    set fg [$Hlist cget -fg]
    set bg $tixOption(input1_bg)
    set width [font measure src-font $LengthString]
    $Hlist configure -indent $width -bg $bg \
      -selectforeground $fg -selectbackground $sbg \
      -selectborderwidth 0 -separator . -font src-font

    # Get display styles
    set normal_fg    [$Hlist cget -fg]
    set highlight_fg [pref get gdb/variable/highlight_fg]
    set disabled_fg  [pref get gdb/variable/disabled_fg]
    set NormalTextStyle [tixDisplayStyle text -refwindow $Hlist \
			   -bg $bg -fg $normal_fg -font src-font]
    set HighlightTextStyle [tixDisplayStyle text -refwindow $Hlist \
			      -bg $bg -fg $highlight_fg -font src-font]
    set DisabledTextStyle [tixDisplayStyle text -refwindow $Hlist \
			     -bg $bg -fg $disabled_fg -font src-font]

    if {[catch {gdb_cmd "show output-radix"} msg]} {
      set Radix 10
    } else {
      regexp {[0-9]+} $msg Radix
    }


    # Update the tree display
    update
    pack $Tree -expand yes -fill both

    # Create the popup menu for this widget
    bind $Hlist <3> "$this postMenu %X %Y"

    # Do not use the tixPopup widget... 
    set Popup [menu $f.menu -tearoff 0]
    set disabled_foreground [$Popup cget -foreground]
    $Popup configure -disabledforeground $disabled_foreground
    set ViewMenu [menu $Popup.view]

    # Populate the view menu
    $ViewMenu add radiobutton -label "Hex" -variable Display($this) \
      -value hexadecimal
    $ViewMenu add radiobutton -label "Decimal" -variable Display($this) \
      -value decimal
    $ViewMenu add radiobutton -label "Binary" -variable Display($this) \
      -value binary
    $ViewMenu add radiobutton -label "Octal" -variable Display($this) \
      -value octal
    $ViewMenu add radiobutton -label "Natural" -variable Display($this) \
      -value natural

    $Popup add command -label "dummy" -state disabled
    $Popup add separator
    $Popup add cascade -label "Format" -menu $ViewMenu
#    $Popup add checkbutton -label "Auto Update"
#    $Popup add command -label "Update Now"
    if {![pref get gdb/mode]} {
      $Popup add command -label "Edit"
    }

    # Make sure to update menu info.
    selectionChanged ""

    window_name "Local Variables"
  }

  # ------------------------------------------------------------------
  #  DESTRUCTOR - destroy window containing widget
  # ------------------------------------------------------------------
  destructor {
#    debug "VariableWin::destructor"
    # Make sure to clean up all the variables
    foreach var [array names Variables] {
      $Variables($var) delete
    }
    
    # Delete the display styles used with this window
    destroy $NormalTextStyle
    destroy $HighlightTextStyle
    destroy $DisabledTextStyle

    # Remove this window and all hooks
    remove_hook gdb_update_hook "$this update"
    remove_hook gdb_busy_hook "$this disable_ui"
    remove_hook gdb_no_inferior_hook "$this no_inferior"
    remove_hook gdb_idle_hook [list $this idle]
  }

  # ------------------------------------------------------------------
  #  METHOD:  reconfig - used when preferences change
  # ------------------------------------------------------------------
  method reconfig {} {
    #    debug "VariableWin::reconfig"
    foreach win [winfo children $itk_interior] { 
      destroy $win
    }

    build_win $itk_interior
  }

  # ------------------------------------------------------------------
  #  METHOD:  build_menu_helper - Create the menu for a subclass.
  # ------------------------------------------------------------------
  method build_menu_helper {first} {
    global Display
    menu [namespace tail $this].mmenu

    [namespace tail $this].mmenu add cascade -label $first -underline 0 -menu [namespace tail $this].mmenu.var

    menu [namespace tail $this].mmenu.var
    if {![pref get gdb/mode]} {
      [namespace tail $this].mmenu.var add command -label Edit -underline 0 -state disabled \
	  -command [format {
	%s editEntry [%s getSelection]
      } $this $this]
      }
    [namespace tail $this].mmenu.var add cascade -label Format -underline 0 \
      -menu [namespace tail $this].mmenu.var.format

    menu [namespace tail $this].mmenu.var.format
    foreach label {Hex Decimal Binary Octal Natural} fmt {hexadecimal decimal binary octal natural} {
      [namespace tail $this].mmenu.var.format add radiobutton \
	-label $label -underline 0 \
	-value $fmt -variable Display($this) \
	-command [format {
	  %s setDisplay [%s getSelection] %s
	} $this $this $fmt]
    }

#    [namespace tail $this].mmenu add cascade -label Update -underline 0 -menu [namespace tail $this].mmenu.update
#    menu [namespace tail $this].mmenu.update

    # The -variable is set when a selection is made in the tree.
#    [namespace tail $this].mmenu.update add checkbutton -label "Auto Update" -underline 0 \
#      -command [format {
#	%s toggleUpdate [%s getSelection]
#      } $this $this]
#    [namespace tail $this].mmenu.update add command -label "Update Now" -underline 0 \
#      -accelerator "Ctrl+U" -command [format {
#	%s updateNow [%s getSelection]
#      } $this $this]

    set top [winfo toplevel [namespace tail $this]]
    $top configure -menu [namespace tail $this].mmenu
    bind_plain_key $top Control-u [format {
      if {!$Running} {
	if {[%s getSelection] != ""} {
	  %s updateNow [%s getSelection]
	}
      }
    } $this $this $this]

    return [namespace tail $this].mmenu.var
  }

  # Return the current selection, or the empty string if none.
  method getSelection {} {
    return [$Hlist info selection]
  }

  # This is called when a selection is made.  It updates the main
  # menu.
  method selectionChanged {variable} {
    global Display

    if {$Running} {
      # Clear the selection, too
      $Hlist selection clear
      return
    }

    # if something is being edited, cancel it
    if {[info exists EditEntry]} {
      UnEdit
    }

    if {$variable == ""} {
      set state disabled
    } else {
      set state normal
    }

    foreach menu [list [namespace tail $this].mmenu.var [namespace tail $this].mmenu.var.format ] {
      set i [$menu index last]
      while {$i >= 0} {
	if {[$menu type $i] != "cascade"} {
	  $menu entryconfigure $i -state $state
	}
	incr i -1
      }
    }

    if {$variable != "" && [$variable editable]} {
      set state normal
    } else {
      set state disabled
    }

    if {$variable != ""} {
      set Display($this) [$variable format]
    }

    foreach label {Hex Decimal Binary Octal Natural} {
      [namespace tail $this].mmenu.var.format entryconfigure $label
      if {$label != "Hex"} {
	[namespace tail $this].mmenu.var.format entryconfigure $label -state $state
      }
    }
#    [namespace tail $this].mmenu.update entryconfigure 0 -variable Update($this,$name)
  }

  method updateNow {variable} {
     # debug "VariableWin::updateNow $variable"

    if {!$Running} {
      set text [label $variable]
      $Hlist entryconfigure $variable -itemtype text -text $text
    }
  }

  method getEntry {x y} {
    set realY [expr {$y - [winfo rooty $Hlist]}]

    # Get the tree entry which we are over
    return [$Hlist nearest $realY]
  }

  method editEntry {variable} {
    if {!$Running} {
      if {$variable != "" && [$variable editable]} {
	edit $variable
      }
    }
  }

  method postMenu {X Y} {
    global Update Display
    #    debug "VariableWin::postMenu"

    # Quicky for menu posting problems.. How to unpost and post??

    if {[winfo ismapped $Popup] || $Running} {
      return
    }

    set variable [getEntry $X $Y]
    if {[string length $variable] > 0} {
      # Configure menu items
      # the title is always first..
      #set labelIndex [$Popup index "dummy"]
      set viewIndex [$Popup index "Format"]
#      set autoIndex [$Popup index "Auto Update"]
#      set updateIndex [$Popup index "Update Now"]
      set noEdit [catch {$Popup index "Edit"} editIndex]

      # Retitle and set update commands
      $Popup entryconfigure 0 -label "[$variable name]"
#      $Popup entryconfigure $autoIndex -command "$this toggleUpdate \{$entry\}" \
	-variable Update($this,$entry) 
#      $Popup entryconfigure $updateIndex -command "$this updateNow \{$entry\}"

      # Edit pane
      if {$variable != "" && [$variable editable]} {
	if {!$noEdit} {
	  $Popup delete $editIndex
	}
	if {![pref get gdb/mode]} {
	  $Popup  add command -label Edit -command "$this edit \{$variable\}"
	}
      } else {
	if {!$noEdit} {
	  $Popup delete $editIndex
	}
      }

      # Set view menu
      set Display($this) [$variable format]
      foreach i {0 1 2 3 4} fmt {hexadecimal decimal binary octal natural} {
	debug "configuring entry $i ([$ViewMenu entrycget $i -label]) to $fmt"
	$ViewMenu entryconfigure $i \
	  -command "$this setDisplay \{$variable\} $fmt"
      }

      tk_popup $Popup $X $Y
    }
  }

  # ------------------------------------------------------------------
  # METHOD edit -- edit a variable
  # ------------------------------------------------------------------
  method edit {variable} {
    global Update tixOption

    # disable menus
    selectionChanged ""

    set fg   [$Hlist cget -foreground]
    set bg   [$Hlist cget -background]

    if {$Editing == ""} {
      # Must create the frame
      set Editing [frame $Hlist.frame -bg $bg -bd 0 -relief flat]
      set lbl [::label $Editing.lbl -fg $fg -bg $bg -font src-font]
      set ent [entry $Editing.ent -bg $tixOption(bg) -font src-font]
      pack $lbl $ent -side left
    }

    if {[info exists EditEntry]} {
      # We already are editing something... So reinstall it first
      # I guess we discard any changes?
      UnEdit
    }

    # Update the label/entry widgets for this instance
    set Update($this,$variable) 1
    set EditEntry $variable
    set label [label $variable 1];	# do not append value
    $Editing.lbl configure -text "$label  "
    $Editing.ent delete 0 end

    # Strip the pointer type, text, etc, from pointers, and such
    set err [catch {$variable value} text]
    if {$err} {return}
    if {[$variable format] == "natural"} {
      # Natural formats must be stripped. They often contain
      # things like strings and characters after them.
      set index [string first \  $text]
      if {$index != -1} {
	set text [string range $text 0 [expr {$index - 1}]]
      }
    }
    $Editing.ent insert 0 $text

    # Find out what the previous entry is
    set previous [getPrevious $variable]

    close $variable
    $Hlist delete entry $variable

    set cmd [format { \
      %s add {%s} %s -itemtype window -window %s \
    } $Hlist $variable $previous $Editing]
    eval $cmd

    if {[$variable numChildren] > 0} {
      $Tree setmode $variable open
    }

    # Set focus to entry
    focus $Editing.ent
    $Editing.ent selection to end

    # Setup key bindings
    bind $Editing.ent <Return> "$this changeValue"
    bind $Hlist <Return> "$this changeValue"
    bind $Editing.ent <Escape> "$this UnEdit"
    bind $Hlist <Escape> "$this UnEdit"
  }

  method getPrevious {variable} {
    set prev [$Hlist info prev $variable]
    set parent [$Hlist info parent $variable]

    if {$prev != ""} {
      # A problem occurs with PREV if its parent is not the same as the entry's
      # parent. For example, consider these variables in the window:
      # + foo        struct {...}
      # - bar        struct {...}
      #     a        1
      #     b        2
      # local        0
      # if you attempt to edit "local", previous will be set at "bar.b", not
      # "struct bar"...
      if {[$Hlist info parent $prev] != $parent} {
	# This is the problem!
	# Find this object's sibling in that parent and place it there.
	set children [$Hlist info children $parent]
	set p {}
	foreach child $children {
	  if {$child == $variable} {
	    break
	  }
	  set p $child
	}

	if {$p == {}} {
	  # This is the topmost child
	  set previous "-before [lindex $children 1]"
	} else {
	  set previous "-after $p"
	}
      } else {
	set previous "-after \{$prev\}"
      }
    } else {
      # this is the first!
      set previous "-at 0"
    }
    
    if {$prev == "$parent"} {
      # This is the topmost-member of a sub-grouping..
      set previous "-at 0"
    }

    return $previous
  }

  method UnEdit {} {
    set previous [getPrevious $EditEntry]
    
    $Hlist delete entry $EditEntry
    set cmd [format {\
       %s add {%s} %s -itemtype text -text {%s} \
    } $Hlist $EditEntry $previous [label $EditEntry]]
    eval $cmd
    if {[$EditEntry numChildren] > 0} {
      $Tree setmode $EditEntry open
    }
    
    # Unbind
    bind $Hlist <Return> {}
    bind $Hlist <Escape> {}
    if {$Editing != ""} {
      bind $Editing.ent <Return> {}
      bind $Editing.ent <Escape> {}
    }
    
    unset EditEntry
    selectionChanged ""
  }

  method changeValue {} {
    # Get the old value
    set new [string trim [$Editing.ent get] \ \r\n]
    if {$new == ""} {
      UnEdit
      return
    }

    if {[catch {$EditEntry value $new} errTxt]} {
      tk_messageBox -icon error -type ok -message $errTxt \
	-title "Error in Expression" -parent [winfo toplevel $itk_interior]
      focus $Editing.ent
      $Editing.ent selection to end
    } else {
      UnEdit
      
      # Get rid of entry... and replace it with new value
      focus $Tree
    }
  }

  method toggleUpdate {variable} {
    global Update

    if {$Update($this,$variable)} {
      # Must update value
      $Hlist entryconfigure $variable \
	-style $NormalTextStyle    \
	-text [label $variable]
    } else {
      $Hlist entryconfigure $variable \
	-style $DisabledTextStyle
    }
    ::update
  }

  method setDisplay {variable format} {
    debug "$variable $format"
    if {!$Running} {
      $variable format $format
      set ::Display($this) $format
      $Hlist entryconfigure $variable -text [label $variable]
    }
  }
  
  # ------------------------------------------------------------------
  # METHOD:   label - used to label the entries in the tree
  # ------------------------------------------------------------------
  method label {variable {noValue 0}} {
    # Ok, this is as hack as it gets
    set blank "                                                                                                                                                             "
    # Use protected data Length to determine how big variable
    # name should be. This should clean the display up a little
    set name [$variable name]
    set indent [llength [split $variable .]]
    set indent [expr {$indent * $Length}]
    set len [string length $name]
    set l [expr {$EntryLength - $len - $indent}]
    set label "$name[string range $blank 0 $l]"
    #debug "label=$label $noValue"
    if {$noValue} {
      return $label
    }

    set err [catch {$variable value} value]
    set value [string trim $value \ \r\t\n]
    #debug "err=$err value=$value"

    # Insert the variable's type for things like ptrs, etc.
    set type [$variable type]
    if {!$err} {
      if {$value == "{...}"} {
	set val " $type $value"
      } elseif {[string first * $type] != -1} {
	set val " ($type) $value"
      } elseif {[string first \[ $type] != -1} {
	set val " $type"
      } else {
	set val " $value"
      }
    } else {
      set val " $value"
    }

    return "$label $val"
  }

  # ------------------------------------------------------------------
  # METHOD:   open - used to open an entry in the variable tree
  # ------------------------------------------------------------------
  method open {path} {
    global Update
    # We must lookup all the variables for this struct
    #    debug "VariableWin::open $path"

    if {!$Running} {
      # Do not open disabled paths
      if {$Update($this,$path)} {
	cursor watch
	populate $path
	cursor {}
      }
    } else {
      $Tree setmode $path open
    }
  }

  # ------------------------------------------------------------------
  # METHOD:   close - used to close an entry in the variable tree
  # ------------------------------------------------------------------
  method close {path} {
    global Update
    #    debug "VariableWin::close"
    # Close the path and destroy all the entry widgets
    #    debug "CLOSE: $path"

    if {!$Running} {
      # Only update when we we are not disabled
      if {$Update($this,$path)} {

	# Delete the offspring of this entry
	$Hlist delete offspring $path
	
	# I don't think we want to do this...
	#set variables [displayedVariables $path]
	#      debug "variables=$variables"	
	# Delete protected/global data
	#foreach var $variables {
	#  catch {
	#    $Variables($var) delete
	#    unset Variables($var)
	#    unset Types($var)
	#    unset Update($this,$var)
	#  }
	  #      debug "destroyed $var"
	#}
      }
    } else {
      $Tree setmode $path close
    }
  }

  method isVariable {var} {

    set err [catch {gdb_cmd "output $var"} msg]
    if {$err 
	|| [regexp -nocase "no symbol|syntax error" $msg]} {
      return 0
    }

    return 1
  }

  # OVERRIDE THIS METHOD
  method getVariablesBlankPath {} {
    debug "You forgot to override getVariablesBlankPath!!"
    return {}
  }

  method cmd {cmd} {
    eval $cmd
  }
  
  # ------------------------------------------------------------------
  # METHOD:   populate - populate an entry in the tree
  # ------------------------------------------------------------------
  method populate {parent} {
    global Update
    debug "VariableWin::populate \"$parent\""

    if {[string length $parent] == 0} {
      set variables [getVariablesBlankPath]
    } else {
      set variables [$parent children]
    }

    debug "variables=$variables"
    foreach variable $variables {
      set Update($this,$variable) 1

      # Update each variable in case its value has changed since the
      # last time it was viewed.
      $variable valueChanged

      $Hlist add $variable          \
	-itemtype text              \
	-text [label $variable]
      if {[$variable numChildren] > 0} {
	# Make sure we get this labeled as openable
	$Tree setmode $variable open
      }
    }
    debug "done with populate"
  }
  
  # Get all current locals
  proc getLocals {} {

    set vars {}
    set err [catch {gdb_get_args} v]
    if {!$err} {
      set vars [concat $vars $v]
    }

    set err [catch {gdb_get_locals} v]
    if {!$err} {
      set vars [concat $vars $v]
    }

    debug "--getLocals:\n$vars\n--getLocals"
    return [lsort $vars]
  }

  method context_switch {} {

    set err [catch {gdb_selected_frame} current_frame]
    if {$err && $Frame != ""} {
      # No current frame
      debug "no current frame"
      set Frame {}
      return 1
    } elseif {$current_frame != $Frame} {
      # We've changed frames. If we knew something about
      # the stack layout, we could be more intelligent about
      # destroying variables, but we don't know that here (yet).
      debug "switching from frame at $Frame to frame at $current_frame"
      set Frame $current_frame
      return 1
    }

    # Nothing changed
    return 0
  }

  # OVERRIDE THIS METHOD and call it from there
  method update {} {
    global Update
    debug "VariableWin::update"

    # First, reset color on label to black
    foreach w $ChangeList {
      catch {
	$Hlist entryconfigure $w -style $NormalTextStyle
      }
    }

    # Get all the values of the variables shown in the box
    set variables [displayedVariables {}]

    # We now have a list of all editable variables.
    # Find out which have changed.
    foreach var $variables {
      #      debug "VARIABLE: $var ($Update($this,$var))"
      if {[$var valueChanged] == "VARIABLE_CHANGED"} {
	# Insert newValue into list and highlight it
	lappend ChangeList $var
	$Hlist entryconfigure $var \
	  -style  $HighlightTextStyle   \
	  -text [label $var]
      }
    }
  }

  method idle {} {
    # Re-enable the UI
    enable_ui
  }

  # RECURSION!!
  method displayedVariables {top} {
    #    debug "VariableWin::displayedVariables"
    set variableList {}
    set variables [$Hlist info children $top]
    foreach var $variables {
      set mode [$Tree getmode $var]
      if {$mode == "close"} {
	set moreVars [displayedVariables $var]
	lappend variableList [join $moreVars]
      }
      lappend variableList $var
    }

    return [join $variableList]
  }

  method deleteTree {} {
    global Update
    #    debug "VariableWin::deleteTree"
    set variables [displayedVariables {}]

    # Delete all HList entries
    $Hlist delete all

    # Delete the variable objects
    foreach i [array names Variables] {
      $Variables($i) delete
      unset Variables($i)
      catch {unset Update($this,$i)}
    }
  }

  # ------------------------------------------------------------------
  # METHOD:   enable_ui
  #           Enable all ui elements.
  # ------------------------------------------------------------------
  method enable_ui {} {
    
    # Clear fencepost
    set Running 0
    cursor {}
  }

  # ------------------------------------------------------------------
  # METHOD:   disable_ui
  #           Disable all ui elements that could affect gdb's state
  # ------------------------------------------------------------------
  method disable_ui {} {

    # Set fencepost
    set Running 1

    # Cancel any edits
    if {[info exists EditEntry]} {
      UnEdit
    }

    # Change cursor
    cursor watch
  }

  # ------------------------------------------------------------------
  # METHOD:   no_inferior
  #           Reset this object.
  # ------------------------------------------------------------------
  method no_inferior {} {

    # Clear out the Hlist
    deleteTree

    # Clear fencepost
    set Running 0
    set Frame {}
    cursor {}
  }

  # ------------------------------------------------------------------
  #  METHOD:  cursor - change the toplevel's cursor
  # ------------------------------------------------------------------
  method cursor {what} {
    [winfo toplevel [namespace tail $this]] configure -cursor $what
    ::update idletasks
  }

  #
  # PUBLIC DATA
  #

  #
  #  PROTECTED DATA
  #

  # the tixTree widget for this class
  protected variable Tree  {}

  # the hlist of this widget
  protected variable Hlist {}

  # entry widgets which need to have their color changed back to black
  # when idle (used in conjunction with update)
  protected variable ChangeList {}

  # array of top-most variables in tree
  protected variable Variables

  protected variable ViewMenu
  protected variable Popup

  # These are for setting the indent level to an number of characters.
  # This will help clean the tree a little
  common EntryLength 15
  common Length 1
  common LengthString " "

  # These should be common... but deletion?
  # Display styles for HList
  protected variable HighlightTextStyle
  protected variable NormalTextStyle
  protected variable DisabledTextStyle
 
  protected variable Radix

  protected variable Locals {}
  protected variable Frame {}

  protected variable Editing {}
  protected variable EditEntry

  # Fencepost for enable/disable_ui and idle/busy hooks.
  protected variable Running 0
}
