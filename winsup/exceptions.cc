/* exceptions.cc

   Copyright 1996, 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdio.h>
#include <errno.h>

#define Win32_Winsock
#include "winsup.h"

#include "exceptions.h"

extern int (*i_WSACleanup) (void) PASCAL;

extern "C" {
void stack (void);
static int handle_exceptions (EXCEPTION_RECORD *, void *, CONTEXT *, void *);
};

static BOOL ctrl_c_handler (DWORD);
static void really_exit (int);

/* This is set to indicate that we have already exited.  */

static NO_COPY int exit_already = 0;

HANDLE NO_COPY hMainThread = NULL;	/* A handle to the main thread */

static const struct
{
  unsigned int code;
  const char *name;
} status_info[] NO_COPY =
{
#define X(s) s, #s
  { X (STATUS_ABANDONED_WAIT_0) },
  { X (STATUS_ACCESS_VIOLATION) },
  { X (STATUS_ARRAY_BOUNDS_EXCEEDED) },
  { X (STATUS_BREAKPOINT) },
  { X (STATUS_CONTROL_C_EXIT) },
  { X (STATUS_DATATYPE_MISALIGNMENT) },
  { X (STATUS_FLOAT_DENORMAL_OPERAND) },
  { X (STATUS_FLOAT_DIVIDE_BY_ZERO) },
  { X (STATUS_FLOAT_INEXACT_RESULT) },
  { X (STATUS_FLOAT_INVALID_OPERATION) },
  { X (STATUS_FLOAT_OVERFLOW) },
  { X (STATUS_FLOAT_STACK_CHECK) },
  { X (STATUS_FLOAT_UNDERFLOW) },
  { X (STATUS_GUARD_PAGE_VIOLATION) },
  { X (STATUS_ILLEGAL_INSTRUCTION) },
  { X (STATUS_INTEGER_DIVIDE_BY_ZERO) },
  { X (STATUS_INTEGER_OVERFLOW) },
  { X (STATUS_INVALID_DISPOSITION) },
  { X (STATUS_IN_PAGE_ERROR) },
  { X (STATUS_NONCONTINUABLE_EXCEPTION) },
  { X (STATUS_NO_MEMORY) },
  { X (STATUS_PENDING) },
  { X (STATUS_PRIVILEGED_INSTRUCTION) },
  { X (STATUS_SINGLE_STEP) },
  { X (STATUS_STACK_OVERFLOW) },
  { X (STATUS_TIMEOUT) },
  { X (STATUS_USER_APC) },
  { X (STATUS_WAIT_0) },
  { 0, 0 }
#undef X
};

/* Initialization code.  */

#ifdef __i386__

// Set up the exception handler for the current thread.  The PowerPC & Mips
// use compiler generated tables to set up the exception handlers for each
// region of code, and the kernel walks the call list until it finds a region
// of code that handles exceptions.  The x86 on the other hand uses segment
// register fs, offset 0 to point to the current exception handler.

asm (".equ __except_list,0");

extern exception_list *_except_list asm ("%fs:__except_list");

static void
init_exception_handler (exception_list *el)
{
  el->handler = handle_exceptions;
  el->prev = _except_list;
  _except_list = el;
}

#define INIT_EXCEPTION_HANDLER(el) init_exception_handler (el)
#endif

void
set_console_handler ()
{
  (void) SetConsoleCtrlHandler (ctrl_c_handler, FALSE);
  if (!SetConsoleCtrlHandler (ctrl_c_handler, TRUE))
    system_printf ("SetConsoleCtrlHandler failed, %E");
}

extern "C"
void
init_exceptions (exception_list *el)
{
#ifdef INIT_EXCEPTION_HANDLER
  INIT_EXCEPTION_HANDLER (el);
#endif
}

void
signals_init ()
{
  set_console_handler ();
  DuplicateHandle (GetCurrentProcess (),
	     GetCurrentThread (),
	     GetCurrentProcess (),
	     &hMainThread,
	     0,
	     FALSE, /* Don't want kids to see thread id */
	     DUPLICATE_SAME_ACCESS);

  ProtectHandle (hMainThread);
}

/* Utilities for dumping the stack, etc.  */

static void
exception (EXCEPTION_RECORD *e,  CONTEXT *in)
{
#ifdef __i386__
#define HAVE_STATUS
  system_printf ("trapped!");
  system_printf ("code %p at %p", e->ExceptionCode, in->Eip);
  system_printf ("ax %p bx %p cx %p dx %p",
		 in->Eax, in->Ebx, in->Ecx, in->Edx);
  system_printf ("si %p di %p bp %p sp %p",
		 in->Esi, in->Edi, in->Ebp, in->Esp);
#endif

#ifndef HAVE_STATUS
  system_printf ("Had an exception");
#endif

  if (e)
    {
      for (int i = 0; status_info[i].name; i++)
	{
	  if (status_info[i].code == e->ExceptionCode)
	    {
	      system_printf ("exception is: %s", status_info[i].name);
	      break;
	    }
	}
    }
}

extern "C" {
#ifdef __i386__
/* Print a stack backtrace.
   This is defined as a C function so it's callable from C.
   Please keep the output the same across all platforms.  */

#define HAVE_STACK_TRACE
void
stack (void)
{
  register int i;
  register char **ebp;

  asm ("mov %%ebp,%0": "=r" (ebp):);

  system_printf ("Stack trace:");
  for (i = 0; i < 16; ++i)
    {
      system_printf ("frame %d: sp = %p, pc = %p",
		     i, ebp, ebp[1]);
      /* Top of stack?  */
      if (ebp[1] == 0)
	break;
      ebp = ((char ***) ebp)[0];
    }
  system_printf ("End of stack trace%s",
		 i == 16 ? " (more stack frames may be present)" : "");
}

/* Main exception handler. */

static int
handle_exceptions (EXCEPTION_RECORD *e, void *arg, CONTEXT *in, void *x)
{
  int sig;

  /* If we've already exited, don't do anything here.  Returning 1
     tells Windows to keep looking for an exception handler.  */
  if (exit_already)
    return 1;

  /* Coerce win32 value to posix value.  */
  switch (e->ExceptionCode)
    {
    case STATUS_FLOAT_DENORMAL_OPERAND:
    case STATUS_FLOAT_DIVIDE_BY_ZERO:
    case STATUS_FLOAT_INEXACT_RESULT:
    case STATUS_FLOAT_INVALID_OPERATION:
    case STATUS_FLOAT_OVERFLOW:
    case STATUS_FLOAT_STACK_CHECK:
    case STATUS_FLOAT_UNDERFLOW:
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
    case STATUS_INTEGER_OVERFLOW:
      sig = SIGFPE;
      break;

    case STATUS_ILLEGAL_INSTRUCTION:
    case STATUS_PRIVILEGED_INSTRUCTION:
    case STATUS_NONCONTINUABLE_EXCEPTION:
      sig = SIGILL;
      break;

    case STATUS_TIMEOUT:
      sig = SIGALRM;
      break;

    case STATUS_ACCESS_VIOLATION:
    case STATUS_DATATYPE_MISALIGNMENT:
    case STATUS_ARRAY_BOUNDS_EXCEEDED:
    case STATUS_GUARD_PAGE_VIOLATION:
    case STATUS_IN_PAGE_ERROR:
    case STATUS_NO_MEMORY:
    case STATUS_INVALID_DISPOSITION:
    case STATUS_STACK_OVERFLOW:
      sig = SIGSEGV;
      break;

    case STATUS_CONTROL_C_EXIT:
      sig = SIGINT;
      break;

    case STATUS_INVALID_HANDLE:
      /* CloseHandle will throw this exception if it is given an
	 invalid handle.  We don't care about the exception; we just
	 want CloseHandle to return an error.  This can be revisited
	 if gcc ever supports Windows style structured exception
	 handling.  */
      return 0;

    default:
      /* If we don't recognize the exception, we have to assume that
	 we are doing structured exception handling, and we let
	 something else handle it.  */
      return 1;
    }

  debug_printf ("In cygwin_except_handler exc %p at %p sp %p", e->ExceptionCode, in->Eip, in->Esp);
  debug_printf ("In cygwin_except_handler sig = %d at %p", sig, in->Eip);

  if (myself->getsig(sig).sa_mask & SIGTOMASK (sig))
    syscall_printf ("signal %d, masked %p", sig, myself->getsig(sig).sa_mask);

  if (!myself->progname[0]
      || (void *) myself->getsig(sig).sa_handler == (void *) SIG_DFL
      || (void *) myself->getsig(sig).sa_handler == (void *) SIG_IGN
      || (void *) myself->getsig(sig).sa_handler == (void *) SIG_ERR)
    {
      static NO_COPY int traced = 0;

      /* Print the exception to the console */
      if (e)
	{
	  for (int i = 0; status_info[i].name; i++)
	    {
	      if (status_info[i].code == e->ExceptionCode)
		{
		  system_printf ("Exception: %s", status_info[i].name);
		  break;
		}
	    }
	}

      /* Another exception could happen while tracing or while exiting.
	 Only do this once.  */
      if (traced++)
	{
	  system_printf ("Error while dumping state (probably corrupted stack)");
	}
      else
	{
	  char *p;
	  if (myself->progname[0])
	    {
	      /* write to progname.core if possible */
	      if ((p = strrchr (myself->progname, '\\')))
		p++;
	      else
		p = myself->progname;
	      char corefile[strlen(p) + sizeof(".core")];
	      __small_sprintf (corefile, "%s.core", p);
	      HANDLE h = CreateFile (corefile, GENERIC_WRITE, 0, &sec_none_nih,
				     CREATE_ALWAYS, 0, 0);
	      if (h != INVALID_HANDLE_VALUE)
		{
		  system_printf ("Dumping stack trace to %s", corefile);
		  SetStdHandle (STD_ERROR_HANDLE, h);
		}
	    }
	  exception (e, in);
	  stack ();
	}
      really_exit (EXIT_SIGNAL | sig);
    }

  debug_printf ("In cygwin_except_handler calling %p",
		 myself->getsig(sig).sa_handler);

  sig_send (myself_nowait, sig);		// Signal myself
  return 0;
}
#endif /* __i386__ */

#ifndef HAVE_STACK_TRACE
void
stack (void)
{
  system_printf ("Stack trace not yet supported on this machine.");
}
#endif
}

/* Utilities to call a user supplied exception handler.  */

#define SIG_NONMASKABLE	(SIGTOMASK (SIGCONT) | SIGTOMASK (SIGKILL) | SIGTOMASK (SIGSTOP))

#ifdef __i386__
#define HAVE_CALL_HANDLER

/* Set the signal mask for this process.
 * Note that some signals are unmaskable, as in UNIX.
 */
extern "C"
void
set_process_mask (sigset_t newmask, int sync)
{
  (void) get_signal_mutex ();		// Get signal mutex, avoid dispatch
  newmask &= ~SIG_NONMASKABLE;
  sigproc_printf ("old mask = %x, new mask = %x", myself->getsigmask (), newmask);
  myself->setsigmask (newmask);	// Set a new mask
  release_signal_mutex (sync);			// Give up mutex
  return;
}

/* Non-raceable(?) sigsuspend
 * Note: This implementation is based on the Single UNIX Specification
 * man page.  This indicates that sigsuspend always returns -1 and that
 * attempts to block unblockable signals will be silently ignored.
 * This is counter to what appears to be documented in some UNIX
 * man pages, e.g. Linux.
 */
int
handle_sigsuspend (sigset_t tempmask)
{
  sigset_t oldmask;
  int newerrno;

  ResetEvent (signal_arrived);	// Interested in any signal after this point
  (void) get_signal_mutex ();	// Disallow signals so I can do stuff.
  oldmask = myself->getsigmask ();	// Remember for restoration
  myself->setsigmask (tempmask & ~SIG_NONMASKABLE);// Let signals we're
				//  interested in through.
  sigproc_printf ("old mask %x, new mask %x", oldmask, tempmask);
  /* Wake up signal handler thread to process any blocked signals.
     Wait for a signal to arrive.
   */
  if (!sig_send (NULL, __SIGSUSPEND))
    newerrno = EINTR;		// Per POSIX
  else
    newerrno = ENOSYS;		// Some other error
  set_errno (newerrno);
  set_process_mask (oldmask, -1);// Restore previous mask
  release_signal_mutex (1);	// Allow wait_sig thread to deliver the
				// signal.  IMPORTANT: Make sure that
				// this happens synchronously before this
				// function returns.  That's why the arg
				// is a `1'.
  return -1;
}

static int
call_handler (int sig, struct sigaction& thissig)
{
  CONTEXT orig;
  int res;
  extern DWORD exec_exit;		// Possible exit value for exec
  extern void get_strace_mutex ();
  extern void release_strace_mutex ();

  SetEvent (signal_arrived);		// For an EINTR case

  if (hExeced != NULL)
    {
      exec_exit = sig;			// Maybe we'll exit with this value
      return 1;
    }

  switch (get_signal_mutex (10))
  {
    case WAIT_OBJECT_0:
      goto got_mutex;
    case WAIT_SIG_EXITING:
      ExitThread (0);
      break;
    case WAIT_TIMEOUT:
      if (WaitForSingleObject (signal_arrived, 0) != WAIT_OBJECT_0)
	{
	  SetEvent (signal_arrived);	// work around potential race?
	  break;
	}
    default:
      goto mutex_error;
  }

  /* Only dispatch to handler if we can get the signal_mutex.
   * Otherwise put the signal back into sigtodo and let someone else
   * worry about it.
   */
  switch (get_signal_mutex ())
  {
    case WAIT_OBJECT_0:
      break;
    case WAIT_SIG_EXITING:
      ExitThread (0);
      break;
    default:
    mutex_error:
      sigproc_printf ("couldn't get sig_mutex lock for signal %d", sig);
      sig_set_pending (sig);
      return -1;		// would have dispatched
  }

got_mutex:
  /* Suspend the running thread, grab its context somewhere safe
     and run the exception handler in the context of the thread -
     we have to do that since sometimes they don't return - and if
     this thread doesn't return, you won't ever get another exception. */

  sigproc_printf ("Suspending %p (mainthread)", myself->getthread2signal());
  if (strace ())
    get_strace_mutex ();
  res = SuspendThread (myself->getthread2signal());
  if (strace ())
    release_strace_mutex ();
  sigproc_printf ("suspend said %d, %E", res);

  /* Clear any waiting threads prior to dispatching to handler function */
  proc_subproc(PROC_CLEARWAIT, 0);

  sigset_t oldmask = myself->getsigmask ();
  myself->setsigmask (myself->getsigmask () | thissig.sa_mask | SIGTOMASK (sig));

  orig.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  if (!GetThreadContext (myself->getthread2signal(), &orig))
    {
      system_printf ("couldn't get context of main thread, %E");
      sig_set_pending (sig);
      return -2;		// would have dispatched
    }

  unsigned int *sp = (unsigned int *) orig.Esp;
  *(--sp) = orig.Eip;	/*  original IP where program was suspended */
  orig.Eip = (unsigned) &&wrap;

  *(--sp) = oldmask;
  *(--sp) = sig;
  *(--sp) = (DWORD) thissig.sa_handler;
  orig.Esp = (DWORD) sp;
  SetThreadContext (myself->getthread2signal(), &orig); /* Restart the thread */
  ResumeThread (myself->getthread2signal());
  release_signal_mutex (-1);
  sigproc_printf ("returning");
  return 1;

  /* This code is run in the standard thread space */
  /* Commented out instructions are emulated already */
 wrap:
  asm ("
##    pushl	%eip
##    pushl	_oldmask
##    pushl	_sigarg
##    pushl	_sigfunc
      pusha
      pushf
##    orl	$0x0,(%esp)		# probe out enough room to play
##    orl	$0x0,-0x800(%esp)
##    orl	$0x0,-0x1000(%esp)
      pushl	%ebp
      movl	%esp,%ebp
  ");

  strace_protect.set (0);

  asm ("
      pushl	0xb*4(%ebp)		# sigarg
      movl	0xa*4(%ebp),%eax	# sigfunc
      call	*%eax
      pushl	$-1
      pushl	0xc*4(%ebp)		# oldmask
      call	_set_process_mask
      leave
      popf
      popa
      popl	-4(%esp)
      popl	-4(%esp)
      popl	-4(%esp)
      ret
  ");

  return 1;
}
#endif /* i386 */

#ifndef HAVE_CALL_HANDLER
#error "Need to supply machine dependent call_handler"
#endif

/* Keyboard interrupt handler.  */
static BOOL
ctrl_c_handler (DWORD type)
{
  int sig;
  BOOL ret;

  if (type == CTRL_LOGOFF_EVENT)
    return 1;

  if ((type == CTRL_CLOSE_EVENT) || (type == CTRL_SHUTDOWN_EVENT))
    /* Return FALSE to prevent an "End task" dialog box from appearing
       for each Cygwin process window that's open when the computer
       is shut down or console window is closed. */
    {
      sig = SIGHUP;
      ret = FALSE;
    }
  else
    /* Otherwise we just send a SIGINT and return TRUE (to indicate
       that we have handled the signal).  At this point, type should be
       a CTRL_C_EVENT or CTRL_BREAK_EVENT. */
    {
      sig = SIGINT;
      ret = TRUE;
    }

  sigproc_printf ("type %d, signal %d", type, sig);
  sig_send (myself_nowait, sig);
  return ret;
}

int __last_sig;
int
sig_handle (int sig)
{
  int rc = 0;

  __last_sig = sig;
  sigproc_printf ("signal %d", sig);

  get_signal_mutex (1);
  struct sigaction thissig = myself->getsig(sig);
  myself->rusage_self.ru_nsignals++;
  release_signal_mutex (-1);

  /* Clear pending SIGCONT on stop signals */
  if (sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU)
    sig_clear (SIGCONT);

  if (sig == SIGKILL)
    goto exit_sig;
  if (sig == SIGSTOP)
    goto stop;
  if (sig == SIGCONT)
    {
      myself->stopsig = 0;
      myself->process_state &= ~PID_STOPPED;
      /* Clear pending stop signals */
      sig_clear (SIGSTOP);
      sig_clear (SIGTSTP);
      sig_clear (SIGTTIN);
      sig_clear (SIGTTOU);
      /* Windows 95 hangs on resuming non-suspended thread */
      SuspendThread (hMainThread);
      while (ResumeThread (hMainThread) > 1)
	;
      /* process pending signals */
      sig_dispatch_pending ();
    }

  char sigmsg[24];
  __small_sprintf (sigmsg, "cygwin: signal %d\n", sig);
  OutputDebugString (sigmsg);

  if ((void *) thissig.sa_handler == (void *) SIG_DFL)
    {
      if (sig == SIGCHLD || sig == SIGIO || sig == SIGCONT || sig == SIGWINCH)
	{
	  sigproc_printf ("default signal %d ignored", sig);
	  goto done;
	}
      if (sig != SIGSTOP && sig != SIGTSTP && sig != SIGTTIN && sig != SIGTTOU)
	goto exit_sig;

stop:
      /* Do not stop processes with pgid == 0.  These are top-level
	 processes that were not invoked by parent cygwin processes. */
      if (myself->parent_alive)
	{
	  HANDLE waitbuf[2];

	  /* Be sure that process's main thread isn't an owner of vital
	     mutex to prevent cygwin subsystem lockups */

	  waitbuf[0] = pinfo_mutex;
	  waitbuf[1] = title_mutex;
	  WaitForMultipleObjects (2, waitbuf, TRUE, INFINITE);
	  SuspendThread (hMainThread);
	  myself->stopsig = sig;
	  myself->process_state |= PID_STOPPED;
	  ReleaseMutex (pinfo_mutex);
	  ReleaseMutex (title_mutex);
	  /* See if we have a living parent.  If so, send it a special signal.
	   * It will figure out exactly which pid has stopped by scanning
	   * its list of subprocesses.
	   */
	  if (alive_parent(myself))
	    {
	      pinfo *parent = procinfo(myself->ppid);
	      sig_send (parent, __SIGCHILDSTOPPED);
	    }
	}
      sigproc_printf ("process %d stopped by signal %d, parent_alive %p",
		      myself->pid, sig, myself->parent_alive);
      goto done;
    }

  if ((void *) thissig.sa_handler == (void *) SIG_IGN)
    {
      sigproc_printf ("signal %d ignored", sig);
      goto done;
    }

  if ((void *) thissig.sa_handler == (void *) SIG_ERR)
    {
    exit_sig:
      sigproc_printf ("signal %d, about to call do_exit", sig);
      do_exit (EXIT_SIGNAL | (sig << 8));
      /* Never returns */
    }

  if ((sig == SIGCHLD) && (thissig.sa_flags & SA_NOCLDSTOP))
    goto done;

  /* Dispatch to the appropriate function. */

  sigproc_printf ("signal %d, about to call %p", sig, thissig.sa_handler);
  rc = call_handler (sig, thissig);

done:
  sigproc_printf ("returning %d", rc);
  return rc;
}

/* Cover function to `do_exit' to handle exiting even in presence of more
   exceptions.  We use to call exit, but a SIGSEGV shouldn't cause atexit
   routines to run.  */

static void
really_exit (int rc)
{
  /* If the exception handler gets a trap, we could recurse awhile.
     If this is non-zero, skip the cleaning up and exit NOW.  */

  if (exit_already++)
    {
    /* We are going down - reset our process_state without locking. */
    myself->record_death_nolock ();
    ExitProcess (rc);
    }

  do_exit (rc);
}

HANDLE NO_COPY pinfo_mutex;
HANDLE NO_COPY title_mutex;

void
events_init (void)
{
  /* pinfo_mutex protects access to process table */

  if (!(pinfo_mutex = CreateMutex (&sec_all_nih, FALSE,
				   shared_name ("pinfo_mutex", 0))))
    api_fatal ("catastrophic failure - unable to create pinfo_mutex, %E");

  ProtectHandle (pinfo_mutex);

  /* title_mutex protects modification of console title. It's neccessary
     while finding console window handle */

  if (!(title_mutex = CreateMutex (&sec_all_nih, FALSE,
				   shared_name ("title_mutex", 0))))
    api_fatal ("can't create title mutex, %E");

  ProtectHandle (title_mutex);
}

void
events_terminate (void)
{
//CloseHandle (pinfo_mutex);	// Use implicit close on exit to avoid race
  ForceCloseHandle (title_mutex);
  exit_already = 1;
}
