/* malloc.cc for WIN32.

   Copyright 1996, 1997, 1998 Cygnus Solutions.

   Written by Steve Chamberlain of Cygnus Support
   sac@cygnus.com

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdlib.h>
#include "winsup.h"

/* we provide these stubs to call into a user's
   provided malloc if there is one - otherwise
   functions we provide - like strdup will cause
   problems if malloced on our heap and free'd on theirs

   Disgusting but true.
*/

static int export_malloc_called = 0;
static int use_internal_malloc = 1;

#undef in
#undef out
#define in(x)
#define out(x)

/* Call though the application pointer,
   which either points to export_malloc, or the application's
   own version. */

void *
malloc (size_t size)
{
  void *res;
  in ("malloc");
  res = user_data->malloc (size);
  out ("malloc");
  return res;
}

void
free (void *p)
{
  in ("free");
  user_data->free (p);
  out ("free");
}

void *
realloc (void *p, size_t size)
{
  void *res;
  in ("realloc");
  res = user_data->realloc (p, size);
  out ("realloc");
  return res;
}

void *
calloc (size_t nmemb, size_t size)
{
  void *res;
  in ("calloc");
  res = user_data->calloc (nmemb, size);
  out ("calloc");
  return res;
}

/* These routines are used by the application if it
   doesn't provide its own malloc. */

extern "C"
void
export_free (void *p)
{
  in ("export_free");
  malloc_printf ("(%p), called by %x", p, ((int *)&p)[-1]);
  if (use_internal_malloc)
    _free_r (_impure_ptr, p);
  else
    user_data->free (p);
  out ("export_free");
}

extern "C"
void *
export_malloc (int size)
{
  void *res;
  in ("export_malloc");
  export_malloc_called = 1;
  if (use_internal_malloc)
    res = _malloc_r (_impure_ptr, size);
  else
    res = user_data->malloc (size);
  malloc_printf ("(%d) = %x, called by %x", size, res, ((int *)&size)[-1]);
  out ("export_malloc");
  return res;
}

extern "C"
void *
export_realloc (void *p, int size)
{
  void *res;
  in ("export_realloc");
  if (use_internal_malloc)
    res = _realloc_r (_impure_ptr, p, size);
  else
    res = user_data->realloc (p, size);
  malloc_printf ("(%x, %d) = %x, called by %x", p, size, res, ((int *)&p)[-1]);
  out ("export_realloc");
  return res;
}

extern "C"
void *
export_calloc (size_t nmemb, size_t size)
{
  void *res;
  in ("export_calloc");
  if (use_internal_malloc)
    res = _calloc_r (_impure_ptr, nmemb, size);
  else
    res = user_data->calloc (nmemb, size);
  malloc_printf ("(%d, %d) = %x, called by %x", nmemb, size, res, ((int *)&nmemb)[-1]);
  out ("export_calloc");
  return res;
}

/* We use a critical section to lock access to the malloc data
   structures.  This permits malloc to be called from different
   threads.  Note that it does not make malloc reentrant, and it does
   not permit a signal handler to call malloc.  The malloc code in
   newlib will call __malloc_lock and __malloc_unlock at appropriate
   times.  */

static NO_COPY CRITICAL_SECTION malloc_critical_section;

void
malloc_init ()
{
  InitializeCriticalSection (&malloc_critical_section);
  /* Check if mallock is provided by application. If so, redirect all
     calls to export_malloc/free/realloc to application provided. This may
     happen if some other dll calls cygwin's malloc, but main code provides
     its own malloc */
  if (!user_data->forkee)
    {
      free (malloc (16));
      if (!export_malloc_called)
	use_internal_malloc = 0;
    }
}

extern "C"
void
__malloc_lock (struct _reent *ptr)
{
  //EnterCriticalSection (&malloc_critical_section);
  SetResourceLock(LOCK_MEMORY_LIST,WRITE_LOCK|READ_LOCK," __malloc_lock");
}

extern "C"
void
__malloc_unlock (struct _reent *ptr)
{
  ReleaseResourceLock(LOCK_MEMORY_LIST,WRITE_LOCK|READ_LOCK," __malloc_unlock");
  //LeaveCriticalSection (&malloc_critical_section);
}
