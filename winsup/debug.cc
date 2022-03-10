/* debug.cc

   Copyright 1998, 1999 Cygnus Solutions.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#define NO_DEBUG_DEFINES
#include "winsup.h"
#include "exceptions.h"

#undef lock
#undef unlock
#define lock() __lock(__FILE__, __LINE__)
#define unlock() __unlock(__FILE__, __LINE__)

/* A generic debug locker.
   NOTICE: Avoid calling *_printf routines within a locked region
   or deadlock may result. */
class locker
{
  CRITICAL_SECTION sync;
public:
  locker () {InitializeCriticalSection (&sync);}
  void __lock (const char *fn, int ln) {
    if (GetCurrentThreadId () == maintid)
      (void)__get_signal_mutex (fn, ln);
    EnterCriticalSection (&sync);
  }
  void __unlock (const char *fn, int ln) {
    LeaveCriticalSection (&sync);
    if (GetCurrentThreadId () == maintid)
      __release_signal_mutex (fn, ln, -1);
  }
};

locker __tn NO_COPY;

typedef struct
  {
    DWORD id;
    const char *name;
  } thread_info;

typedef struct
  {
    LPTHREAD_START_ROUTINE func;
    VOID *arg;
    HANDLE sync;
  } thread_start;

static NO_COPY thread_info threads[32];	// increase as necessary
#define NTHREADS (sizeof(threads) / sizeof(threads[0]))

void
regthread (const char *name, DWORD tid)
{
  __tn.lock ();
  for (DWORD i = 0; i < NTHREADS; i++)
    if (threads[i].name == NULL || strcmp (threads[i].name, name) == 0 ||
	threads[i].id == tid)
      {
	threads[i].name = name;
	threads[i].id = tid;
	break;
      }
  __tn.unlock ();
}

static DWORD
thread_stub (VOID *arg)
{
  exception_list except_entry;
  thread_start info = *((thread_start *) arg);

  /* marco@ddi.nl: Needed for the reent's  of this local dll thread
     I assume that the local threads are using the reent structure of
     the main thread
   */
#ifdef _MT_SAFE
  if ( !TlsSetValue(user_data->threadinterface->reent_index,&user_data->threadinterface->reents) )
	api_fatal(" Sig proc MT init failed\n");
#endif

  SetEvent (info.sync);

  init_exceptions (&except_entry);

  return (*info.func) (info.arg);
}

HANDLE
makethread (LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags,
	    const char *name)
{
  DWORD tid;
  HANDLE h;
  SECURITY_ATTRIBUTES *sa;
  thread_start info;

  info.func = start;
  info.arg = param;
  info.sync = CreateEvent (&sec_none_nih, FALSE, FALSE, NULL);

  if (*name != '+')
    sa = &sec_none_nih;
  else
    {
      name++;
      sa = &sec_none;
    }

  if ((h = CreateThread (sa, 0, thread_stub, (VOID *)&info, flags, &tid)))
    {
      regthread (name, tid);
      WaitForSingleObject (info.sync, INFINITE);
    }
  CloseHandle (info.sync);

  return h;
}

/* Return the symbolic name of the current thread for debugging.
 */
const char *
threadname (DWORD tid)
{
  const char *res = NULL;
  if (!tid)
    tid = GetCurrentThreadId ();

  __tn.lock ();
  for (DWORD i = 0; i < NTHREADS && threads[i].name != NULL; i++)
    if (threads[i].id == tid)
      {
	res = threads[i].name;
	break;
      }
  __tn.unlock ();

  if (!res)
    {
      static char buf[30] NO_COPY = {0};
      __small_sprintf (buf, "unknown (%p)", tid);
      res = buf;
    }

  return res;
}

#ifdef DEBUGGING
#include <stdlib.h>

typedef struct _h
  {
    BOOL allocated;
    HANDLE h;
    const char *name;
    const char *func;
    int ln;
    struct _h *next;
  } handle_list;

static handle_list starth NO_COPY;
static handle_list *endh NO_COPY;

static handle_list freeh[20] NO_COPY;
#define NFREEH (sizeof (freeh) / sizeof (freeh[0]))

locker __debug_lock NO_COPY;

static handle_list *
find_handle (HANDLE h)
{
  handle_list *hl;
  for (hl = &starth; hl->next != NULL; hl = hl->next)
    if (hl->next->h == h)
      goto out;
  endh = hl;
  hl = NULL;

out:
  return hl;
}

static handle_list *
newh ()
{
  handle_list *hl;
  __debug_lock.lock ();
  for (hl = freeh; hl < freeh + NFREEH; hl++)
    if (hl->name == NULL)
      goto out;

  /* All used up??? */
  if ((hl = (handle_list *)malloc (sizeof *hl)) != NULL)
    {
      memset (hl, 0, sizeof (*hl));
      hl->allocated = TRUE;
    }

out:
  __debug_lock.unlock ();
  return hl;
}

void
add_handle (const char *func, int ln, HANDLE h, const char *name)
{
  handle_list *hl;
  __debug_lock.lock ();

  if (find_handle (h))
    goto out;		/* Already did this once */

  if ((hl = newh()) == NULL)
    {
      __debug_lock.unlock ();
      system_printf ("couldn't allocate memory for %s(%d): %s(%p)",
		     func, ln, name, h);
      return;
    }
  hl->h = h;
  hl->name = name;
  hl->func = func;
  hl->ln = ln;
  hl->next = NULL;
  endh->next = hl;
  endh = hl;

out:
  __debug_lock.unlock ();
}

BOOL
close_handle (const char *func, int ln, HANDLE h, const char *name, BOOL force)
{
  BOOL ret;
  handle_list *hl;
  __debug_lock.lock ();

  if ((hl = find_handle (h)) && !force)
    {
      hl = hl->next;
      __debug_lock.unlock ();	// race here
      system_printf ("attempt to close protected handle %s:%d(%s<%p>)",
		     hl->func, hl->ln, hl->name, hl->h);
      system_printf (" by %s:%d(%s<%p>)", func, ln, name, h);
      return FALSE;
    }

  ret = CloseHandle (h);
  if (hl)
    {
      handle_list *hnuke = hl->next;
      hl->next = hl->next->next;
      if (hnuke->allocated)
	free (hnuke);
      else
	hnuke->name = NULL;
    }

  __debug_lock.unlock ();
  return ret;
}

// Use these until gdb is more sophisticated with its stack traces

#undef WaitForSingleObject

DWORD
WFSO (HANDLE hHandle, DWORD dwMilliseconds)
{
  return WaitForSingleObject (hHandle, dwMilliseconds);
}

#undef WaitForMultipleObjects

DWORD
WFMO (DWORD nCount, CONST HANDLE *lpHandles, BOOL fWaitAll, DWORD dwMilliseconds)
{
  return WaitForMultipleObjects (nCount, lpHandles, fWaitAll, dwMilliseconds);
}
#endif /*DEBUGGING*/
