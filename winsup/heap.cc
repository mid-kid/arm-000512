/* heap.cc: Cygwin heap manager.

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <errno.h>
#include "winsup.h"

/* Initialize the heap at process start up.  */

void
heap_init ()
{
  /* If we're the forkee, we must allocate the heap at exactly the same place
     as our parent.  If not, we don't care where it ends up.  */

  if (user_data->base)
    {
      char *p = (char *) VirtualAlloc (user_data->base, user_data->size,
				       MEM_RESERVE, PAGE_READWRITE);
      if (p == NULL)
	api_fatal ("unable to allocate heap, %E");
      if (p != user_data->base)
	api_fatal ("heap allocated but not at %p", user_data->base);
      if (! VirtualAlloc (user_data->base,
			  (char *) user_data->ptr - (char *) user_data->base,
			  MEM_COMMIT, PAGE_READWRITE))
	api_fatal ("MEM_COMMIT failed, %E");
    }
  else
    {
      user_data->size = cygwin_shared->heap_chunk_size ();
      user_data->base = (char *) VirtualAlloc (0, user_data->size,
				       MEM_RESERVE, PAGE_READWRITE);
      if (user_data->base == NULL)
	api_fatal ("unable to allocate heap, heap_chunk_size %d, %E",
		   user_data->size);
      user_data->ptr = user_data->base;
    }

  malloc_init ();
}

/*
 * FIXME: size_t is the wrong type for incr, because size_t is unsigned
 * but the input to sbrk can be a negative number!
 */

static char *
commit_and_inc (size_t incr)
{
  char *res = (char *) user_data->ptr;

  if (!VirtualAlloc (user_data->ptr, incr, MEM_COMMIT, PAGE_READWRITE))
    {
      system_printf ("VirtualAlloc failed");
      set_errno (ENOMEM);
      res = (char *) -1;
    }
  else
    user_data->ptr = (char *) user_data->ptr + incr;

  return res;
}

/* FIXME: a negative incr through split heaps won't work.
   Need to rewrite.  How about only have sbrk use one heap, have malloc not
   use sbrk and instead have malloc use multiple heaps (and if that causes
   problems then abandon sbrk).  The fork code to copy r/w data from parent to
   child will have to be more sophisticated but that should be doable (and the
   result will be more robust).  */

extern "C"
void *
_sbrk (size_t incr_arg)
{
  char *res = (char *) (user_data->ptr);
  /* local signed version of incr (see FIXME above) */
  int incr = incr_arg;

  if (user_data->base == 0)
    {
      user_data->ptr = user_data->base;
      user_data->size = 0;
    }

  syscall_printf ("_sbrk (%d)", incr);
  syscall_printf ("base %p, ptr %p, end %p, size %d, avail %d",
		  user_data->base, user_data->ptr,
		  (char *) user_data->ptr + user_data->size,
		  user_data->size, user_data->size -
		      ((char *) user_data->ptr - (char *) user_data->base));

  /* zero == special case, just return current top */
  if (incr != 0)
    {
      if (incr < 0)
	{
	  /* less than zero: just decrease allocation */
	  user_data->ptr = (res + incr < user_data->base ? user_data->base : res + incr);
	  res = (char *) user_data->ptr;
	}
      else
	{
	  /* Want more space, see if it can be done in the current pool */
	  if ((char *) user_data->ptr + incr <=
			       (char *) user_data->base + user_data->size)
	    {
	      /* have reserves: just commit what has been requested */
	      res = commit_and_inc (incr);
	    }
	  else
	    {
	      /* Try to add more in multiples of heap_chunk_size.  */
	      unsigned int newalloc = cygwin_shared->heap_chunk_size ();

	      while ((char *) user_data->ptr + incr >=
		     (char *) user_data->base + user_data->size + newalloc)
		newalloc += cygwin_shared->heap_chunk_size ();

	      debug_printf ("trying to extend heap by %d bytes", newalloc);
	      if (user_data->base == 0 ||
		  !VirtualAlloc ((char *) user_data->base + user_data->size,
				 newalloc,
				 MEM_RESERVE,
				 PAGE_READWRITE))
		{
		  /* Unable to allocate a contiguous block following
		     the current area.  Try to allocate another area. */
		  char *r;
		  debug_printf ("extending heap failed, starting new one");

		  /* It's possible that newalloc < incr. */
		  while ((int)newalloc < incr)
		    newalloc += cygwin_shared->heap_chunk_size ();

		  r = (char *) VirtualAlloc (0,
					     newalloc,
					     MEM_RESERVE,
					     PAGE_READWRITE);
		  if (!r)
		    {
		      __seterrno ();
		      set_errno (ENOMEM); /* failed in VirtualAlloc */
		      res =  (char *) -1;
		    }
		  else
		    {
		      debug_printf ("new heap at %p", r);
		      user_data->ptr = user_data->base = r;
		      user_data->size = newalloc;
		      myself->process_state |= PID_SPLIT_HEAP;
		      res = commit_and_inc (incr);
		    }
		}
	      else
		{
		  /* Now must commit requested incr in two chunks: what was
		     previously in the pool, and what we just added.  */
		  unsigned long size1, size2;

		  debug_printf ("heap successfully extended");
		  size1 = (char *) user_data->base + user_data->size -
						(char *) user_data->ptr;
		  size2 = incr - size1;
		  if ((size1 && !VirtualAlloc (user_data->ptr,
					      size1,
					      MEM_COMMIT,
					      PAGE_READWRITE))
		      ||  (size2 && !VirtualAlloc ((char *) user_data->base +
							   user_data->size,
						  size2,
						  MEM_COMMIT,
						  PAGE_READWRITE)))
		    {
		      system_printf ("unable to commit extension");
		      set_errno (ENOMEM); /* failed in VirtualAlloc */
		      res = (char *) -1;
		    }
		  else
		    {
		      /* succeeded, record new pool size and alloc end */
		      user_data->size += newalloc;
		      user_data->ptr = (char *) user_data->ptr + incr;
		    }
		}
	    }
	}
    }

  syscall_printf ("%p = sbrk (%p) (total %p)", res, incr,
		     (char *) user_data->ptr - (char *) user_data->base);
  return res;
}
