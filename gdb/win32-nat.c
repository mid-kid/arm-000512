/* Target-vector operations for controlling win32 child processes, for GDB.
   Copyright 1995, 1996, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without eve nthe implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* by Steve Chamberlain, sac@cygnus.com */

/* We assume we're being built with and will be used for cygwin.  */

#include "defs.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "gdbcore.h"
#include "command.h"
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include "windefs.h"
#else /* other WIN32 compiler */
#include <windows.h>
#endif

#include "buildsym.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_string.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include <sys/param.h>
#include <unistd.h>

/* The ui's event loop. */
extern int (*ui_loop_hook) PARAMS ((int signo));

/* The string sent by cygwin when it processes a signal.
   FIXME: This should be in a cygwin include file. */
#define CYGWIN_SIGNAL_STRING "cygwin: signal"

#define CHECK(x) 	check (x, __FILE__,__LINE__)
#define DEBUG_EXEC(x)	if (debug_exec)		printf x
#define DEBUG_EVENTS(x)	if (debug_events)	printf x
#define DEBUG_MEM(x)	if (debug_memory)	printf x
#define DEBUG_EXCEPT(x)	if (debug_exceptions)	printf x

/* Forward declaration */
extern struct target_ops child_ops;

static void child_stop PARAMS ((void));
static int win32_child_thread_alive PARAMS ((int));
void child_kill_inferior PARAMS ((void));

static int last_sig = 0;	/* Set if a signal was received from the
				   debugged process */

/* Thread information structure used to track information that is
   not available in gdb's thread structure. */
typedef struct thread_info_struct
{
  struct thread_info_struct *next;
  DWORD id;
  HANDLE h;
  char *name;
  int suspend_count;
  CONTEXT context;
} thread_info;

static thread_info thread_head = {NULL};

/* The process and thread handles for the above context. */

static DEBUG_EVENT current_event;	/* The current debug event from
					   WaitForDebugEvent */
static HANDLE current_process_handle;	/* Currently executing process */
static thread_info *current_thread;	/* Info on currently selected thread */
static DWORD main_thread_id;		/* Thread ID of the main thread */

/* Counts of things. */
static int exception_count = 0;
static int event_count = 0;

/* User options. */
static int new_console = 0;
static int new_group = 0;
static int debug_exec = 0;		/* show execution */
static int debug_events = 0;		/* show events from kernel */
static int debug_memory = 0;		/* show target memory accesses */
static int debug_exceptions = 0;	/* show target exceptions */

/* This vector maps GDB's idea of a register's number into an address
   in the win32 exception context vector.

   It also contains the bit mask needed to load the register in question.

   One day we could read a reg, we could inspect the context we
   already have loaded, if it doesn't have the bit set that we need,
   we read that set of registers in using GetThreadContext.  If the
   context already contains what we need, we just unpack it. Then to
   write a register, first we have to ensure that the context contains
   the other regs of the group, and then we copy the info in and set
   out bit. */

#define context_offset(x) ((int)&(((CONTEXT *)NULL)->x))
static const int mappings[] =
{
  context_offset(Eax),
  context_offset(Ecx),
  context_offset(Edx),
  context_offset(Ebx),
  context_offset(Esp),
  context_offset(Ebp),
  context_offset(Esi),
  context_offset(Edi),
  context_offset(Eip),
  context_offset(EFlags),
  context_offset(SegCs),
  context_offset(SegSs),
  context_offset(SegDs),
  context_offset(SegEs),
  context_offset(SegFs),
  context_offset(SegGs),
  context_offset(FloatSave.RegisterArea[0 * 10]),
  context_offset(FloatSave.RegisterArea[1 * 10]),
  context_offset(FloatSave.RegisterArea[2 * 10]),
  context_offset(FloatSave.RegisterArea[3 * 10]),
  context_offset(FloatSave.RegisterArea[4 * 10]),
  context_offset(FloatSave.RegisterArea[5 * 10]),
  context_offset(FloatSave.RegisterArea[6 * 10]),
  context_offset(FloatSave.RegisterArea[7 * 10]),
};

/* This vector maps the target's idea of an exception (extracted
   from the DEBUG_EVENT structure) to GDB's idea. */

struct xlate_exception
  {
    int them;
    enum target_signal us;
  };

static const struct xlate_exception
  xlate[] =
{
  {EXCEPTION_ACCESS_VIOLATION, TARGET_SIGNAL_SEGV},
  {STATUS_STACK_OVERFLOW, TARGET_SIGNAL_SEGV},
  {EXCEPTION_BREAKPOINT, TARGET_SIGNAL_TRAP},
  {DBG_CONTROL_C, TARGET_SIGNAL_INT},
  {EXCEPTION_SINGLE_STEP, TARGET_SIGNAL_TRAP},
  {-1, -1}};

/* Find a thread record given a thread id.
   If get_context then also retrieve the context for this
   thread. */
static thread_info *
thread_rec (DWORD id, int get_context)
{
  thread_info *th;

  for (th = &thread_head; (th = th->next) != NULL; )
    if (th->id == id)
      {
	if (!th->suspend_count && get_context)
	  {
	    if (get_context > 0)
	      th->suspend_count = SuspendThread (th->h) + 1;
	    else if (get_context < 0)
	      th->suspend_count = -1;

	    th->context.ContextFlags = CONTEXT_DEBUGGER;
	    GetThreadContext (th->h, &th->context);
	  }
	return th;
      }

  return NULL;
}

/* Add a thread to the thread list */
static thread_info *
child_add_thread(DWORD id, HANDLE h)
{
  thread_info *th;

  if ((th = thread_rec (id, FALSE)))
    return th;

  th = (thread_info *) xmalloc (sizeof (*th));
  memset(th, 0, sizeof (*th));
  th->id = id;
  th->h = h;
  th->next = thread_head.next;
  thread_head.next = th;
  add_thread (id);
  return th;
}

/* Clear out any old thread list and reintialize it to a
   pristine state. */
static void
child_init_thread_list ()
{
  thread_info *th = &thread_head;

  DEBUG_EVENTS (("gdb: child_init_thread_list\n"));
  init_thread_list ();
  while (th->next != NULL)
    {
      thread_info *here = th->next;
      th->next = here->next;
      (void) CloseHandle (here->h);
      free (here);
    }
}

/* Delete a thread from the list of threads */
static void
child_delete_thread (DWORD id)
{
  thread_info *th;

  if (info_verbose)
    printf_unfiltered ("[Deleting %s]\n", target_pid_to_str (id));
  delete_thread (id);

  for (th = &thread_head;
       th->next != NULL && th->next->id != id;
       th = th->next)
    continue;

  if (th->next != NULL)
    {
      thread_info *here = th->next;
      th->next = here->next;
      CloseHandle (here->h);
      free (here);
    }
}

static void
check (BOOL ok, const char *file, int line)
{
  if (!ok)
    printf_filtered ("error return %s:%d was %d\n", file, line, GetLastError ());
}

static void
do_child_fetch_inferior_registers (int r)
{
  if (r >= 0)
    supply_register (r, ((char *) &current_thread->context) + mappings[r]);
  else
    {
      for (r = 0; r < NUM_REGS; r++)
	do_child_fetch_inferior_registers (r);
    }
}

static void
child_fetch_inferior_registers (int r)
{
  current_thread = thread_rec (inferior_pid, TRUE);
  do_child_fetch_inferior_registers (r);
}

static void
do_child_store_inferior_registers (int r)
{
  if (r >= 0)
    read_register_gen (r, ((char *) &current_thread->context) + mappings[r]);
  else
    {
      for (r = 0; r < NUM_REGS; r++)
	do_child_store_inferior_registers (r);
    }
}

/* Store a new register value into the current thread context */
static void
child_store_inferior_registers (int r)
{
  current_thread = thread_rec (inferior_pid, TRUE);
  do_child_store_inferior_registers (r);
}

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */

static int
handle_load_dll (PTR dummy)
{
  LOAD_DLL_DEBUG_INFO * event = &current_event.u.LoadDll;
  DWORD dll_name_ptr;
  DWORD done;
  char dll_buf[MAX_PATH + 1];
  char *p, *dll_name = NULL, *dll_basename;
  struct objfile *objfile;
  MEMORY_BASIC_INFORMATION minfo;

  dll_buf[0] = dll_buf[sizeof(dll_buf) - 1] = '\0';

  /* The following code attempts to find the name of the dll by reading the
     name from the processes memory.  Unfortunately it doesn't work right.
     Doing this the "right way" for Windows is very difficult.  FIXME */
#ifdef DOESNT_WORK
  memset (&minfo, 0, sizeof minfo);
  if (VirtualQueryEx (current_process_handle, (LPCVOID) event->lpBaseOfDll,
		      &minfo, sizeof(minfo)) && minfo.BaseAddress) {
      DWORD len;
      IMAGE_DOS_HEADER *hmm0 = (IMAGE_DOS_HEADER *) minfo.BaseAddress;
      HMODULE hmm = (HMODULE) (((DWORD) hmm0) + hmm0->e_lfanew);

      if ((len = GetModuleFileName (hmm, dll_buf, MAX_PATH)))
	{
	  dll_name = dll_buf;
	  dll_name[len] = '\0';
	}
  }
#endif

  /* Attempt to read the name of the dll that was detected.
     This is documented to work only when actively debugging
     a program.  It will not work for attached processes. */
  if (dll_name == NULL || *dll_name == '\0')
    {
      int size = event->fUnicode ? sizeof (WCHAR) : sizeof (char);
      int len = 0;
      char b[2];

      ReadProcessMemory (current_process_handle,
			 (LPCVOID) event->lpImageName,
			 (char *) &dll_name_ptr,
			 sizeof (dll_name_ptr), &done);

      /* See if we could read the address of a string, and that the
	 address isn't null. */

      if (done != sizeof (dll_name_ptr) || !dll_name_ptr)
	return 1;

      do
	{
	  ReadProcessMemory (current_process_handle,
			     (LPCVOID) (dll_name_ptr + len * size),
			     &b,
			     size,
			     &done);
	  len++;
	}
      while ((b[0] != 0 || b[size - 1] != 0) && done == size);

      dll_name = alloca (len);

      if (event->fUnicode)
	{
	  WCHAR *unicode_dll_name = (WCHAR *) alloca (len * sizeof (WCHAR));
	  ReadProcessMemory (current_process_handle,
			     (LPCVOID) dll_name_ptr,
			     unicode_dll_name,
			     len * sizeof (WCHAR),
			     &done);

	  WideCharToMultiByte (CP_ACP, 0,
			       unicode_dll_name, len,
			       dll_name, len, 0, 0);
	}
      else
	{
	  ReadProcessMemory (current_process_handle,
			     (LPCVOID) dll_name_ptr,
			     dll_name,
			     len,
			     &done);
	}
    }

  if (!dll_name)
    return 1;

  while ((p = strchr (dll_name, '\\')))
    *p = '/';

  /* FIXME!! It would be nice to define one symbol which pointed to the
     front of the dll if we can't find any symbols. */

  if (!(dll_basename = strrchr(dll_name, '/')))
    dll_basename = dll_name;
  else
    dll_basename++;

  ALL_OBJFILES(objfile)
    {
      char *objfile_basename;
      objfile_basename = strrchr(objfile->name, '/');

      if (objfile_basename &&
	  strcmp(dll_basename, objfile_basename + 1) == 0)
	{
	  printf_unfiltered ("%x:%s (symbols previously loaded)\n",
			     event->lpBaseOfDll, dll_name);
	  goto out;
	}
  }

  /* The symbols in a dll are offset by 0x1000, which is the
     the offset from 0 of the first byte in an image - because
     of the file header and the section alignment.

     FIXME: Is this the real reason that we need the 0x1000 ? */

  printf_unfiltered ("%x:%s", event->lpBaseOfDll, dll_name);
  symbol_file_add (dll_name, 0, (int) event->lpBaseOfDll + 0x1000, 0, 0, 0, 0, 1);
  printf_unfiltered ("\n");

out:
  return 1;
}

/* Handle DEBUG_STRING output from child process.
   Cygwin prepends its messages with a "cygwin:".  Interpret this as
   a Cygwin signal.  Otherwise just print the string as a warning. */
static int
handle_output_debug_string (struct target_waitstatus *ourstatus)
{
  char *s;
  int gotasig = FALSE;

  if (!target_read_string
      ((CORE_ADDR) current_event.u.DebugString.lpDebugStringData, &s, 1024, 0)
      || !s || !*s)
    return gotasig;

  if (strncmp(s, CYGWIN_SIGNAL_STRING, sizeof(CYGWIN_SIGNAL_STRING) - 1))
    {
      warning (s);
    }
  else
    {
      char *p;
      /*last_sig = */strtol(s + sizeof(CYGWIN_SIGNAL_STRING) - 1, &p, 0);
      if (gotasig = (ourstatus->value.sig = target_signal_from_host (last_sig)))
	ourstatus->kind = TARGET_WAITKIND_STOPPED;
    }

  free (s);
  return gotasig;
}

static int
handle_exception (struct target_waitstatus *ourstatus)
{
  int i;
  int done = 0;
  thread_info *th;

  ourstatus->kind = TARGET_WAITKIND_STOPPED;

  /* Record the context of the current thread */
  th = thread_rec (current_event.dwThreadId, -1);

  switch (current_event.u.Exception.ExceptionRecord.ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
      DEBUG_EXCEPT (("gdb: Target exception ACCESS_VIOLATION at 0x%08x\n",
		     current_event.u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      break;
    case STATUS_STACK_OVERFLOW:
      DEBUG_EXCEPT (("gdb: Target exception STACK_OVERFLOW at 0x%08x\n",
		     current_event.u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      break;
    case EXCEPTION_BREAKPOINT:
      DEBUG_EXCEPT (("gdb: Target exception BREAKPOINT at 0x%08x\n",
		     current_event.u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    case DBG_CONTROL_C:
      DEBUG_EXCEPT (("gdb: Target exception CONTROL_C at 0x%08x\n",
		     current_event.u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_INT;
      /* User typed CTRL-C.  Continue with this status */
      last_sig = SIGINT;	/* FIXME - should check pass state */
      break;
    case EXCEPTION_SINGLE_STEP:
      DEBUG_EXCEPT (("gdb: Target exception SINGLE_STEP at 0x%08x\n",
		     current_event.u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    default:
      /* This may be a structured exception handling exception.  In
	 that case, we want to let the program try to handle it, and
	 only break if we see the exception a second time.  */
      if (current_event.u.Exception.dwFirstChance)
	return 0;

      printf_unfiltered ("gdb: unknown target exception 0x%08x at 0x%08x\n",
			 current_event.u.Exception.ExceptionRecord.ExceptionCode,
			 current_event.u.Exception.ExceptionRecord.ExceptionAddress);
      ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
  exception_count++;
  return 1;
}

/* Resume all artificially suspended threads if we are continuing
   execution */
static BOOL
child_continue (DWORD continue_status, int id)
{
  int i;
  thread_info *th;
  BOOL res;

  DEBUG_EVENTS (("ContinueDebugEvent (cpid=%d, ctid=%d, DBG_CONTINUE);\n",
		 current_event.dwProcessId, current_event.dwThreadId));
  if (res = ContinueDebugEvent (current_event.dwProcessId,
				current_event.dwThreadId,
			        continue_status))
    for (th = &thread_head; (th = th->next) != NULL; )
      if (((id == -1) || (id == th->id)) && th->suspend_count)
	{
	  for (i = 0; i < th->suspend_count; i++)
	    (void) ResumeThread (th->h);
	  th->suspend_count = 0;
	}

  return res;
}

static int
child_wait (int pid, struct target_waitstatus *ourstatus)
{
  /* We loop when we get a non-standard exception rather than return
     with a SPURIOUS because resume can try and step or modify things,
     which needs a current_thread->h.  But some of these exceptions mark
     the birth or death of threads, which mean that the current thread
     isn't necessarily what you think it is. */

  while (1)
    {
      DWORD continue_status;
      BOOL debug_event = WaitForDebugEvent (&current_event, 20);
      char *p;
      thread_info *th;
      int sig;

      if (debug_event)
	{
	  event_count++;

	  continue_status = DBG_CONTINUE;

	  switch (current_event.dwDebugEventCode)
	    {
	    case CREATE_THREAD_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%x code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "CREATE_THREAD_DEBUG_EVENT"));
	      /* Record the existence of this thread */
	      child_add_thread (current_event.dwThreadId,
				current_event.u.CreateThread.hThread);
	      if (info_verbose)
		printf_unfiltered ("[New %s]\n",
				   target_pid_to_str (current_event.dwThreadId));
	      break;

	    case EXIT_THREAD_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "EXIT_THREAD_DEBUG_EVENT"));
	      child_delete_thread (current_event.dwThreadId);
	      break;

	    case CREATE_PROCESS_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "CREATE_PROCESS_DEBUG_EVENT"));
	      current_process_handle = current_event.u.CreateProcessInfo.hProcess;

	      main_thread_id = inferior_pid = current_event.dwThreadId;
	      /* Add the main thread */
	      current_thread = child_add_thread (inferior_pid,
						 current_event.u.CreateProcessInfo.hThread);
	      break;

	    case EXIT_PROCESS_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "EXIT_PROCESS_DEBUG_EVENT"));
	      ourstatus->kind = TARGET_WAITKIND_EXITED;
	      ourstatus->value.integer = current_event.u.ExitProcess.dwExitCode;
	      CloseHandle (current_process_handle);
	      return current_event.dwProcessId;
	      break;

	    case LOAD_DLL_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "LOAD_DLL_DEBUG_EVENT"));
	      catch_errors (handle_load_dll, NULL, "", RETURN_MASK_ALL);
	      registers_changed();          /* mark all regs invalid */
	      break;

	    case UNLOAD_DLL_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "UNLOAD_DLL_DEBUG_EVENT"));
	      break;	/* FIXME: don't know what to do here */

	    case EXCEPTION_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "EXCEPTION_DEBUG_EVENT"));
	      if (handle_exception (ourstatus))
		return current_event.dwThreadId;
	      continue_status = DBG_EXCEPTION_NOT_HANDLED;
	      break;

	    case OUTPUT_DEBUG_STRING_EVENT: /* message from the kernel */
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     current_event.dwProcessId, current_event.dwThreadId,
			     "OUTPUT_DEBUG_STRING_EVENT"));
	      if (handle_output_debug_string (ourstatus))
		return main_thread_id;
	      break;
	    default:
	      printf_unfiltered ("gdb: kernel event for pid=%d tid=%d\n",
				 current_event.dwProcessId,
				 current_event.dwThreadId);
	      printf_unfiltered ("                 unknown event code %d\n",
				 current_event.dwDebugEventCode);
	      break;
	    }

	  CHECK (child_continue (continue_status, -1));
	}
      else
	{
	  int detach = 0;

	  if (ui_loop_hook != NULL)
	    detach = ui_loop_hook (0);

	  if (detach)
	    child_kill_inferior ();
	}
    }
}

/* Attach to process PID, then initialize for debugging it.  */

static void
child_attach (args, from_tty)
     char *args;
     int from_tty;
{
  BOOL ok;

  if (!args)
    error_no_arg ("process-id to attach");

  current_event.dwProcessId = strtoul (args, 0, 0);

  ok = DebugActiveProcess (current_event.dwProcessId);

  if (!ok)
    error ("Can't attach to process.");

  exception_count = 0;
  event_count = 0;

  if (from_tty)
    {
      char *exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', %s\n", exec_file,
			   target_pid_to_str (current_event.dwProcessId));
      else
	printf_unfiltered ("Attaching to %s\n",
			   target_pid_to_str (current_event.dwProcessId));

      gdb_flush (gdb_stdout);
    }

  push_target (&child_ops);
}

static void
child_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s %s\n", exec_file,
			 target_pid_to_str (inferior_pid));
      gdb_flush (gdb_stdout);
    }
  inferior_pid = 0;
  unpush_target (&child_ops);
}

/* Print status information about what we're accessing.  */

static void
child_files_info (ignore)
     struct target_ops *ignore;
{
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
      attach_flag ? "attached" : "child", target_pid_to_str (inferior_pid));
}

/* ARGSUSED */
static void
child_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

/* Start an inferior win32 child process and sets inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().  */

static void
child_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  char real_path[MAXPATHLEN];
  char *winenv;
  char *temp;
  int  envlen;
  int i;

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  struct target_waitstatus dummy;
  BOOL ret;
  DWORD flags;
  char *args;

  if (!exec_file)
    {
      error ("No executable specified, use `target exec'.\n");
    }

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);

  cygwin32_conv_to_win32_path (exec_file, real_path);

  flags = DEBUG_ONLY_THIS_PROCESS;

  if (new_group)
    flags |= CREATE_NEW_PROCESS_GROUP;

  if (new_console)
    flags |= CREATE_NEW_CONSOLE;

  args = alloca (strlen (real_path) + strlen (allargs) + 2);

  strcpy (args, real_path);

  strcat (args, " ");
  strcat (args, allargs);

  /* Prepare the environment vars for CreateProcess.  */
  {
    /* This code use to assume all env vars were file names and would
       translate them all to win32 style.  That obviously doesn't work in the
       general case.  The current rule is that we only translate PATH.
       We need to handle PATH because we're about to call CreateProcess and
       it uses PATH to find DLL's.  Fortunately PATH has a well-defined value
       in both posix and win32 environments.  cygwin.dll will change it back
       to posix style if necessary.  */

    static const char *conv_path_names[] =
      {
	"PATH=",
	0
      };

    /* CreateProcess takes the environment list as a null terminated set of
       strings (i.e. two nulls terminate the list).  */

    /* Get total size for env strings.  */
    for (envlen = 0, i = 0; env[i] && *env[i]; i++)
      {
	int j, len;

	for (j = 0; conv_path_names[j]; j++)
	  {
	    len = strlen (conv_path_names[j]);
	    if (strncmp (conv_path_names[j], env[i], len) == 0)
	      {
		if (cygwin32_posix_path_list_p (env[i] + len))
		  envlen += len
		    + cygwin32_posix_to_win32_path_list_buf_size (env[i] + len);
		else
		  envlen += strlen (env[i]) + 1;
		break;
	      }
	  }
	if (conv_path_names[j] == NULL)
	  envlen += strlen (env[i]) + 1;
      }

    winenv = alloca (envlen + 1);

    /* Copy env strings into new buffer.  */
    for (temp = winenv, i = 0; env[i] && *env[i]; i++)
      {
	int j, len;

	for (j = 0; conv_path_names[j]; j++)
	  {
	    len = strlen (conv_path_names[j]);
	    if (strncmp (conv_path_names[j], env[i], len) == 0)
	      {
		if (cygwin32_posix_path_list_p (env[i] + len))
		  {
		    memcpy (temp, env[i], len);
		    cygwin32_posix_to_win32_path_list (env[i] + len, temp + len);
		  }
		else
		  strcpy (temp, env[i]);
		break;
	      }
	  }
	if (conv_path_names[j] == NULL)
	  strcpy (temp, env[i]);

	temp += strlen (temp) + 1;
      }

    /* Final nil string to terminate new env.  */
    *temp = 0;
  }

  ret = CreateProcess (0,
		       args, 	/* command line */
		       NULL,	/* Security */
		       NULL,	/* thread */
		       TRUE,	/* inherit handles */
		       flags,	/* start flags */
		       winenv,
		       NULL,	/* current directory */
		       &si,
		       &pi);
  if (!ret)
    error ("Error creating process %s, (error %d)\n", exec_file, GetLastError());

  exception_count = 0;
  event_count = 0;

  current_process_handle = pi.hProcess;
  current_event.dwProcessId = pi.dwProcessId;
  memset (&current_event, 0, sizeof (current_event));
  inferior_pid = current_event.dwThreadId = pi.dwThreadId;
  push_target (&child_ops);
  child_init_thread_list ();
  init_wait_for_inferior ();
  clear_proceed_status ();
  target_terminal_init ();
  target_terminal_inferior ();

  /* Ignore the first trap */
  child_wait (inferior_pid, &dummy);

  proceed ((CORE_ADDR) - 1, TARGET_SIGNAL_0, 0);
}

static void
child_mourn_inferior ()
{
  (void) child_continue (DBG_CONTINUE, -1);
  unpush_target (&child_ops);
  generic_mourn_inferior ();
}

/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal. */

static void
child_stop ()
{
  DEBUG_EVENTS (("gdb: GenerateConsoleCtrlEvent (CTRLC_EVENT, 0)\n"));
  CHECK (GenerateConsoleCtrlEvent (CTRL_C_EVENT, 0));
  registers_changed();		/* refresh register state */
}

int
child_xfer_memory (CORE_ADDR memaddr, char *our, int len,
		   int write, struct target_ops *target)
{
  DWORD done;
  if (write)
    {
      DEBUG_MEM (("gdb: write target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      WriteProcessMemory (current_process_handle, (LPVOID) memaddr, our,
			  len, &done);
      FlushInstructionCache (current_process_handle, (LPCVOID) memaddr, len);
    }
  else
    {
      DEBUG_MEM (("gdb: read target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      ReadProcessMemory (current_process_handle, (LPCVOID) memaddr, our, len,
			 &done);
    }
  return done;
}

void
child_kill_inferior (void)
{
  CHECK (TerminateProcess (current_process_handle, 0));

  for (;;)
    {
      if (!child_continue (DBG_CONTINUE, -1))
	break;
      if (!WaitForDebugEvent (&current_event, INFINITE))
	break;
      if (current_event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
	break;
    }

  CHECK (CloseHandle (current_process_handle));

  /* this may fail in an attached process so don't check. */
  (void) CloseHandle (current_thread->h);
  target_mourn_inferior();	/* or just child_mourn_inferior? */
}

void
child_resume (int pid, int step, enum target_signal sig)
{
  int i;
  thread_info *th;
  DWORD continue_status = last_sig > 0 && last_sig < NSIG ?
			  DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE;

  DEBUG_EXEC (("gdb: child_resume (pid=%d, step=%d, sig=%d);\n",
	       pid, step, sig));

  /* Get context for currently selected thread */
  th = thread_rec (current_event.dwThreadId, FALSE);
  if (step)
    {
#ifdef i386
      /* Single step by setting t bit */
      child_fetch_inferior_registers (PS_REGNUM);
      th->context.EFlags |= FLAG_TRACE_BIT;
#endif
    }

  if (th->context.ContextFlags)
    {
      CHECK (SetThreadContext (th->h, &th->context));
      th->context.ContextFlags = 0;
    }

  /* Allow continuing with the same signal that interrupted us.
     Otherwise complain. */
  if (sig && sig != last_sig)
    fprintf_unfiltered (gdb_stderr, "Can't send signals to the child.  signal %d\n", sig);

  last_sig = 0;
  child_continue (continue_status, pid);
}

static void
child_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static int
child_can_run ()
{
  return 1;
}

static void
child_close ()
{
  DEBUG_EVENTS (("gdb: child_close, inferior_pid=%d\n", inferior_pid));
}

struct target_ops child_ops ;

static void 
init_child_ops(void)
{
  child_ops.to_shortname =   "child";
  child_ops.to_longname =   "Win32 child process";
  child_ops.to_doc =   "Win32 child process (started by the \"run\" command).";
  child_ops.to_open =   child_open;
  child_ops.to_close =   child_close;
  child_ops.to_attach =   child_attach;
  child_ops.to_detach =   child_detach;
  child_ops.to_resume =   child_resume;
  child_ops.to_wait  =   child_wait;
  child_ops.to_fetch_registers  =   child_fetch_inferior_registers;
  child_ops.to_store_registers  =   child_store_inferior_registers;
  child_ops.to_prepare_to_store =   child_prepare_to_store;
  child_ops.to_xfer_memory  =   child_xfer_memory;
  child_ops.to_files_info  =   child_files_info;
  child_ops.to_insert_breakpoint =   memory_insert_breakpoint;
  child_ops.to_remove_breakpoint =   memory_remove_breakpoint;
  child_ops.to_terminal_init  =   terminal_init_inferior;
  child_ops.to_terminal_inferior =   terminal_inferior;
  child_ops.to_terminal_ours_for_output =   terminal_ours_for_output;
  child_ops.to_terminal_ours  =   terminal_ours;
  child_ops.to_terminal_info  =   child_terminal_info;
  child_ops.to_kill  =   child_kill_inferior;
  child_ops.to_load  =   0;
  child_ops.to_lookup_symbol =   0;
  child_ops.to_create_inferior =   child_create_inferior;
  child_ops.to_mourn_inferior =   child_mourn_inferior;
  child_ops.to_can_run  =   child_can_run;
  child_ops.to_notice_signals =   0;
  child_ops.to_thread_alive  =   win32_child_thread_alive;
  child_ops.to_stop  =   child_stop;
  child_ops.to_stratum =   process_stratum;
  child_ops.DONT_USE =   0;
  child_ops.to_has_all_memory =   1;
  child_ops.to_has_memory =   1;
  child_ops.to_has_stack =   1;
  child_ops.to_has_registers =   1;
  child_ops.to_has_execution =   1;
  child_ops.to_sections =   0;
  child_ops.to_sections_end =   0;
  child_ops.to_magic =   OPS_MAGIC;
}

void
_initialize_inftarg ()
{
  struct cmd_list_element *c;
  init_child_ops() ;

  add_show_from_set
    (add_set_cmd ("new-console", class_support, var_boolean,
		  (char *) &new_console,
		  "Set creation of new console when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("new-group", class_support, var_boolean,
		  (char *) &new_group,
		  "Set creation of new group when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugexec", class_support, var_boolean,
		  (char *) &debug_exec,
		  "Set whether to display execution in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugevents", class_support, var_boolean,
		  (char *) &debug_events,
		  "Set whether to display kernel events in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugmemory", class_support, var_boolean,
		  (char *) &debug_memory,
		  "Set whether to display memory accesses in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugexceptions", class_support, var_boolean,
		  (char *) &debug_exceptions,
		  "Set whether to display kernel exceptions in child process.",
		  &setlist),
     &showlist);

  add_target (&child_ops);
}

/* Determine if the thread referenced by "pid" is alive
   by "polling" it.  If WaitForSingleObject returns WAIT_OBJECT_0
   it means that the pid has died.  Otherwise it is assumed to be alive. */
static int
win32_child_thread_alive (int pid)
{
  return WaitForSingleObject(thread_rec (pid, FALSE)->h, 0) == WAIT_OBJECT_0 ?
	 FALSE : TRUE;
}

/* Convert pid to printable format. */
char *
cygwin_pid_to_str (int pid)
{
  static char buf[80];
  if (pid == current_event.dwProcessId)
    sprintf (buf, "process %d", pid);
  else
    sprintf (buf, "thread %d.0x%x", current_event.dwProcessId, pid);
  return buf;
}
