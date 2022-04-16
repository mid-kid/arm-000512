/* sigproc.cc: inter/intra signal and sub process handler

   Copyright 1997, 1998, 1999 Cygnus Solutions.

   Written by Christopher Faylor <cgf@cygnus.com>

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include "winsup.h"

/*
 * Convenience defines
 */
#define WSSC		   60000 // Wait for signal completion
#define WPSP		   40000 // Wait for proc_subproc mutex
#define WSPX		   20000 // Wait for wait_sig to terminate
#define WWSP		   20000 // Wait for wait_subproc to terminate

#define WAIT_SIG_PRIORITY		THREAD_PRIORITY_HIGHEST

#define TOTSIGS	(NSIG + __SIGOFFSET)

#define sip_printf(fmt, args...) sigproc_printf (fmt , ## args)

/* How to determine if a process is accepting messages.
 */
#define proc_alive(p) \
  (((p)->process_state & (PID_ACTIVE | PID_IN_USE)) ==\
   (PID_ACTIVE | PID_IN_USE))

#define wake_wait_subproc() SetEvent (events[0])

/*
 * Global variables
 */
const char *__sp_fn ;
int __sp_ln;

char NO_COPY myself_nowait_dummy[1] = {'0'};// Flag to sig_send that signal goes to
					//  current process but no wait is required
char NO_COPY myself_nowait_nonmain_dummy[1] = {'1'};// Flag to sig_send that signal goes to
					//  current process but no wait is required
					//  if this is not the main thread.

HANDLE NO_COPY signal_arrived = NULL;	// Event signaled when a signal has
					//  resulted in a user-specified
					//  function call
HANDLE NO_COPY signal_mutex = NULL;	// Block dispatching to functions
/*
 * Common variables
 */


/* How long to wait for message/signals.  Normally this is infinite.
 * On termination, however, these are set to zero as a flag to exit.
 */

#define Static static NO_COPY

Static DWORD proc_loop_wait = INFINITE;	// Wait for subprocesses to exit
Static DWORD sig_loop_wait = INFINITE;	// Wait for signals to arrive

Static HANDLE sigcatch_nonmain = NULL;	// The semaphore signaled when
					//  signals are available for
					//  processing from non-main thread
Static HANDLE sigcatch_main = NULL;	// Signalled when main thread sends a
					//  signal
Static HANDLE sigcatch_nosync = NULL;	// Signal wait_sig to scan sigtodo
					//  but not to bother with any
					//  synchronization
Static HANDLE sigcomplete_main = NULL;	// Event signaled when a signal has
					//  finished processing for the main
					//  thread
Static HANDLE sigcomplete_nonmain = NULL;// Semaphore raised for non-main
					//  threads when a signal has finished
					//  processing
Static HANDLE hwait_sig = NULL;		// Handle of wait_sig thread
Static HANDLE hwait_subproc = NULL;	// Handle of sig_subproc thread

Static HANDLE wait_sig_inited = NULL;	// Control synchronization of
					//  message queue startup
Static HANDLE sync_proc_subproc = NULL;	// Control access to
					//  subproc stuff

/* Used by WaitForMultipleObjects.  These are handles to child processes.
 */
Static HANDLE events[PSIZE + 1];	// All my children's handles++
#define hchildren (events + 1)		// Where the children handles begin
Static pinfo *pchildren[PSIZE];		// All my children info
Static pinfo *zombies[PSIZE];		// All my deceased children info
Static int nchildren = 0;		// Number of active children
Static int nzombies = 0;		// Number of deceased children

Static waitq waitq_head = {0};		// Start of queue for wait'ing threads
Static waitq waitq_main;		// Storage for main thread

DWORD NO_COPY maintid;			// ID of the main thread
Static DWORD sigtid;			// ID of the signal thread

Static int pending_signals = 0;		// TRUE if signals pending

/* Functions
 */
static int checkstate (waitq *);
static BOOL __inline get_proc_lock (DWORD, DWORD);
static HANDLE getsem (pinfo *, const char *, int, int);
#ifdef NOSTRACE
#define proc_strace() 0
#else
static void proc_strace (void);
#endif
static void remove_child (int);
static void remove_zombie (int);
static DWORD wait_sig (VOID *arg);
static int stopped_or_terminated (waitq *, pinfo *);
static DWORD wait_subproc (VOID *);
static void zap_subproc (pinfo *);

/* Determine if the parent process is alive.
 */

BOOL
alive_parent (pinfo *p)
{
  DWORD res;
  if (!(p)->parent_alive)
    {
      debug_printf ("No parent_alive mutex");
      res = FALSE;
    }
  else
    for (int i = 0; i < 2; i++)
      switch (res = WaitForSingleObject ((p)->parent_alive, 0))
	{
	  case WAIT_OBJECT_0:
	    debug_printf ("parent dead.");
	    res = FALSE;
	    break;
	  case WAIT_TIMEOUT:
	    debug_printf ("parent still alive");
	    res = TRUE;
	    break;
	  case WAIT_FAILED:
	    DWORD werr = GetLastError ();
	    if (werr == ERROR_INVALID_HANDLE && i == 0)
	      continue;
	    system_printf ("WFSO for parent_alive(%p) failed, error %d",
			   (p)->parent_alive, werr);
	    res = FALSE;
	    break;
	}
  return res;
}

/* Test to determine if a process really exists and is processing
 * signals.
 */
BOOL
proc_exists (pinfo *p)
{
  HANDLE h;

  if (p == NULL)
    return FALSE;

  if (p == myself || p == myself_nowait_nonmain || p == myself_nowait)
    return TRUE;

  if (p->process_state == PID_NOT_IN_USE || !p->dwProcessId)
    return FALSE;

  sip_printf ("checking for existence of pid %d, window pid %d", p->pid,
	      p->dwProcessId);
  if (p->parent_alive && p->ppid == myself->pid && p->hProcess != NULL)
    {
      sip_printf ("it's mine, process_state %x", p->process_state);
      return ISSTATE (p, PID_INITIALIZING) || proc_alive (p);
    }

  /* Note: Process is alive if OpenProcess() call fails due to permissions */
  if (((h = OpenProcess (STANDARD_RIGHTS_REQUIRED, FALSE, p->dwProcessId))
      != NULL) || (GetLastError () == ERROR_ACCESS_DENIED))
    {
      sip_printf ("it exists");
      if (h)
	ForceCloseHandle (h);
      return ISSTATE (p, PID_INITIALIZING) || proc_alive (p);
    }

  sip_printf ("it doesn't exist");
  /* If the parent pid does not exist, clean this process out of the pinfo
   * table.  It must have died abnormally.
   */
  if (!p->parent_alive || !proc_exists (procinfo (p->ppid)))
    {
      p->hProcess = NULL;
      p->process_state = PID_NOT_IN_USE;
    }
  return FALSE;
}

/* Handle all subprocess requests
 */
#define vchild ((pinfo *) val)
int
proc_subproc (DWORD what, DWORD val)
{
  int rc = 1;
  int potential_match;
  DWORD exitcode;
  pinfo *child;
  waitq *w;

#define wval	 ((waitq *) val)

  sip_printf ("args: %x, %d", what, val);

  block_sig_dispatch ();		// No signals please

  if (!get_proc_lock (what, val))	// Serialize access to this function
    {
      sip_printf ("I am not ready");
      goto out1;
    }

  switch (what)
    {
    /* Add a new subprocess to the children arrays.
     * (usually called from the main thread)
     */
    case PROC_ADDCHILD:
      if (nchildren >= PSIZE - 1)
	system_printf ("nchildren too large %d", nchildren);
      if (WaitForSingleObject (vchild->hProcess, 0) != WAIT_TIMEOUT)
	{
	  system_printf ("invalid process handle %p.  pid %d, win pid %d",
			vchild->hProcess, vchild->pid, vchild->dwProcessId);
	  rc = 0;
	  break;
	}

      pchildren[nchildren] = vchild;
      hchildren[nchildren] = vchild->hProcess;
      ProtectHandle (vchild->hProcess);
      sip_printf ("added pid %d to wait list, slot %d, winpid %p, handle %p",
		  vchild->pid, nchildren, vchild->dwProcessId,
		  vchild->hProcess);

      /* Child inherits the wait_subproc handle.  It is used to determine if
       * the parent is still alive and processing subprocess termination.
       * If its parent is not alive, then the child will handle the task
       * of removing itself from the pinfo list.
       */
      vchild->parent_alive = hwait_subproc;
      nchildren++;
      wake_wait_subproc ();
      break;

    /* A child is in the stopped state.  Scan wait() queue to see if anyone
     * should be notified.  (Called from wait_sig thread)
     */
    case PROC_CHILDSTOPPED:
      child = myself;		// Just to avoid accidental NULL dereference
      sip_printf ("Received stopped notification");
      goto scan_wait;

    /* A child process had terminated.
     * Possibly this is just due to an exec().  Cygwin implements an exec()
     * as a "handoff" from one windows process to another.  If child->hProcess
     * is different from what is recorded in hchildren, then this is an exec().
     * Otherwise this is a normal child termination event.
     * (called from wait_subproc thread)
     */
    case PROC_CHILDTERMINATED:
      rc = 0;
      child = pchildren[val];
      if (GetExitCodeProcess (hchildren[val], &exitcode) &&
	  hchildren[val] != child->hProcess)
	{
	  sip_printf ("pid %d[%d], reparented old hProcess %p, new %p",
		      child->pid, val, hchildren[val], child->hProcess);
	  ForceCloseHandle (hchildren[val]);
	  hchildren[val] = child->hProcess; /* Filled out by child */
	  wake_wait_subproc ();
	  break;			// This was an exec()
	}

      sip_printf ("pid %d[%d] terminated, handle %p, nchildren %d, nzombies %d",
		  child->pid, val, hchildren[val], nchildren, nzombies);
      remove_child (val);		// Remove from children arrays
      zombies[nzombies++] = child;	// Add to zombie array
      wake_wait_subproc ();		// Notify wait_subproc thread that
					//  nchildren has changed.
      child->process_state = PID_ZOMBIE;// Walking dead
      if (!proc_loop_wait)		// Don't bother if wait_subproc is
	break;				//  exiting

      /* The POSIX spec seems to subtly imply that a SIGCHLD should be
       * sent prior to any wait()s being activated.  So, allow recursively
       * calling this function, and send a SIGCHLD to ourself.
       */
      (void) ReleaseMutex (sync_proc_subproc);
      rc = sig_send (NULL, SIGCHLD);
      if (!get_proc_lock (what, val))	// Get the lock again
	goto out1;			// Ouch.

    scan_wait:
      /* Scan the linked list of wait()ing threads.  If a wait's parameters
       * match this pid, then activate it.
       */
      for (w = &waitq_head; w->next != NULL; w = w->next)
	{
	  if ((potential_match = checkstate (w)) > 0)
	    sip_printf ("released waiting thread");
	  else if (potential_match < 0)
	    sip_printf ("only found non-terminated children");
	  else if (potential_match == 0)		// nothing matched
	    {
	      sip_printf ("waiting thread found no children");
	      HANDLE oldw = w->next->ev;
	      w->next->ev = NULL;
	      if (!SetEvent (oldw))
		system_printf ("couldn't wake up wait event %p, %E", oldw);
	      w->next = w->next->next;
	    }
	  if (w->next == NULL)
	    break;
	}

      sip_printf ("finished processing terminated/stopped child");
      break;

    /* Clear all waiting threads.  Called from exceptions.cc prior to
     * the main thread's dispatch to a signal handler function.
     * (called from wait_sig thread)
     */
    case PROC_CLEARWAIT:
      /* Clear all "wait"ing threads. */
      sip_printf ("clear waiting threads");
      for (w = &waitq_head; w->next != NULL; w = w->next)
	{
	  sip_printf ("clearing waiting thread, pid %d", w->next->pid);
	  w->next->status = -1;		/* flag that a signal was received */
	  if (!SetEvent (w->next->ev))
	    system_printf ("Couldn't wake up wait event, %E");
	}
      waitq_head.next = NULL;
      sip_printf ("finished clearing");
      break;

    /* Handle a wait4() operation.  Allocates an event for the calling
     * thread which is signaled when the appropriate pid exits or stops.
     * (usually called from the main thread)
     */
    case PROC_WAIT:
      wval->ev = NULL;		// Don't know event flag yet

      if (wval->pid <= 0)
	child = NULL;		// Not looking for a specific pid
      else if ((child = procinfo (wval->pid)) == NULL)
	goto out;		// invalid pid.  flag no such child

      wval->status = 0;		// Don't know status yet

      /* Put waitq structure at the end of a linked list. */
      for (w = &waitq_head; w->next != NULL; w = w->next)
	if (w->next == wval && (w->next = w->next->next) == NULL)
	  break;

      wval->next = NULL;	/* This will be last in the list */
      sip_printf ("wval->pid %d, wval->options %d", wval->pid, wval->options);

      /* If the first time for this thread, create a new event, otherwise
       * reset the event.
       */
      if ((wval->ev = wval->thread_ev) == NULL)
	{
	  wval->ev = wval->thread_ev = CreateEvent (&sec_none_nih, TRUE,
						    FALSE, NULL);
	  ProtectHandle (wval->ev);
	}
      ResetEvent (wval->ev);

      /* Scan list of children to see if any have died.
       * If so, the event flag is set so that the wait* ()
       * process will return immediately.
       *
       * If no children were found and the wait option was WNOHANG,
       * then set the pid to 0 and remove the waitq value from
       * consideration.
       */
      w->next = wval;		/* set at end of wait queue */
      if ((potential_match = checkstate (w)) <= 0)
	{
	  if (!potential_match)
	    {
	      w->next = NULL;		// don't want to keep looking
	      wval->ev = NULL;		// flag that there are no children
	      sip_printf ("no appropriate children, %p, %p",
			  wval->thread_ev, wval->ev);
	    }
	  else if (wval->options & WNOHANG)
	    {
	      w->next = NULL;		// don't want to keep looking
	      wval->pid = 0;		// didn't find a pid
	      if (!SetEvent (wval->ev))	// wake up wait4 () immediately
		system_printf ("Couldn't wake up wait event, %E");
	      sip_printf ("WNOHANG and no terminated children, %p, %p",
			  wval->thread_ev, wval->ev);
	    }
	}
      if (w->next != NULL)
	sip_printf ("wait activated %p, %p", wval->thread_ev, wval->ev);
      else if (wval->ev != NULL)
	sip_printf ("wait activated %p.  Reaped zombie.", wval->ev);
      else
	sip_printf ("wait not activated %p, %p", wval->thread_ev, wval->ev);
      break;
  }

out:
  (void) ReleaseMutex (sync_proc_subproc);	// Release the lock
out1:
  allow_sig_dispatch (TRUE);			// Allow signals
  sip_printf ("returning %d", rc);
  return rc;
}

/* Terminate the wait_subproc thread.
 * Called on process exit.
 * Also called by spawn_guts to disassociate any subprocesses from this
 * process.  Subprocesses will then know to clean up after themselves and
 * will not become zombies.
 */
void
proc_terminate (void)
{
  sip_printf ("nchildren %d, nzombies %d", nchildren, nzombies);
  /* Signal processing is assumed to be blocked in this routine. */
  if (hwait_subproc)
    {
      int rc;
      lock_pinfo_for_update (INFINITE);
      proc_loop_wait = 0;	// Tell wait_subproc thread to exit
      wake_wait_subproc ();	// Wake wait_subproc loop

      /* Wait for wait_subproc thread to exit (but not *too* long) */
      if ((rc = WaitForSingleObject (hwait_subproc, WWSP)) != WAIT_OBJECT_0)
	if (rc == WAIT_TIMEOUT)
	  system_printf ("WFSO(hwait_subproc) timed out");
	else
	  system_printf ("WFSO(hwait_subproc), rc %d, %E", rc);
      ForceCloseHandle (hwait_subproc);
      hwait_subproc = NULL;

      (void) WaitForSingleObject (sync_proc_subproc, WPSP);
      (void) proc_subproc (PROC_CLEARWAIT, 0);

      /* Clean out zombie processes from the pid list. */
      int i;
      for (i = 0; i < nzombies; i++)
	{
	  pinfo *child;
	  if ((child = zombies[i])->hProcess)
	    {
	      ForceCloseHandle (child->hProcess);
	      child->hProcess = NULL;
	    }
	  child->process_state = PID_NOT_IN_USE;
	}

      /* Disassociate my subprocesses */
      for (i = 0; i < nchildren; i++)
	{
	  pinfo *child;
	  if ((child = pchildren[i])->process_state == PID_NOT_IN_USE)
	    continue;		// Should never happen
	  if (!child->hProcess)
	    sip_printf ("%d(%d) hProcess cleared already?", child->pid,
			child->dwProcessId);
	  else if (proc_exists (child))
	    {
	      sip_printf ("%d(%d) closing active child handle", child->pid,
			  child->dwProcessId);
	      ForceCloseHandle (child->hProcess);
	      child->hProcess = NULL;
	    }
	  else
	    {
	      sip_printf ("%d(%d) doesn't exist", child->pid,
			  child->dwProcessId);
	      zap_subproc (child);
	      continue;
	    }
	  child->ppid = 1;
	  child->parent_alive = NULL;
	  if (child->pgid == myself->pid)
	    child->process_state |= PID_ORPHANED;
	}
      nchildren = nzombies = 0;

      /* Attempt to close and release sync_proc_subproc in a
       * non-raceable manner.
       */
      HANDLE h = sync_proc_subproc;
      sync_proc_subproc = NULL;
      ReleaseMutex (h);
      ForceCloseHandle (h);
      unlock_pinfo ();
    }
  sip_printf ("leaving");
}

/* Clear pending signal from the sigtodo array
 */
void
sig_clear (int sig)
{
  (void) InterlockedExchange (myself->getsigtodo(sig), 0L);
  return;
}

/* Force the wait_sig thread to wake up and scan the sigtodo array.
 */
void
sig_dispatch_pending (int force)
{
  if (!hwait_sig)
    return;

  int was_pending = pending_signals;
#ifdef DEBUGGING
  sip_printf ("pending_signals %d", was_pending);
#endif
  if (!was_pending && !force)
#ifdef DEBUGGING
    sip_printf ("no need to wake anything up");
#else
    ;
#endif
  else
    {
      /* Wait for wait_sig to finish initializing, if appropriate */
      if (wait_sig_inited)
	{
	  (void) WaitForSingleObject (wait_sig_inited, INFINITE);
	  (void) ForceCloseHandle (wait_sig_inited);
	  wait_sig_inited = NULL;
	}
      if (ReleaseSemaphore (sigcatch_nosync, 1, NULL))
#ifdef DEBUGGING
	sip_printf ("woke up wait_sig");
#else
	;
#endif
      else if (!hwait_sig)
	/*sip_printf ("I'm going away now")*/;
      else
	system_printf ("%E releasing sigcatch_nosync(%p)", sigcatch_nosync);
    }
  return;
}

/* Message initialization.  Called from dll_crt0_1
 *
 * This routine starts the signal handling thread.  The wait_sig_inited
 * event is used to signal that the thread is ready to handle signals.
 * We don't wait for this during initialization but instead detect it
 * in sig_send to gain a little concurrency.
 */
void
sigproc_init (void)
{
  extern void signals_init ();

  signals_init ();		// stuff used by exceptions.cc

  wait_sig_inited = CreateEvent (&sec_none_nih, FALSE, FALSE, NULL);
  ProtectHandle (wait_sig_inited);

  /* signal_arrived	   - local event signaled when main thread has been
   *			     dispatched to a signal handler function.o
   */
  signal_arrived = CreateEvent (&sec_none_nih, TRUE, FALSE, NULL);
  ProtectHandle (signal_arrived);

  maintid = GetCurrentThreadId ();// For use in determining if signals
				  //  should be blocked.

  if ((signal_mutex = CreateMutex (&sec_none_nih, FALSE, NULL)) == NULL)
    api_fatal ("Can't create signal mutex (signal_mutex), %E");
  ProtectHandle (signal_mutex);

  if (!(hwait_sig = makethread (wait_sig, NULL, 0, "sig")))
    {
      system_printf ("cannot create wait_sig thread, %E");
      api_fatal ("terminating");
    }

  ProtectHandle (hwait_sig);

  /* sync_proc_subproc is used by proc_subproc.  It serialises
   * access to the children and zombie arrays.
   */
  if ((sync_proc_subproc = CreateMutex (&sec_none_nih, FALSE, NULL)) == NULL)
    {
      system_printf ("cannot create sync_proc_subproc mutex, %E");
      api_fatal ("terminating");
    }

  ProtectHandle (sync_proc_subproc);

  /* Initialize waitq structure for main thread.  A waitq structure is
   * allocated for each thread that executes a wait to allow multiple threads
   * to perform waits.  Pre-allocate a waitq structure for the main thread.
   */
  waitq *w;
  if ((w = (waitq *)waitq_storage.get ()) == NULL)
    {
      w = &waitq_main;
      waitq_storage.set (w);
    }
  memset (w, 0, sizeof *w);	// Just to be safe

  sip_printf ("process/signal handling enabled(%x)", myself->process_state);
  return;
}

/* Called on process termination to terminate signal and process threads.
 */
void
sigproc_terminate (void)
{
  if (!sig_loop_wait)
    sip_printf ("sigproc_terminate: sigproc handling not active");
  else
    {
      sigproc_printf ("entering");
      sig_loop_wait = 0;	// Tell wait_sig to exit when it is
				//  finished with anything it is doing
      block_sig_dispatch ();	// Avoid all signals
      ForceCloseHandle (signal_mutex); // Wake up anyone waiting for this.

      /* If !hwait_sig, then the process probably hasn't even finished
       * its initialization phase.
       */
      if (hwait_sig)
	{
	  ForceCloseHandle (hwait_sig);
	  hwait_sig = NULL;

	  /* Exiting thread.  Cleanup.  Don't set to inactive if a child has been
	     execed with the same pid. */
	  if (!myself->dwProcessId || myself->dwProcessId == GetCurrentProcessId ())
	    myself->process_state &= ~PID_ACTIVE;
	  else
	    sip_printf ("Did not clear PID_ACTIVE since %d != %d",
			myself->dwProcessId, GetCurrentProcessId ());

	  /* In case of a sigsuspend */
	  SetEvent (signal_arrived);

	  ForceCloseHandle (sigcatch_main);
	  ForceCloseHandle (sigcatch_nonmain);
	  ForceCloseHandle (sigcatch_nosync);
	  ForceCloseHandle (sigcomplete_main);
	  ForceCloseHandle (sigcomplete_nonmain);
	}
      proc_terminate ();		// Terminate process handling thread
      sip_printf ("done");
    }

  /* Set this so that subsequent tests will succeed. */
  if (!myself->dwProcessId)
    myself->dwProcessId = GetCurrentProcessId ();

  return;
}

/* Send a signal to another process by raising its signal semaphore.
 * If pinfo *p == NULL, send to the current process.
 * If sending to this process, wait for notification that a signal has
 * completed before returning.
 * If not sending to this process, grab the sig_dispatch mutex to prevent
 * the signal handler from dispatching to a function which may never return.
 * Otherwise, we'd get handle leaks.
 */
int
sig_send (pinfo *p, int sig)
{
  int rc = 1;
  DWORD tid = GetCurrentThreadId ();
  BOOL its_me;
  HANDLE thiscatch = NULL;
  HANDLE thiscomplete = NULL;
  BOOL wait_for_completion;

  /* FIXME: need to protect access of pinfo -- there's currently a
     serious race condition here even though it doesn't seem to be
     actively biting us now. */

  if (p == myself_nowait_nonmain)
    p = (tid == maintid) ? myself : myself_nowait;
  if (!(its_me = (p == NULL || p == myself || p == myself_nowait)))
    wait_for_completion = FALSE;
  else
    {
      if (!sig_loop_wait || !hwait_sig)
	goto out;		// Either exiting or not yet initializing
      /* See if this is the first signal call after initialization.
       * If so, wait for notification that all initialization has completed.
       * Then set the handle to NULL to avoid checking this again.
       */
      if (wait_sig_inited)
	{
	  (void) WaitForSingleObject (wait_sig_inited, INFINITE);
	  (void) ForceCloseHandle (wait_sig_inited);
	  wait_sig_inited = NULL;
	}
      wait_for_completion = p != myself_nowait;
      p = myself;
    }

  /* It is possible that the process is not yet ready to receive messages
   * or that it has exited.  Detect this.
   */
  if (!proc_alive (p))	/* Is the process accepting messages? */
    {
      sip_printf ("invalid pid %d(%x), signal %d",
		  p->pid, p->process_state, sig);
      set_errno (ESRCH);
      goto out;
    }

  sip_printf ("pid %d, signal %d, its_me %d", p->pid, sig, its_me);

  if (its_me)
    {
      if (!wait_for_completion)
	thiscatch = sigcatch_nosync;
      else if (sig == __SIGSUSPEND)
	{
	  thiscatch = sigcatch_nosync;
	  /* wait_sig will scan for signals when sigcatch_nosync is
	     released. */
	}
      else if (tid != maintid)
	{
	  thiscatch = sigcatch_nonmain;
	  thiscomplete = sigcomplete_nonmain;
	}
      else
	{
	  thiscatch = sigcatch_main;
	  thiscomplete = sigcomplete_main;
	  ResetEvent (thiscatch);
	  ResetEvent (thiscomplete);
	}
    }
  else if ((thiscatch = getsem (p, "sigcatch", 0, 0)))
    block_sig_dispatch ();	// We've got the handle, don't allow signals
				//  (see above)
  else
    goto out;			// Couldn't get the semaphore.  getsem issued
				//  an error, if appropriate.

  /* Increment the sigtodo array to signify which signal to assert.
   */
  (void) InterlockedIncrement (p->getsigtodo(sig));
  if (its_me)
    pending_signals = 1;

  /* Notify the process that a signal has arrived.
   */
  if (!ReleaseSemaphore (thiscatch, 1, NULL))
    {
      /* Couldn't signal the semaphore.  This probably means that the
       * process is exiting.
       */
      if (!its_me)
	{
	  ForceCloseHandle (thiscatch);
	  allow_sig_dispatch (TRUE);
	}
      else
	{
	  if (!hwait_sig)
	    sip_printf ("I'm going away now");
	  else
	    system_printf ("error sending signal(%d) to pid %d, %E",
			  sig, p->pid);
	}
      goto out;
    }

  /* No need to wait for signal completion unless this was a signal to
   * this process.
   *
   * If it was a signal to this process, wait for a dispatched signal
   * if this was __SIGSUSPEND.  Otherwise just wait for the wait_sig to
   * signal that it has finished processing the signal.
   */
  if (!wait_for_completion)
    {
    rc = WAIT_OBJECT_0;
    sip_printf ("Not waiting for sigcomplete.  its_me %d sig %d", its_me, sig);
    if (!its_me)
      {
	ForceCloseHandle (thiscatch);
	allow_sig_dispatch (TRUE);
      }
    }
  else if (sig == __SIGSUSPEND)
    {
      sip_printf ("Waiting for signal_arrived %p", signal_arrived);
      rc = WaitForSingleObject (signal_arrived, INFINITE);
    }
  else
    {
      sip_printf ("Waiting for thiscomplete %p", thiscomplete);
      rc = WaitForSingleObject (thiscomplete, WSSC);
      /* Check for strangeness due to this thread being redirected by the
	 signal handler.  Sometimes a WAIT_TIMEOUT will occur when the
	 thread hasn't really timed out.  So, check again.
	 FIXME: This isn't foolproof. */
      if (rc != WAIT_OBJECT_0 &&
	  WaitForSingleObject (thiscomplete, 0) == WAIT_OBJECT_0)
	rc = WAIT_OBJECT_0;
    }

  if (rc == WAIT_OBJECT_0)
    rc = 0;		// Successful exit
  else
    {
      /* It's an error unless sig_loop_wait == 0 (the process is exiting). */
      if (sig_loop_wait)
	system_printf ("wait for %s_complete event failed, sig %d, rc %d, %E",
		      (sig == __SIGSUSPEND) ? "suspend" : "sig", sig, rc);
      set_errno (ENOSYS);
      rc = -1;
    }

out:
  sip_printf ("returning %d from sending signal %d", rc, sig);
  return rc;
}

/* Set pending signal from the sigtodo array
 */
void
sig_set_pending (int sig)
{
  (void) InterlockedIncrement (myself->getsigtodo(sig));
  return;
}

/* Initialize the wait_subproc thread.
 * Called from fork() or spawn() to initialize the handling of subprocesses.
 */
void
subproc_init (void)
{
  if (hwait_subproc)
    return;

  /* A "wakeup" handle which can be toggled to make wait_subproc reexamine
   * the hchildren array.
   */
  events[0] = CreateEvent (&sec_none_nih, FALSE, FALSE, NULL);
  if (!(hwait_subproc = makethread (wait_subproc, NULL, 0, "+proc")))
    system_printf ("cannot create wait_subproc thread, %E");
  ProtectHandle (events[0]);
  ProtectHandle (hwait_subproc);
  sip_printf ("started wait_subproc thread %p", hwait_subproc);
}

/* Allow sig_handle to dispatch signals to their handler functions.
 */
void
__allow_sig_dispatch (const char *fn, int ln, int synchronize)
{
  DWORD tid;
  /* There is no need to worry about being interrupted by a signal if
   * we are not executing in the main thread since only the main thread
   * will be interrupted.
   */
  if (hwait_sig == NULL)
    /* sip_printf ("process is exiting")*/;
  else if ((tid = GetCurrentThreadId ()) != maintid)
   /*  sip_printf ("current thread id %p != maintid %p", tid, maintid)*/;
  else
   __release_signal_mutex (fn, ln, synchronize);
}


/* Block sig_handle from dispatching signals to their handler functions.
 */
void
__block_sig_dispatch (const char *fn, int ln)
{
  /* There is no need to worry about being interrupted by a signal if
   * we are not executing in the main thread since only the main thread
   * will be interrupted.
   */
  if (hwait_sig == NULL)
    sip_printf ("process is exiting");
  else if (GetCurrentThreadId () != maintid)
    sip_printf ("current thread id %p != maintid %p",
		GetCurrentThreadId (), maintid);
  else
    {
      sip_printf ("waiting for signal_mutex (%p)", signal_mutex);
      switch (__get_signal_mutex (fn, ln))
      {
	case WAIT_OBJECT_0:
	case WAIT_SIG_EXITING:
	  return;
	default:
	  system_printf ("WFSO(signal_mutex<%p>) failed, %E", signal_mutex);
      }
    }
}

class mutex_stack
{
public:
  const char *fn;
  int ln;
} mstack[100];

int msi = 0;

DWORD
__get_signal_mutex (const char *fn, int ln, DWORD howlong)
{
  if (hExeced)
    return WAIT_OBJECT_0;
  DWORD res = WaitForSingleObject (signal_mutex, howlong);
  if (res == WAIT_OBJECT_0)
    {
      mstack[msi].fn = fn;
      mstack[msi].ln = ln;
      msi++;
    }
  else if (!sig_loop_wait)
    return WAIT_SIG_EXITING;
  else if (howlong >= 1000)
    {
      small_printf ("****** %s:%d __get_signal_mutex failed, res %d, %E\n", fn, ln, res);
      for (int i = 0; i < msi; i++)
	small_printf("  %d) function %s, line %d\n", i, mstack[i].fn, mstack[i].ln);
    }
  
#ifdef DEBUGGING
  if (res == WAIT_OBJECT_0)
    sip_printf ("acquired for %s:%d", fn, ln);
  else
    sip_printf ("not acquired for %s:%d, res %d, %E", fn, ln, res);
#endif
  return res;
}

void
__release_signal_mutex (const char *fn, int ln, int waitforit)
{
  if (hExeced || !sig_loop_wait)
    return;

  int released = 0;

  if (ReleaseMutex (signal_mutex))
    released = 1;
  else
    released = 0;

#if 0
  if (released && ReleaseMutex (signal_mutex))
    {
      small_printf ("****** %s:%d\n", fn, ln);
      for (int i = 0; i < msi; i++)
	small_printf("  %d) function %s, line %d\n", i, mstack[i].fn, mstack[i].ln);
      if (msi > 0)
	msi--;
    }
#endif

  if (msi > 0)
    msi--;

  if (released)
    sip_printf ("released for %s:%d", fn, ln);
  else
    sip_printf ("%s(%d) did not have mutex", fn, ln);
  if (waitforit < 0)
    return;

  save_errno err;
  if (waitforit && pending_signals)
    sig_send (myself_nowait_nonmain, __SIGFLUSH);
  else
    sig_dispatch_pending ();
}

/* Check the state of all of our children to see if any are stopped or
 * terminated.
 */
static int
checkstate (waitq *w)
{
  int i, x, potential_match = 0;
  pinfo *child;

  sip_printf ("nchildren %d, nzombies %d", nchildren, nzombies);

  /* Check already dead processes first to see if they match the criteria
   * given in w->next.
   */
  for (i = 0; i < nzombies; i++)
    if ((x = stopped_or_terminated (w, child = zombies[i])) < 0)
      potential_match = -1;
    else if (x > 0)
      {
	remove_zombie (i);
	potential_match = 1;
	goto out;
      }

  sip_printf ("checking alive children");

  /* No dead terminated children matched.  Check for stopped children. */
  for (i = 0; i < nchildren; i++)
    if ((x = stopped_or_terminated (w, pchildren[i])) < 0)
      potential_match = -1;
    else if (x > 0)
      {
	potential_match = 1;
	break;
      }

out:
  sip_printf ("returning %d", potential_match);
  return potential_match;
}

/* Get or create a process specific semaphore used in message passing.
 */
static HANDLE
getsem (pinfo *p, const char *str, int init, int max)
{
  HANDLE h;

  if (p != NULL && !proc_alive (p))
    {
      set_errno (ESRCH);
      return NULL;
    }

  SetLastError (0);
  if (p == NULL)
    {
      DWORD winpid = GetCurrentProcessId ();
      h = CreateSemaphore (&sec_none_nih, init, max, str = shared_name (str, winpid));
      p = myself;
    }
  else
    {
      h = OpenSemaphore (SEMAPHORE_ALL_ACCESS, FALSE,
			 str = shared_name (str, p->dwProcessId));

      if (h == NULL)
	{
	  if (GetLastError () == ERROR_FILE_NOT_FOUND && !proc_exists (p))
	    set_errno (ESRCH);
	  else
	    set_errno (EPERM);
	  return NULL;
	}
    }

  if (!h)
    {
      system_printf ("can't %s %s, %E", p ? "open" : "create", str);
      set_errno (ESRCH);
    }
  return h;
}

/* Get the sync_proc_subproc mutex to control access to
 * children, zombie arrays.
 * Attempt to handle case where process is exiting as we try to grab
 * the mutex.
 */
static BOOL __inline
get_proc_lock (DWORD what, DWORD val)
{
  DWORD rc;
  Static int lastwhat = -1;
  if (!sync_proc_subproc)
    return FALSE;
  if ((rc = WaitForSingleObject (sync_proc_subproc, WPSP)) == WAIT_OBJECT_0)
    {
      lastwhat = what;
      return TRUE;
    }
  if (!sync_proc_subproc)
    return FALSE;
  system_printf ("WFSO(sync_proc_subproc) %d for(%d,%d), %E, last %d",
		rc, what, val, lastwhat);
  return TRUE;
}

#ifndef NOSTRACE
/* Send __SIGSTRACE signal to children.
 */
static void
proc_strace (void)
{
  int i;

  for (i = 0; i < nchildren; i++)
    sip_printf ("child %d, handle %p, has not terminated",
		pchildren[i]->pid, hchildren[i]);
  for (i = 0; i < nzombies; i++)
    sip_printf ("child %d, handle %p, has terminated", zombies[i]->pid,
		zombies[i]->hProcess);

  strace_dump ();
  for (i = 0; i < nchildren; i++)
    sig_send (pchildren[i], __SIGSTRACE);
}
#endif

/* Remove a child from pchildren/hchildren by swapping it with the
 * last child in the list.
 */
static void
remove_child (int ci)
{
  sip_printf ("removing [%d], pid %d, handle %p, nchildren %d",
	      ci, pchildren[ci]->pid, hchildren[ci], nchildren);
  if (ci < --nchildren)
    {
      pchildren[ci] = pchildren[nchildren];
      hchildren[ci] = hchildren[nchildren];
    }

  return;
}

/* Remove a zombie from zombies by swapping it with the last child in the list.
 */
static void
remove_zombie (int ci)
{
  sip_printf ("removing %d, pid %d, nzombies %d", ci, zombies[ci]->pid,
	      nzombies);
  if (ci < --nzombies)
    zombies[ci] = zombies[nzombies];

  return;
}

/* Check status of child process vs. waitq member.
 *
 * parent_w is the pointer to the parent of the waitq member in question.
 * child is the subprocess being considered.
 *
 * Returns
 *   1 if stopped or terminated child matches parent_w->next criteria
 *  -1 if a non-stopped/terminated child matches parent_w->next criteria
 *   0 if child does not match parent_w->next criteria
 */
static int
stopped_or_terminated (waitq *parent_w, pinfo *child)
{
  int potential_match;
  waitq *w = parent_w->next;

  sip_printf ("considering pid %d", child->pid);
  if (w->pid == -1)
    potential_match = 1;
  else if (w->pid == 0)
    potential_match = child->pgid == myself->pgid;
  else if (w->pid < 0)
    potential_match = child->pgid == -w->pid;
  else
    potential_match = (w->pid == child->pid);

  if (!potential_match)
    return 0;

  BOOL terminated;

  if ((terminated = child->process_state == PID_ZOMBIE) ||
      (w->options & WUNTRACED) && child->stopsig)
    {
      parent_w->next = w->next;	/* successful wait.  remove from wait queue */
      w->pid = child->pid;

      if (!terminated)
	{
	  sip_printf ("stopped child");
	  w->status = (child->stopsig << 8) | 0x7f;
	  child->stopsig = 0;
	}
      else
	{
	  DWORD status;
	  if (!GetExitCodeProcess (child->hProcess, &status))
	    status = 0xffff;
	  if (status & EXIT_SIGNAL)
	    w->status = (status >> 8) & 0xff;	/* exited due to signal */
	  else
	    w->status = (status & 0xff) << 8;	/* exited via "exit ()" */

	  add_rusage (&myself->rusage_children, &child->rusage_children);
	  add_rusage (&myself->rusage_children, &child->rusage_self);

	  if (w->rusage)
	    {
	      add_rusage ((struct rusage *) w->rusage, &child->rusage_children);
	      add_rusage ((struct rusage *) w->rusage, &child->rusage_self);
	    }
	  zap_subproc (child);
	}

      if (!SetEvent (w->ev))	/* wake up wait4 () immediately */
	system_printf ("couldn't wake up wait event %p, %E", w->ev);
      return 1;
    }

  return -potential_match;
}

/* Process signals by waiting for a semaphore to become signaled.
 * Then scan an in-memory array representing queued signals.
 * Executes in a separate thread.
 *
 * Signals sent from this process are sent a completion signal so
 * that returns from kill/raise do not occur until the signal has
 * has been handled, as per POSIX.
 */
static DWORD
wait_sig (VOID *arg)
{
  /* Initialization */

  /* sigcatch_nosync       - semaphore incremented by sig_dispatch_pending and
   *			     by foreign processes to force an examination of
   *			     the sigtodo array.
   * sigcatch_main	   - ditto for local main thread.
   * sigcatch_nonmain      - ditto for local non-main threads.
   *
   * sigcomplete_main	   - event used to signal main thread on signal
   *			     completion
   * sigcomplete_nonmain   - semaphore signaled for non-main thread on signal
   *			     completion
   */
  sigcatch_nosync = getsem (NULL, "sigcatch", 0, MAXLONG);
  sigcatch_nonmain = CreateSemaphore (&sec_none_nih, 0, MAXLONG, NULL);
  sigcatch_main = CreateSemaphore (&sec_none_nih, 0, MAXLONG, NULL);
  sigcomplete_nonmain = CreateSemaphore (&sec_none_nih, 0, MAXLONG, NULL);
  sigcomplete_main = CreateEvent (&sec_none_nih, TRUE, FALSE, NULL);
sigproc_printf ("sigcatch_nonmain %p", sigcatch_nonmain);

  ProtectHandle (sigcatch_nosync);
  ProtectHandle (sigcatch_nonmain);
  ProtectHandle (sigcatch_main);
  ProtectHandle (sigcomplete_nonmain);
  ProtectHandle (sigcomplete_main);

  /* Setting dwProcessId flags that this process is now capable of receiving
   * signals.  Prior to this, dwProcessId was set to the windows pid of
   * of the original windows process which spawned us unless this was a
   * "toplevel" process.
   */
  myself->dwProcessId = GetCurrentProcessId ();
  myself->process_state |= PID_ACTIVE;
  myself->process_state &= ~PID_INITIALIZING;

  /* If we've been execed, then there is still a stub left in the previous
   * windows process waiting to see if it's started a cygwin process or not.
   * Signalling subproc_ready indicates that we are a cygwin process.
   */
  if (child_proc_info && child_proc_info->type == PROC_EXEC)
    {
      debug_printf ("subproc_ready %p", child_proc_info->subproc_ready);
      if (!SetEvent (child_proc_info->subproc_ready))
	system_printf ("SetEvent (subproc_ready) failed, %E");
      CloseHandle (child_proc_info->subproc_ready);
    }

  SetEvent (wait_sig_inited);
  sigtid = GetCurrentThreadId ();

  (void) SetThreadPriority (hwait_sig, WAIT_SIG_PRIORITY);
  HANDLE catchem[] = {sigcatch_main, sigcatch_nonmain, sigcatch_nosync};
  for (;;)
    {
      DWORD rc = WaitForMultipleObjects (3, catchem, FALSE, sig_loop_wait);

      /* sigproc_terminate sets sig_loop_wait to zero to indicate that
       * this thread should terminate.
       */
      if (rc == WAIT_TIMEOUT || !sig_loop_wait)
	  break;			// Exiting
      if (rc == WAIT_FAILED)
	{
	  if (sig_loop_wait != 0)
	    system_printf ("WFMO failed, %E");
	  break;
	}

      rc -= WAIT_OBJECT_0;
      int dispatched = FALSE;
      sip_printf ("awake");
      /* A sigcatch semaphore has been signaled.  Scan the sigtodo
       * array looking for any unprocessed signals.
       */
      int new_pending_signals = 0;
      for (int sig = -__SIGOFFSET; sig < NSIG; sig++)
	{
#ifdef NOSIGQUEUE
	  if (InterlockedExchange (myself->getsigtodo(sig), 0L) > 0)
#else
	  while (InterlockedDecrement (myself->getsigtodo(sig)) >= 0)
#endif
	    {
	      if (sig > 0 && sig != SIGCONT && sig != SIGKILL && sig != SIGSTOP &&
		  (sigismember (& myself->getsigmask (), sig) ||
		   myself->process_state & PID_STOPPED))
		{
		  sip_printf ("sig %d blocked", sig);
		  break;
		}

	      /* Found a signal to process */
	      sip_printf ("processing signal %d", sig);
	      switch (sig)
		{
		case __SIGFLUSH:
		  /* just forcing the loop */
		  break;

		/* Internal signal to force a flush of strace data to disk. */
		case __SIGSTRACE:
		  proc_strace ();	// Dump cached strace_printf stuff.
		  break;

		/* Signalled from a child process that it has stopped */
		case __SIGCHILDSTOPPED:
		  sip_printf ("Received child stopped notification");
		  dispatched |= sig_handle (SIGCHLD);
		  if (proc_subproc (PROC_CHILDSTOPPED, 0))
		    dispatched = 1;
		  break;

		/* Called by sigsuspend for "nonraceable" detection of
		 * dispatched signals.
		 * Waits for sig_send to give up the sig_dispatch mutex
		 * before continuing.
		 */
		case __SIGSUSPEND:
		  break;

		/* A normal UNIX signal */
		default:
		  sip_printf ("Got signal %d", sig);
		  dispatched |= sig_handle (sig);
		  goto nextsig;
		}
	    }
#ifndef NOSIGQUEUE
	  /* Decremented too far. */
	  if (InterlockedIncrement (myself->getsigtodo(sig)) > 0)
	    new_pending_signals = -1;
#endif
	nextsig:
	  continue;
	}

      /* Signal completion of signal handling depending on which semaphore
       * woke up the WaitForMultipleObjects above.
       */
      switch (rc)
	{
	case 0:
	  SetEvent (sigcomplete_main);
	  break;
	case 1:
	  ReleaseSemaphore (sigcomplete_nonmain, 1, NULL);
	  break;
	default:
	  /* Signal from another process.  No need to synchronize. */
	  break;
	}

      /* Signal that a signal was (or could have been) dispatched */
      if (dispatched)
	{
	  sip_printf ("signaling signal_arrived");
	  new_pending_signals = dispatched < 0;
#if 0
	  SetEvent (signal_arrived);
#endif
	}
      pending_signals = new_pending_signals;
      sip_printf ("looping");
    }

  sip_printf ("done");
  return 0;
}

/* Wait for subprocesses to terminate. Executes in a separate thread. */
static DWORD
wait_subproc (VOID *arg)
{
  sip_printf ("starting");

  for (;;)
    {
      DWORD rc = WaitForMultipleObjects (nchildren + 1, events, FALSE,
					 proc_loop_wait);
      if (rc == WAIT_TIMEOUT)
	  break;			// Exiting

      if (rc == WAIT_FAILED)
	{
	  if (!proc_loop_wait)
	    break;

	  system_printf ("wait failed. nchildren %d, wait %d, %E",
			nchildren, proc_loop_wait);
	  for (int i = 0; i < nchildren + 1; i++)
	    if ((rc = WaitForSingleObject (events[i], 0)) == WAIT_OBJECT_0 ||
		rc == WAIT_TIMEOUT)
	      continue;
	    else
	      system_printf ("event[%d] %p, %E", i, events[0]);
	  break;
	}

      rc -= WAIT_OBJECT_0;
      if (rc-- != 0)
	(void)proc_subproc (PROC_CHILDTERMINATED, rc);
      sip_printf ("looping");
    }

  ForceCloseHandle (events[0]);
  events[0] = NULL;
  sip_printf ("done");
  return 0;
}

/* Clear out a subprocess from pinfo. */
static void
zap_subproc (pinfo *child)
{
  (void) ForceCloseHandle (child->hProcess);
  sip_printf ("closed handle %p", child->hProcess);
  child->hProcess = NULL;
  child->process_state = PID_NOT_IN_USE;	/* a reaped child */
}
