/* Startup code for gdbtk.
   Copyright 1994, 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "inferior.h"
#include "command.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "gdbcore.h"
#include "tracepoint.h"
#include "demangle.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <sys/stat.h>

#include <tcl.h>
#include <tk.h>
#include <itcl.h> 
#include <tix.h> 
#include "guitcl.h"
#include "gdbtk.h"

#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include "top.h"
#include <sys/ioctl.h>
#include "gdb_string.h"
#include "dis-asm.h"
#include <stdio.h>
#include "gdbcmd.h"

#include "annotate.h"
#include <sys/time.h>

int in_fputs = 0;

/* Set by gdb_stop, this flag informs x_event to tell its caller
   that it should forcibly detach from the target. */
int gdbtk_force_detach = 0;

extern int  (*ui_load_progress_hook) PARAMS ((char *, unsigned long));
extern void (*pre_add_symbol_hook) PARAMS ((char *));
extern void (*post_add_symbol_hook) PARAMS ((void));
extern void (*selected_frame_level_changed_hook) PARAMS ((int));
extern int (*ui_loop_hook) PARAMS ((int));

static void   gdbtk_create_tracepoint PARAMS ((struct tracepoint *));
static void   gdbtk_delete_tracepoint PARAMS ((struct tracepoint *));
static void   gdbtk_modify_tracepoint PARAMS ((struct tracepoint *));
static void   gdbtk_trace_find  PARAMS ((char *arg, int from_tty));
static void   gdbtk_trace_start_stop PARAMS ((int, int));
static void   gdbtk_create_breakpoint PARAMS ((struct breakpoint *));
static void   gdbtk_delete_breakpoint PARAMS ((struct breakpoint *));
static void   gdbtk_modify_breakpoint PARAMS ((struct breakpoint *));
static void   gdbtk_file_changed PARAMS ((char *));
static void   gdbtk_exec_file_display PARAMS ((char *));
static void   tk_command_loop PARAMS ((void));
static void   gdbtk_call_command PARAMS ((struct cmd_list_element *, char *, int));
static int    gdbtk_wait PARAMS ((int, struct target_waitstatus *));
int x_event PARAMS ((int));
static int    gdbtk_query PARAMS ((const char *, va_list));
static void   gdbtk_warning PARAMS ((const char *, va_list));
static char*  gdbtk_readline PARAMS ((char *));
static void gdbtk_readline_begin (char *format, ...);
static void gdbtk_readline_end PARAMS ((void));
static void   gdbtk_flush PARAMS ((GDB_FILE *));
static void gdbtk_pre_add_symbol PARAMS ((char *));
static void gdbtk_print_frame_info PARAMS ((struct symtab *, int, int, int));
static void gdbtk_post_add_symbol PARAMS ((void));
static void gdbtk_register_changed PARAMS ((int regno));
static void gdbtk_memory_changed PARAMS ((CORE_ADDR addr, int len));
static void tracepoint_notify PARAMS ((struct tracepoint *, const char *));
static void gdbtk_selected_frame_changed PARAMS ((int));
static void gdbtk_context_change PARAMS ((int));
static void gdbtk_error_begin PARAMS ((void));
static void report_error (void);

/*
 * gdbtk_fputs can't be static, because we need to call it in gdbtk.c.
 * See note there for details.
 */

void   gdbtk_fputs PARAMS ((const char *, GDB_FILE *));
int           gdbtk_load_hash PARAMS ((char *, unsigned long));
static void   breakpoint_notify PARAMS ((struct breakpoint *, const char *));

/*
 * gdbtk_add_hooks - add all the hooks to gdb.  This will get called by the
 * startup code to fill in the hooks needed by core gdb.
 */

void
gdbtk_add_hooks(void)
{
  command_loop_hook = tk_command_loop;
  call_command_hook = gdbtk_call_command;
  readline_begin_hook = gdbtk_readline_begin;
  readline_hook = gdbtk_readline;
  readline_end_hook = gdbtk_readline_end;

  print_frame_info_listing_hook = gdbtk_print_frame_info;
  query_hook = gdbtk_query;
  warning_hook = gdbtk_warning;
  flush_hook = gdbtk_flush;

  create_breakpoint_hook = gdbtk_create_breakpoint;
  delete_breakpoint_hook = gdbtk_delete_breakpoint;
  modify_breakpoint_hook = gdbtk_modify_breakpoint;

  interactive_hook       = gdbtk_interactive;
  target_wait_hook       = gdbtk_wait;
  ui_load_progress_hook  = gdbtk_load_hash;

  ui_loop_hook = x_event;
  pre_add_symbol_hook    = gdbtk_pre_add_symbol;
  post_add_symbol_hook   = gdbtk_post_add_symbol;
  file_changed_hook      = gdbtk_file_changed;
  exec_file_display_hook = gdbtk_exec_file_display;

  create_tracepoint_hook = gdbtk_create_tracepoint;
  delete_tracepoint_hook = gdbtk_delete_tracepoint;
  modify_tracepoint_hook = gdbtk_modify_tracepoint;
  trace_find_hook        = gdbtk_trace_find;
  trace_start_stop_hook  = gdbtk_trace_start_stop;

  register_changed_hook = gdbtk_register_changed;
  memory_changed_hook = gdbtk_memory_changed;
  selected_frame_level_changed_hook = gdbtk_selected_frame_changed;
  context_hook = gdbtk_context_change;

  error_begin_hook = gdbtk_error_begin;
}

/* These control where to put the gdb output which is created by
   {f}printf_{un}filtered and friends.  gdbtk_fputs and gdbtk_flush are the
   lowest level of these routines and capture all output from the rest of GDB.

   The reason to use the result_ptr rather than the gdbtk_interp's result
   directly is so that a call_wrapper invoked function can preserve its result
   across calls into Tcl which might be made in the course of the function's
   execution.
   
   * result_ptr->obj_ptr is where to accumulate the result.
   * GDBTK_TO_RESULT flag means the output goes to the gdbtk_tcl_fputs proc
     instead of to the result_ptr.
   * GDBTK_MAKES_LIST flag means add to the result as a list element.

   */

gdbtk_result *result_ptr = NULL;


/* This allows you to Tcl_Eval a tcl command which takes
   a command word, and then a single argument. */
  
int gdbtk_two_elem_cmd (cmd_name, argv1)
    char *cmd_name;
    char * argv1;
{
  char *command;
  int result, flags_ptr, arg_len, cmd_len;

  arg_len = Tcl_ScanElement (argv1, &flags_ptr);
  cmd_len = strlen (cmd_name);
  command = malloc(arg_len + cmd_len + 2);
  strcpy (command, cmd_name);
  strcat (command, " ");
  
  Tcl_ConvertElement (argv1, command + cmd_len + 1, flags_ptr);

  result = Tcl_Eval (gdbtk_interp, command);
  if (result != TCL_OK)
    report_error ();
  free (command);
  return result;
}

static void
gdbtk_flush (stream)
     GDB_FILE *stream;
{
#if 0
  /* Force immediate screen update */

  Tcl_VarEval (gdbtk_interp, "gdbtk_tcl_flush", NULL);
#endif
}

/* This handles all the output from gdb.  All the gdb printf_xxx functions
 * eventually end up here.  The output is either passed to the result_ptr
 * where it will go to the result of some gdbtk command, or passed to the
 * Tcl proc gdbtk_tcl_fputs (where it is usually just dumped to the console
 * window.
 *
 * The cases are:
 *
 * 1) result_ptr == NULL - This happens when some output comes from gdb which
 *    is not generated by a command in gdbtk-cmds, usually startup stuff.
 *    In this case we just route the data to gdbtk_tcl_fputs.
 * 2) The GDBTK_TO_RESULT flag is set - The result is supposed to go to Tcl.
 *    We place the data into the result_ptr, either as a string,
 *    or a list, depending whether the GDBTK_MAKES_LIST bit is set.
 * 3) The GDBTK_TO_RESULT flag is unset - We route the data to gdbtk_tcl_fputs
 *    UNLESS it was coming to gdb_stderr.  Then we place it in the result_ptr
 *    anyway, so it can be dealt with.
 *
 */

void
gdbtk_fputs (ptr, stream)
     const char *ptr;
     GDB_FILE *stream;
{
  in_fputs = 1;

  if (result_ptr != NULL)
    {
      if (result_ptr->flags & GDBTK_TO_RESULT)
	{
	  if (result_ptr->flags & GDBTK_MAKES_LIST)
	    Tcl_ListObjAppendElement(NULL, result_ptr->obj_ptr, 
				     Tcl_NewStringObj((char *) ptr, -1));
	  else			         
	    Tcl_AppendToObj (result_ptr->obj_ptr, (char *) ptr, -1);
	}
      else if (stream == gdb_stderr || result_ptr->flags & GDBTK_ERROR_ONLY)
	{
	  if (result_ptr->flags & GDBTK_ERROR_STARTED)
	    Tcl_AppendToObj (result_ptr->obj_ptr, (char *) ptr, -1);
	  else
	    {
	      Tcl_SetStringObj (result_ptr->obj_ptr, (char *) ptr, -1);
	      result_ptr->flags |= GDBTK_ERROR_STARTED;
	    }
	}
      else
	{
	  gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", (char *) ptr);
	  if (result_ptr->flags & GDBTK_MAKES_LIST)
	    gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", " ");
	}
    }
  else
    {
      gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", (char *) ptr);
    }
  
  in_fputs = 0;
}

/*
 * This routes all warnings to the Tcl function "gdbtk_tcl_warning".
 */

static void
gdbtk_warning (warning, args)
     const char *warning;
     va_list args;
{
  char buf[200];

  vsprintf (buf, warning, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_warning", buf);
}


/* Error-handling function for all hooks */
/* Hooks are not like tcl functions, they do not simply return */
/* TCL_OK or TCL_ERROR.  Also, the calling function typically */
/* doesn't care about errors in the hook functions.  Therefore */
/* after every hook function, report_error should be called. */
/* report_error can just call Tcl_BackgroundError() which will */
/* pop up a messagebox, or it can silently log the errors through */
/* the gdbtk dbug command.  */

static void
report_error ()
{
  TclDebug ('E',Tcl_GetVar(gdbtk_interp,"errorInfo",TCL_GLOBAL_ONLY));
  /*  Tcl_BackgroundError(gdbtk_interp); */
}

/*
 * This routes all ignorable warnings to the Tcl function
 * "gdbtk_tcl_ignorable_warning".
 */

void
gdbtk_ignorable_warning (class, warning)
     const char *class;
     const char *warning;
{
  char buf[512];
  sprintf (buf, "gdbtk_tcl_ignorable_warning {%s} {%s}", class, warning);
  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error();
}

static void
gdbtk_register_changed(regno)
     int regno;
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_register_changed") != TCL_OK)
    report_error();
}

static void
gdbtk_memory_changed(addr, len)
     CORE_ADDR addr;
     int len;
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_memory_changed") != TCL_OK)
    report_error();
}


/* This function is called instead of gdb's internal command loop.  This is the
   last chance to do anything before entering the main Tk event loop. 
   At the end of the command, we enter the main loop. */

static void
tk_command_loop ()
{
  extern FILE *instream;

  /* We no longer want to use stdin as the command input stream */
  instream = NULL;

  if (Tcl_Eval (gdbtk_interp, "gdbtk_tcl_preloop") != TCL_OK)
    {
      char *msg;

      /* Force errorInfo to be set up propertly.  */
      Tcl_AddErrorInfo (gdbtk_interp, "");

      msg = Tcl_GetVar (gdbtk_interp, "errorInfo", TCL_GLOBAL_ONLY);
#ifdef _WIN32
      MessageBox (NULL, msg, NULL, MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
      fputs_unfiltered (msg, gdb_stderr);
#endif
    }

#ifdef _WIN32
  close_bfds ();
#endif

  Tk_MainLoop ();
}

/* This hook is installed as the ui_loop_hook, which is used in several
 * places to keep the gui alive (x_event runs gdbtk's event loop). Users
 * include:
 * - ser-tcp.c in socket reading code
 * - ser-unix.c in serial port reading code
 * - built-in simulators while executing
 *
 * x_event used to be called on SIGIO on the socket to the X server
 * for unix. Unfortunately, Linux does not deliver SIGIO, so we resort
 * to an elaborate scheme to keep the gui alive.
 *
 * For simulators and socket or serial connections on all hosts, we
 * rely on ui_loop_hook (x_event) to keep us going. If the user
 * requests a detach (as a result of pressing the stop button -- see
 * comments before gdb_stop in gdbtk-cmds.c), it sets the global
 * GDBTK_FORCE_DETACH, which is the value that x_event returns to
 * it's caller. It is up to the caller of x_event to act on this
 * information.
 *
 * For native unix, we simply set an interval timer which calls
 * x_event to allow the debugger to run through the Tcl event
 * loop. See comments before gdbtk_start_timer and gdb_stop_timer
 * in gdbtk.c.
 *
 * For native windows (and a few other targets, like the v850 ICE),
 * we rely on the target_wait loops to call ui_loop_hook to keep us alive. */
int
x_event (signo)
     int signo;
{
  static int in_x_event = 0;
  static Tcl_Obj *varname = NULL;
  static int count = 0;
  if (in_x_event || in_fputs)
    return; 

  in_x_event = 1;
  gdbtk_force_detach = 0;

  /* Process pending events */
  while (Tcl_DoOneEvent (TCL_DONT_WAIT|TCL_ALL_EVENTS) != 0)
    ;

  if (load_in_progress)
    {
      int val;
      if (varname == NULL)
	{
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 1
	  Tcl_Obj *varnamestrobj = Tcl_NewStringObj("download_cancel_ok",-1);
	  varname = Tcl_ObjGetVar2(gdbtk_interp, varnamestrobj, NULL, TCL_GLOBAL_ONLY);
#else
	  varname = Tcl_GetObjVar2(gdbtk_interp, "download_cancel_ok", NULL, TCL_GLOBAL_ONLY);
#endif	  
	}
      if ((Tcl_GetIntFromObj(gdbtk_interp,varname,&val) == TCL_OK) && val)
	{
	  quit_flag = 1;
#ifdef REQUEST_QUIT
	  REQUEST_QUIT;
#else
	  if (immediate_quit) 
	    quit ();
#endif
	}
    }
  in_x_event = 0;

  return gdbtk_force_detach;
}

/* VARARGS */
static void
gdbtk_readline_begin (char *format, ...)
{
  va_list args;
  char buf[200];

  va_start (args, format);
  vsprintf (buf, format, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_readline_begin", buf);
}

static char *
gdbtk_readline (prompt)
     char *prompt;
{
  int result;

#ifdef _WIN32
  close_bfds ();
#endif

  result = gdbtk_two_elem_cmd ("gdbtk_tcl_readline", prompt);

  if (result == TCL_OK)
    {
      return (strdup (gdbtk_interp -> result));
    }
  else
    {
      gdbtk_fputs (gdbtk_interp -> result, gdb_stdout);
      gdbtk_fputs ("\n", gdb_stdout);
      return (NULL);
    }
}

static void
gdbtk_readline_end ()
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_tcl_readline_end") != TCL_OK)
    report_error ();
}

static void
gdbtk_call_command (cmdblk, arg, from_tty)
     struct cmd_list_element *cmdblk;
     char *arg;
     int from_tty;
{
  running_now = 0;
  if (cmdblk->class == class_run || cmdblk->class == class_trace)
    {

      running_now = 1;
      if (!No_Update)
	Tcl_Eval (gdbtk_interp, "gdbtk_tcl_busy");
      (*cmdblk->function.cfunc)(arg, from_tty);
      running_now = 0;
      if (!No_Update)
	Tcl_Eval (gdbtk_interp, "gdbtk_tcl_idle");
    }
  else
    (*cmdblk->function.cfunc)(arg, from_tty);
}

/* The next three functions use breakpoint_notify to allow the GUI 
 * to handle creating, deleting and modifying breakpoints.  These three
 * functions are put into the appropriate gdb hooks in gdbtk_init.
 */

static void
gdbtk_create_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "create");
}

static void
gdbtk_delete_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "delete");
}

static void
gdbtk_modify_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "modify");
}

/* This is the generic function for handling changes in
 * a breakpoint.  It routes the information to the Tcl
 * command "gdbtk_tcl_breakpoint" in the form:
 *   gdbtk_tcl_breakpoint action b_number b_address b_line b_file
 * On error, the error string is written to gdb_stdout.
 */

static void
breakpoint_notify(b, action)
     struct breakpoint *b;
     const char *action;
{
  char buf[256];
  int v;
  struct symtab_and_line sal;
  char *filename;

  if (b->type != bp_breakpoint)
    return;

  /* We ensure that ACTION contains no special Tcl characters, so we
     can do this.  */
  sal = find_pc_line (b->address, 0);
  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "";

  sprintf (buf, "gdbtk_tcl_breakpoint %s %d 0x%lx %d {%s} {%s} %d %d",
	   action, b->number, (long)b->address, b->line_number, filename,
	   bpdisp[b->disposition], b->enable,  b->thread);

  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
}

int
gdbtk_load_hash (section, num)
     char *section;
     unsigned long num;
{
  char buf[128];
  sprintf (buf, "Download::download_hash %s %ld", section, num);
  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
  return  atoi (gdbtk_interp->result);
}


/* This hook is called whenever we are ready to load a symbol file so that
   the UI can notify the user... */
static void
gdbtk_pre_add_symbol (name)
  char *name;
{
  gdbtk_two_elem_cmd("gdbtk_tcl_pre_add_symbol", name);
}

/* This hook is called whenever we finish loading a symbol file. */
static void
gdbtk_post_add_symbol ()
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_tcl_post_add_symbol") != TCL_OK)
    report_error ();
}

/* This hook function is called whenever we want to wait for the
   target.  */

static int
gdbtk_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  gdbtk_force_detach = 0;
  gdbtk_start_timer ();
  pid = target_wait (pid, ourstatus);
  gdbtk_stop_timer ();

  return pid;
}

/*
 * This handles all queries from gdb.
 * The first argument is a printf style format statement, the rest are its
 * arguments.  The resultant formatted string is passed to the Tcl function
 * "gdbtk_tcl_query".  
 * It returns the users response to the query, as well as putting the value
 * in the result field of the Tcl interpreter.
 */

static int
gdbtk_query (query, args)
     const char *query;
     va_list args;
{
  char buf[200];
  long val;

  vsprintf (buf, query, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_query", buf);
 
  val = atol (gdbtk_interp->result);
  return val;
}


static void
gdbtk_print_frame_info (s, line, stopline, noerror)
  struct symtab *s;
  int line;
  int stopline;
  int noerror;
{
  current_source_symtab = s;
  current_source_line = line;
}

static void
gdbtk_create_tracepoint (tp)
  struct tracepoint *tp;
{
  tracepoint_notify (tp, "create");
}

static void
gdbtk_delete_tracepoint (tp)
  struct tracepoint *tp;
{
  tracepoint_notify (tp, "delete");
}

static void
gdbtk_modify_tracepoint (tp)
  struct tracepoint *tp;
{
  tracepoint_notify (tp, "modify");
}

static void
tracepoint_notify(tp, action)
     struct tracepoint *tp;
     const char *action;
{
  char buf[256];
  int v;
  struct symtab_and_line sal;
  char *filename;

  /* We ensure that ACTION contains no special Tcl characters, so we
     can do this.  */
  sal = find_pc_line (tp->address, 0);

  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "N/A";
  sprintf (buf, "gdbtk_tcl_tracepoint %s %d 0x%lx %d {%s} %d", action, tp->number, 
	   (long)tp->address, sal.line, filename, tp->pass_count);

  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
}

/*
 * gdbtk_trace_find
 *
 * This is run by the trace_find_command.  arg is the argument that was passed
 * to that command, from_tty is 1 if the command was run from a tty, 0 if it
 * was run from a script.  It runs gdbtk_tcl_tfind_hook passing on these two
 * arguments.
 *
 */

static void
gdbtk_trace_find (arg, from_tty)
     char *arg;
     int from_tty;
{
  Tcl_Obj *cmdObj;
  
  cmdObj = Tcl_NewListObj (0, NULL);
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj,
			    Tcl_NewStringObj ("gdbtk_tcl_trace_find_hook", -1));
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj, Tcl_NewStringObj (arg, -1));
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj, Tcl_NewIntObj(from_tty));
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 1
  if (Tcl_GlobalEvalObj (gdbtk_interp, cmdObj) != TCL_OK)
    report_error ();
#else
  if (Tcl_EvalObj (gdbtk_interp, cmdObj, TCL_EVAL_GLOBAL) != TCL_OK)
    report_error ();
#endif    
}

/*
 * gdbtk_trace_start_stop
 *
 * This is run by the trace_start_command and trace_stop_command.
 * The START variable determines which, 1 meaning trace_start was run,
 * 0 meaning trace_stop was run.
 *
 */

static void
gdbtk_trace_start_stop (start, from_tty)
     int start;
     int from_tty;
{
  
  if (start)
    Tcl_GlobalEval (gdbtk_interp, "gdbtk_tcl_tstart");
  else
    Tcl_GlobalEval (gdbtk_interp, "gdbtk_tcl_tstop");

}

static void
gdbtk_selected_frame_changed (level)
     int level;
{
  Tcl_UpdateLinkedVar (gdbtk_interp, "gdb_selected_frame_level");
}

/* Called when the current thread changes. */
/* gdb_context is linked to the tcl variable "gdb_context_id" */
static void
gdbtk_context_change (num)
     int num;
{
  gdb_context = num;
}

/* Called from file_command */
static void
gdbtk_file_changed (filename)
     char *filename;
{
  gdbtk_two_elem_cmd ("gdbtk_tcl_file_changed", filename);
}

/* Called from exec_file_command */
static void
gdbtk_exec_file_display (filename)
     char *filename;
{
  gdbtk_two_elem_cmd ("gdbtk_tcl_exec_file_display", filename);
}

/* Called from error_begin, this hook is used to warn the gui
   about multi-line error messages */
static void
gdbtk_error_begin ()
{
  if (result_ptr != NULL)
    result_ptr->flags |= GDBTK_ERROR_ONLY;
}

/* Local variables: */
/* change-log-default-name: "ChangeLog-gdbtk" */
/* End: */
