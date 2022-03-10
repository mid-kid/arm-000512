#
# ModalDialog
# ----------------------------------------------------------------------
# Implements the post and unpost behavior of a Modal Dialog.
#
# For now the point behind this is to control calling 
# ide_grab_support.  If you call disable all the windows of an
# application but one, destroy that window, THEN re-enable the
# windows, Windows brings the last enabled window in the last
# active application to the foreground (Doh!).
#
# ----------------------------------------------------------------------
#   AUTHOR:  Jim Ingham <jingham@cygnus.com>
#   Copyright (C) 1998 Cygnus Solutions
#

class ModalDialog {

  # This is the variable we vwait on when the dialog is posted.  
  # It is set to 1 in the unpost method, and to -1 in the destructor.

  private variable unpost_notification 0

  destructor {
    debug "  UNPOST $this"
    set unpost_notification -1
  }

  # ------------------------------------------------------------------
  #  METHOD:  unpost - unposts the dialog box...
  # ------------------------------------------------------------------
  public method unpost {} {
    after idle [list set [scope unpost_notification] 1]
  }

  # ------------------------------------------------------------------
  #  METHOD:  cancel - This just unposts the dialog box...
  #           If you want to programatically cancel a dialog
  #           selection, for instance if the app is going away
  #           use this rather than unpost.  That way a sub-class
  #           that actually has to do some work can override it.
  # ------------------------------------------------------------------
  public method cancel {} {
    ModalDialog::unpost
  }


  # ------------------------------------------------------------------
  #  METHOD:  post - posts the dialog box...
  # ------------------------------------------------------------------
  public method post {{on_top 0}} {

    debug "POST $this"
    set top [winfo toplevel [namespace tail $this]]
    wm protocol $top WM_DELETE_WINDOW [code $this cancel]

    if {$on_top} {
      after 500 keep_raised $top
    }
    
    ide_grab_support disable_except $top
    focus $top
    grab set $top
 
    vwait [scope unpost_notification]

    grab release $top
 
    # Enable all the windows in the application BEFORE
    # you destroy this one, or Windows will bring another
    # app to the foreground.
  
    ide_grab_support enable_all

    # We can get here either by someone calling unpost (if an OK button
    # is clicked or whatever), or by someone destroying the dialog (for
    # instance by using the Window Manager.)  Only delete the object if
    # we are not already in the process of doing this.

    if {$unpost_notification == 1} {
      ::delete object $this
    } 
  }
}
