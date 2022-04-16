#
# About
# ----------------------------------------------------------------------
# Implements About window
# ----------------------------------------------------------------------
#   AUTHOR:  Martin M. Hunt	hunt@cygnus.com     
#            Copyright (C) 1997, 1998, 1999 Cygnus Solutions   
#

class About {
  inherit ManagedWin ModalDialog
  constructor {args} {
    global gdb_ImageDir
    set f [frame $itk_interior.f]
    label $f.image1 -bg white -image \
      [image create photo -file [file join $gdb_ImageDir insight.gif]]
    message $f.m -bg white -fg black -text [gdb_cmd {show version}] -aspect 500 -relief flat
    pack $f.image1 $f.m $itk_interior.f -fill both -expand yes
    pack  $itk_interior
    bind $f.image1 <1> [code $this unpost]
    bind $f.m <1> [code $this unpost]
    window_name "About Cygnus Insight"
  }
}

