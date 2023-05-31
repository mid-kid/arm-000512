/* fork.cc

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include "winsup.h"
#include "dll_init.h"

DWORD chunksize = 0;
/* Timeout to wait for child to start, parent to init child, etc.  */
/* FIXME: Once things stabilize, bump up to a few minutes.  */
#define FORK_WAIT_TIMEOUT (300 * 1000)     /* 300 seconds */

#define dll_data_start &_data_start__
#define dll_data_end &_data_end__
#define dll_bss_start &_bss_start__
#define dll_bss_end &_bss_end__

#if 0
void
print_checksum (int idx, register void *low, register void *high)
{
  int pi;
  register  int sum = 0;
  small_printf ("CK %d %x %x ", idx, low, high);

  for (int *pi = (int *)low; pi < (int *)high; pi++)
    {
      sum += *pi;
    }
  small_printf ("%x\n", sum);
}
#endif

static void
stack_base (child_info_fork &ch)
{
  MEMORY_BASIC_INFORMATION m;
  memset (&m, 0, sizeof m);
  if (!VirtualQuery ((LPCVOID) &m, &m, sizeof m))
    system_printf ("couldn't get memory info, %E");

  DWORD fs4, fs8;
  asm ("movl %%fs:4,%0" : "=r" (fs4));
  asm ("movl %%fs:8,%0" : "=r" (fs8));
  ch.stacktop = m.AllocationBase;
  ch.stackbottom = (LPBYTE) m.BaseAddress + m.RegionSize;
  ch.stacksize = (DWORD) ch.stackbottom - (DWORD) &m;
  debug_printf ("bottom %p, top %p, stack %p, size %d, reserve %d",
		ch.stackbottom, ch.stacktop, &m, ch.stacksize,
		(DWORD) ch.stackbottom - (DWORD) ch.stacktop);
}

/* Copy memory from parent to child.
   The result is a boolean indicating success.  */

static int
fork_copy (PROCESS_INFORMATION &pi, const char *what, ...)
{
  va_list args;
  char *low;
  int pass = 0;

  va_start (args, what);

  while ((low = va_arg (args, char *)))
    {
      char *high = va_arg (args, char *);
      DWORD todo = chunksize ?: high - low;
      char *here;

      for (here = low; here < high; here += todo)
	{
	  DWORD done = 0;
	  if (here + todo > high)
	    todo = high - here;
	  int res = WriteProcessMemory (pi.hProcess, here, here, todo, &done);
	  debug_printf ("child handle %p, low %p, high %p, res %d", pi.hProcess,
			low, high, res);
	  if (!res || todo != done)
	    {
	      if (!res)
		__seterrno ();
	      /* If this happens then there is a bug in our fork
		 implementation somewhere. */
	      system_printf ("%s pass %d failed, %p..%p, done %d, %E",
			    what, pass, low, high, done);
	      goto err;
	    }
	}

      pass++;
    }

#if 0
  print_checksum (idx, low, high);
#endif
  debug_printf ("done");
  return 1;

err:
  TerminateProcess (pi.hProcess, 1);
  set_errno (EAGAIN);
  return 0;
}

/* Wait for child to finish what it's doing and signal us.
   We don't want to wait forever here.If there's a problem somewhere
   it'll hang the entire system (since all forks are mutex'd). If we
   time out, set errno = EAGAIN and hope the app tries again.  */
static int
sync_with_child (PROCESS_INFORMATION &pi, HANDLE subproc_ready,
		 BOOL hang_child, const char *s)
{
  /* We also add the child process handle to the wait. If the child fails
     to initialize (eg. because of a missing dll). Then this
     handle will become signalled. This stops a *looong* timeout wait.
  */
  HANDLE w4[2];

  debug_printf ("waiting for child.  reason: %s", s);
  w4[1] = pi.hProcess;
  w4[0] = subproc_ready;
  DWORD rc = WaitForMultipleObjects (2, w4, FALSE, FORK_WAIT_TIMEOUT);

  if (rc == WAIT_OBJECT_0 ||
      WaitForSingleObject (subproc_ready, 0) == WAIT_OBJECT_0)
    /* That's ok */;
  else if (rc == WAIT_FAILED || rc == WAIT_TIMEOUT)
    {
      if (rc != WAIT_FAILED)
	system_printf ("WaitForMultipleObjects timed out");
      else
	system_printf ("WaitForMultipleObjects failed, %E");
      set_errno (EAGAIN);
      syscall_printf ("-1 = fork(), WaitForMultipleObjects failed");
      TerminateProcess (pi.hProcess, 1);
      return 0;
    }
  else
    {
      /* Child died. Clean up and exit. */
      DWORD errcode;
      GetExitCodeProcess (pi.hProcess, &errcode);
      /* Fix me.  This is not enough.  The fork should not be considered
       * to have failed if the process was essentially killed by a signal.
       */
      if (errcode != STATUS_CONTROL_C_EXIT)
	{
	    system_printf ("child %d(%p) died before initialization with status code %p",
			  pi.dwProcessId, pi.hProcess, errcode);
	    system_printf ("*** child state %s", s);
	}
      set_errno (EAGAIN);
      syscall_printf ("Child died before subproc_ready signalled");
      return 0;
    }

  debug_printf ("child signalled me");
  if (hang_child)
    {
      int n = SuspendThread (pi.hThread);
      debug_printf ("suspend count %d", n); \
    }
  return 1;
}

static int
resume_child (PROCESS_INFORMATION &pi, HANDLE subproc_ready,
	      HANDLE forker_finished)
{
  int rc;

  debug_printf ("here");
  SetEvent (forker_finished);

  rc = ResumeThread (pi.hThread);

  debug_printf ("rc %d", rc);
  if (rc == 1)
    return 1;		// Successful resumption

  /* Can't resume the thread.  Not sure why this would happen unless
     there's a bug in the system.  Things seem to be working OK now
     though, so flag this with EAGAIN, but print a message on the
     console.  */
  small_printf ("fork: ResumeThread failed, rc = %d, %E", rc);
  set_errno (EAGAIN);
  syscall_printf ("-1 = fork(), ResumeThread failed");
  TerminateProcess (pi.hProcess, 1);
  return 0;
}

/* Notify parent that it is time for the next step.
   Note that this has to be a macro since the parent may be messing with
   our stack. */
#define sync_with_parent(s, hang_self) \
({ \
  debug_printf ("signalling parent: %s", s); \
  /* Tell our parent we're waiting. */ \
  if (!SetEvent (child_proc_info->subproc_ready)) \
    api_fatal ("fork child - SetEvent failed, %E"); \
  if (hang_self) \
    { \
      /* Wait for the parent to fill in our stack and heap. \
	 Don't wait forever here.  If our parent dies we don't want to clog \
	 the system.  If the wait fails, we really can't continue so exit.  */ \
      DWORD psync_rc = WaitForSingleObject (child_proc_info->forker_finished, FORK_WAIT_TIMEOUT); \
      switch (psync_rc) \
	{ \
	case WAIT_TIMEOUT: \
	  api_fatal ("sync_with_parent - WFSO timed out"); \
	  break; \
	case WAIT_FAILED: \
	  api_fatal ("sync_with_parent - WFSO failed, %E"); \
	  break; \
	default: \
	  break; \
	} \
      debug_printf ("awake"); \
    } \
})

static volatile void grow_stack_slack();

static void *
stack_dummy (int here)
{
  return &here;
}

extern "C" int
fork ()
{
  int res;
  DWORD rc;
  sigset_t oldmask, forkmask;
  void *stack_here;
  int x;
  char **load_dll = NULL;
  PROCESS_INFORMATION pi = {0};

  /* FIXME: something is broken when copying the stack from the parent
     to the child; we try various tricks here to make sure that the
     stack is good enough to prevent page faults, but the true cause
     is still unknown.  DJ */
  volatile char dummy[4096];
  dummy[0] = dummy[4095] = 0;	// Just to leave some slack in the stack

  grow_stack_slack ();

  debug_printf ("entering");
  /* Calculate how much of stack to copy to child */
  stack_here = stack_dummy (0);

  sigfillset (&forkmask);

  if (ISSTATE(myself, PID_SPLIT_HEAP))
    {
      system_printf ("The heap has been split, CYGWIN can't fork this process.");
      system_printf ("Increase the heap_chunk_size in the registry and try again.");
      set_errno (ENOMEM);
      syscall_printf ("-1 = fork (), split heap");
      return -1;
    }

  /* Don't start the fork until we have the lock.  */
  rc = lock_pinfo_for_update (FORK_WAIT_TIMEOUT);
  pinfo *child = cygwin_shared->p.allocate_pid ();
  unlock_pinfo ();

  if (!child)
    {
      set_errno (EAGAIN);
      syscall_printf ("-1 = fork (), process table full");
      unlock_pinfo ();
      return -1;
    }

  /* Block all signals while in a fork */
  sigprocmask (SIG_SETMASK, &forkmask, &oldmask);

  /* This will help some of the confusion.  */
  fflush (stdout);

  debug_printf ("parent pid %d, child pid %d", myself->pid, child->pid);

  HANDLE subproc_ready = CreateEvent (&sec_all, FALSE, FALSE, NULL);
  HANDLE forker_finished = CreateEvent (&sec_all, FALSE, FALSE, NULL);
  ProtectHandle (subproc_ready);
  ProtectHandle (forker_finished);

  /* If we didn't obtain all the resources we need to fork, allow the program
     to continue, but record the fact that fork won't work.  */
  if (forker_finished == NULL || subproc_ready == NULL)
    {
      system_printf ("unable to allocate fork() resources.");
      system_printf ("fork() disabled.");
      return -1;
    }

  subproc_init ();

  debug_printf ("about to call setjmp");
  static child_info_fork ch;
  x = setjmp (ch.jmp);

  if (x == 0)
    {
      /* Parent.  */
#ifdef DEBUGGING
      /* The ProtectHandle call allocates memory so we need to make sure
	 that enough is set aside here so that the sbrk pointer does not
	 move when ProtectHandle is called after the child is started.
	 Otherwise the sbrk pointers in the parent will not agree with
	 the child and when user_data is (regrettably) copied over,
	 the user_data->ptr field will not be accurate. */
      free (malloc (4096));
#endif
      ch.cb = sizeof ch;
      ch.type = PROC_FORK1;
      ch.cygpid = child->pid;
      ch.subproc_ready = subproc_ready;
      ch.shared_h = cygwin_shared_h;
      ch.forker_finished = forker_finished;
      ch.heapsize = user_data->size;
      ch.heapbase = user_data->base;
      ch.heapptr = user_data->ptr;

      stack_base (ch);

      /* Initialize things that are done later in dll_crt0_1 that aren't done
	 for the forkee.  */
      strcpy(child->progname, myself->progname);

      STARTUPINFO si = {0};

      si.cb = sizeof (STARTUPINFO);
      si.lpReserved2 = (LPBYTE)&ch;
      si.cbReserved2 = sizeof(ch);

      child->strace_mask = myself->strace_mask;
      child->strace_file = myself->strace_file;

      int c_flags = GetPriorityClass (GetCurrentProcess ()) /*|
		    CREATE_NEW_PROCESS_GROUP*/;

      /* If we don't have a console, then don't create a console for the
	 child either.  */
      HANDLE console_handle = CreateFileA ("CONOUT$", GENERIC_WRITE,
					   FILE_SHARE_WRITE, &sec_none_nih,
					   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
					   NULL);

      syscall_printf ("CreateProcessA (%s, %s,0,0,1,%x, 0,0,%p,%p)",
		      myself->progname, myself->progname, c_flags, &si, &pi);
      if (console_handle != INVALID_HANDLE_VALUE && console_handle != 0)
	CloseHandle (console_handle);
      else
	c_flags |= DETACHED_PROCESS;

      rc = CreateProcessA (myself->progname, /* image to run */
			   myself->progname, /* what we send in arg0 */
			   &sec_none_nih,  /* process security attrs */
			   &sec_none_nih,  /* thread security attrs */
			   TRUE,	  /* inherit handles from parent */
			   c_flags,
			   NULL,	  /* environment filled in later */
			   0,		  /* use current drive/directory */
			   &si,
			   &pi);

      if (!rc)
	{
	  __seterrno ();
	  syscall_printf ("-1 = fork(), CreateProcessA failed");
	  child->process_state = PID_NOT_IN_USE;
	  unlock_pinfo ();
	  sigprocmask (SIG_SETMASK, &oldmask, NULL);
	  ForceCloseHandle(subproc_ready);
	  ForceCloseHandle(forker_finished);
	  subproc_ready = forker_finished = NULL;
	  return -1;
	}

      ProtectHandle (pi.hThread);
      ProtectHandle (pi.hProcess);

      /* Fill in fields in the child's process table entry.  */
      child->ppid = myself->pid;
      child->hProcess = pi.hProcess;
      child->dwProcessId = pi.dwProcessId;
      child->uid = myself->uid;
      child->gid = myself->gid;
      child->pgid = myself->pgid;
      child->sid = myself->sid;
      child->ctty = myself->ctty;
      child->umask = myself->umask;
      child->copysigs(myself);
      if (number_of_sockets)
	child->process_state |= PID_SOCKETS_USED;
      child->process_state |= PID_INITIALIZING |
			      (myself->process_state & PID_USETTY);
      set_child_mmap_ptr (child);

      /* Check if forkee must reload dlopened dlls.
	 We have to:
	 1) create a list with dlls names
	 2) let the child reload these dlls
	 3) write memory of these dlls in child	*/

      int n = 0;
      if (DllList::the().forkeeMustReloadDlls() &&
	 (n = DllList::the().numberOfOpenedDlls()) != 0)
	{
	  load_dll = (char **)malloc (sizeof (char *) * (n + 1));
	  int i = 0;
	  DO_LOADED_DLL_NAMES(name)
	  {
	    if (name)
	      load_dll[i++] = strdup (name);
	  }
	  DONE;
	  load_dll[i] = NULL;
	}

      /* Wait for subproc to initialize itself. */
      if (!sync_with_child(pi, subproc_ready, TRUE, "waiting for longjmp"))
	goto cleanup;

      /* CHILD IS STOPPED */
      debug_printf ("child is alive (but stopped)");

      /* Initialize, in order: data, bss, heap, stack, dll data, dll bss
	 Note: variables marked as NO_COPY will not be copied
	 since they are placed in a protected segment. */

      rc = fork_copy (pi, "user/cygwin data",
		      user_data->data_start, user_data->data_end,
		      user_data->bss_start, user_data->bss_end,
		      ch.heapbase, ch.heapptr,
		      stack_here, ch.stackbottom,
		      dll_data_start, dll_data_end,
		      dll_bss_start, dll_bss_end, NULL);
      if (!rc)
	goto cleanup;

      debug_printf ("number of dlls = %d, load_dll %p", n, load_dll);
      /* Now fill data/bss of linked dll */
      DO_LINKED_DLL (p)
      {
	debug_printf ("copying data/bss of a linked dll");
	if (!fork_copy (pi, "linked dll data/bss", p->data_start, p->data_end,
						   p->bss_start, p->bss_end,
						   NULL))
	  goto cleanup;
      }
      DONE;

      unlock_pinfo ();
      proc_register (child);

      /* Start thread, and wait for it to reload dlls.  */
      if (!resume_child (pi, subproc_ready, forker_finished) ||
	  !sync_with_child (pi, subproc_ready, !!load_dll, "child loading dlls"))
	goto cleanup;

      /* child reload dlls & then write their data and bss */
      if (load_dll)
      {
	/* CHILD IS STOPPED */
	/* write memory of reloaded dlls */
	DO_LOADED_DLL (p)
	{
	  debug_printf ("copying data/bss for a loaded dll");
	  if (!fork_copy (pi, "loaded dll data/bss", p->data_start, p->data_end,
						     p->bss_start, p->bss_end,
						     NULL))
	    goto cleanup;
	}
	DONE;
	/* Start the child up again. */
	(void) resume_child (pi, subproc_ready, forker_finished);

	/* Release the list of dll names that was built up for the child
	   to reference.  The child now has its own copy. */
	for (int i = 0; load_dll[i] != NULL; i++)
	   free (load_dll[i]);
	free (load_dll);
      }

      ForceCloseHandle (subproc_ready);
      ForceCloseHandle (pi.hThread);
      ForceCloseHandle (forker_finished);
      forker_finished = NULL;
      pi.hThread = NULL;

      res = child->pid;
    }
  else
    {
      /* We arrive here via a longjmp from "crt0".  */
      (void) stack_dummy (0);		// Just to make sure
      debug_printf ("child is running %d", x);

      myself->start_time = time (NULL);

      debug_printf ("self %p, pid %d, ppid %d",
		    myself, x, myself ? myself->ppid : -1);

      sync_with_parent ("after longjmp.", TRUE);

      /* If we've played with the stack, stacksize != 0.  That means that
	 fork() was invoked from other than the main thread.  Make sure that
	 when the "main" thread exits it calls do_exit, like a normal process.
	 Exit with a status code of 0. */
      if (child_proc_info->stacksize)
	{
	  ((DWORD *)child_proc_info->stackbottom)[-17] = (DWORD)do_exit;
	  ((DWORD *)child_proc_info->stackbottom)[-15] = (DWORD)0;
	}

      pinfo *p = procinfo (myself->ppid);

      if (p && p->parent_alive)
	(void) CloseHandle (p->parent_alive);	// avoid a handle leak

      dtable.fixup_after_fork ();

      DllList::the().forkeeStartLoadedDlls();
      debug_printf ("load_dll %p", load_dll);
      /* reload dlls if necessary */
      if (!load_dll)
	sync_with_parent ("performed fork fixup.", FALSE);
      else
	{
	  char* ptr;

	  for (int i = 0; (ptr = load_dll[i]) != NULL; i++)
	    {
	      HANDLE loadres;
	      debug_printf ("reloading library %s", ptr );
	      if (!(loadres = LoadLibrary (ptr)))
		system_printf ("can't reload library %s", ptr);
	      else
		debug_printf ("library loaded %x", loadres);
	      free (ptr);
	    }
	  free (load_dll);
	  /* Let parent know that we've loaded all of the dlls.
	     Wait for it to fill in their data segments. */
	  sync_with_parent ("loaded dlls", TRUE);
	}

      DllList::the().forkeeEndLoadedDlls ();

      (void) ForceCloseHandle (child_proc_info->subproc_ready);
      (void) ForceCloseHandle (child_proc_info->forker_finished);

      if (recreate_mmaps_after_fork (myself->mmap_ptr))
	api_fatal ("recreate_mmaps_after_fork_failed");

      /* Initialize winsock if neccessary */
      if (ISSTATE(myself, PID_SOCKETS_USED))
	winsock_init ();

#if 0
      print_checksum (4, cygwin_shared->base[0][0], cygwin_shared->base[0][1]);
      print_checksum (5, cygwin_shared->base[1][0], cygwin_shared->base[1][1]);
      print_checksum (6, cygwin_shared->base[2][0], cygwin_shared->base[2][1]);
      print_checksum (7, cygwin_shared->base[3][0], cygwin_shared->base[3][1]);
#endif

      res = 0;
      /* Set thread local stuff to zero.  Under Windows 95/98 this is sometimes
	 non-zero, for some reason.
	 FIXME:  There is a memory leak here after a fork. */
      for (per_thread **t = threadstuff; *t; t++)
	if ((*t)->clear_on_fork ())
	  (*t)->set ();

      /* With the current malloc implementation, we can not handle
	 calls to malloc until after the parent has copied the malloc
	 state over to us.  That means that although we are about to
	 enable signal handling, we will fail if the signal handler
	 calls malloc.  POSIX.1 does not permit a signal handler to
	 call malloc, so we may be able to get away with this.  */

      sigfillset (&myself->getsigmask ());
      /* Initialize signal/process handling */
      sigproc_init ();
    }

  sigprocmask (SIG_SETMASK, &oldmask, NULL);
  syscall_printf ("%d = fork()", res);
  return res;

/* Common cleanup code for failure cases */
cleanup:
  /* Remember to de-allocate the fd table. */
  child->process_state = PID_NOT_IN_USE;
  unlock_pinfo ();
  if (pi.hProcess)
    ForceCloseHandle (pi.hProcess);
  if (pi.hThread)
    ForceCloseHandle (pi.hThread);
  if (subproc_ready)
    ForceCloseHandle (subproc_ready);
  if (forker_finished)
    ForceCloseHandle (forker_finished);
  if (load_dll)
    {
      rc = 0;
      char* ptr = load_dll[rc++];
      while (ptr)
	{
	  free (ptr);
	  ptr = load_dll[rc++];
	}
      free (load_dll);
    }
  forker_finished = subproc_ready = child->hProcess = NULL;
  sigprocmask (SIG_SETMASK, &oldmask, NULL);
  return -1;
}

static volatile void
grow_stack_slack ()
{
  volatile char dummy[16384];
  dummy[0] = dummy[16383] = 0;	// Just to make some slack in the stack
}

extern "C"
int
vfork ()
{
  return fork ();
}
