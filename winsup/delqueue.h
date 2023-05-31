/* delqueue.h

   Copyright 1996, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

/* First pass at a file deletion queue structure.

   We can't keep this list in the per-process info, since
   one process may open a file, and outlive a process which
   wanted to unlink the file - and the data would go away.

   Perhaps the FILE_FLAG_DELETE_ON_CLOSE would be ok,
   but brief experimentation didn't get too far.
*/   

#include <windows.h>

#define MAX_DELQUEUES_PENDING 100

class delqueue_list 
{
  char name[MAX_DELQUEUES_PENDING][MAX_PATH];
  char inuse[MAX_DELQUEUES_PENDING];
  int empty;

public:
  void init ();  
  void queue_file (const char *dosname);
  void process_queue ();
};
