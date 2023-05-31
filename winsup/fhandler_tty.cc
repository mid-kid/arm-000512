/* fhandler_tty.cc

   Copyright 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "winsup.h"
#include <ctype.h>

/* Tty master stuff */

fhandler_tty_master NO_COPY *tty_master;

static DWORD process_input (void *);		// Input queue thread
static DWORD process_output (void *);		// Output queue thread
static DWORD process_ioctl (void *);		// Ioctl requests thread


fhandler_tty_master::fhandler_tty_master (const char *name) :
	fhandler_pty_master (name, FH_TTYM)
{
  set_cb (sizeof *this);
  console = NULL;
  hThread = NULL;
  ttynum = 0;
}

int
fhandler_tty_master::init (int ttynum_in)
{
  termios_printf ("Creating master for tty%d", ttynum_in);
  ttynum = ttynum_in;

  /* Redirection of stderr almost never occured in Win32 world. So stderr is
   * connected to our physical device
   */
  HANDLE err = GetStdHandle (STD_ERROR_HANDLE);

  if (err == INVALID_HANDLE_VALUE)
    {
      termios_printf ("can't get stderr handle");
      return -1;
    }
  console = dtable.build_fhandler (-1, FH_CONSOLE, "/dev/ttym");
  if (console == NULL)
    {
      termios_printf ("can't create fhandler");
      return -1;
    }

  console->init (err, GENERIC_READ | GENERIC_WRITE, O_BINARY);

  cygwin_shared->tty[ttynum_in]->common_init (this);

  hThread = makethread (process_input, NULL, 0, "ttyin");
  if (hThread == NULL)
    {
      termios_printf ("can't create input thread");
      return -1;
    }
  else
    {
      SetThreadPriority (hThread, THREAD_PRIORITY_HIGHEST);
      CloseHandle (hThread);
    }

  hThread = makethread (process_ioctl, NULL, 0, "ttyioctl");
  if (hThread == NULL)
    {
      termios_printf ("can't create ioctl thread");
      return -1;
    }
  else
    {
      SetThreadPriority (hThread, THREAD_PRIORITY_HIGHEST);
      CloseHandle (hThread);
    }

  hThread = makethread (process_output, NULL, 0, "ttyout");
  if (hThread == NULL)
    {
      termios_printf ("can't create output thread");
      return -1;
    }
  else
    {
      SetThreadPriority (hThread, THREAD_PRIORITY_HIGHEST);
    }

  return 0;
}

/* Process tty input. */

void
fhandler_pty_master::doecho (const void *str, unsigned len)
{
  WaitForSingleObject (output_mutex, INFINITE);
  WriteFile (ttyp->to_master, str, len, &len, NULL);
//  WaitForSingleObject (output_done_event, INFINITE);
  ReleaseMutex (output_mutex);
}

int
fhandler_pty_master::process_input_to_slave (const char *rptr, int nraw)
{
  static NO_COPY char buf[INP_BUFFER_SIZE];
  static NO_COPY char *bptr = buf;
  unsigned int n, written;
  char c;
  int eol = 0;
  int towrite = nraw;

  while (nraw > 0)
    {
      c = *rptr++;
      nraw--;

      termios_printf ("char %c", c);

      /* Check for special chars */

#ifndef NOSTRACE
      if (c == '\024' && strace() & _STRACE_CACHE)
	{
	  sig_send(NULL, __SIGSTRACE);
	  continue;
	}
#endif
      if (c == '\r')
	{
	  if (ttyp->termios.c_iflag & IGNCR)
	    continue;
	  if (ttyp->termios.c_iflag & ICRNL)
	    c = '\n';
	  ++eol;
	}
      else if (c == '\n' && (ttyp->termios.c_iflag & INLCR))
	{
	  c = '\r';
	  ++eol;
	}

      if (ttyp->termios.c_iflag & ISTRIP)
	c &= 0x7f;
      if (ttyp->termios.c_lflag & ISIG)
	{
	  if (c == ttyp->termios.c_cc[VINTR] ||	// interrupt chars
	      c == ttyp->termios.c_cc[VQUIT] ||
	      c == ttyp->termios.c_cc[VSUSP])
	    {
	      int sig = c == ttyp->termios.c_cc[VINTR]? SIGINT:
		     (c == ttyp->termios.c_cc[VQUIT]? SIGQUIT: SIGTSTP);
	      termios_printf ("got interrupt %d, sending signal %d", c);
	      kill_pgrp (ttyp->getpgid (), sig);
	      ttyp->termios.c_lflag &= ~FLUSHO;
	      goto restart_output;
	    }
	}
      if (ttyp->termios.c_iflag & IXON)
	{
	  if (c == ttyp->termios.c_cc[VSTOP])
	    {
	      ttyp->OutputStopped++;
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VSTART])
	    {
restart_output:
	      ttyp->OutputStopped = 0;
	      SetEvent (restart_output_event);
	      continue;
	    }
	  else if ((ttyp->termios.c_iflag & IXANY) && ttyp->OutputStopped)
	    goto restart_output;
	}
      if (ttyp->termios.c_lflag & IEXTEN)
	{
	  if (c == ttyp->termios.c_cc[VDISCARD])
	    {
	      ttyp->termios.c_lflag ^= FLUSHO;
	      continue;
	    }
	}
      if (ttyp->termios.c_lflag & ICANON)
	{
	  if (c == ttyp->termios.c_cc[VERASE])
	    {
	      if (bptr > buf)
		{
		  bptr--;
		  doecho ("\b \b", 3);
		}
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VWERASE])
	    {
	      while (bptr > buf)
		if (isspace (bptr[-1]))
		  break;
		else
		  {
		    bptr--;
		    doecho ("\b \b", 3);
		  }
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VKILL])
	    {
	      while (bptr > buf)
		{
		  bptr--;
		  doecho ("\b \b", 3);
		}
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VREPRINT])
	    {
	      doecho ("\n\r", 2);
	      doecho (buf, bptr-buf);
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VEOF])
	    {
	      termios_printf ("EOF");
	      n = ttyp->read_retval = bptr - buf;
	      termios_printf ("about to write %d chars to inbuf", n);
	      WriteFile (get_output_handle (), (unsigned *) buf, n? n: 1, &written, NULL);
	      if (n)
		{
		  Sleep (10);
		  ttyp->read_retval = 0;
		  WriteFile (get_output_handle (), (unsigned *) buf, 1, &written, NULL);
		}
	      continue;
	    }
	  else if (c == ttyp->termios.c_cc[VEOL] ||
		   c == ttyp->termios.c_cc[VEOL2] ||
		   c == '\n')
	    {
	      eol++;
	      termios_printf ("EOL");
	    }
	}
      if (ttyp->termios.c_lflag & ECHO)
	{
	  doecho (&c, 1);
	}
      *bptr++ = c;
      if ((ttyp->termios.c_lflag & ICANON) && !eol)
	continue;

      n = ttyp->read_retval = bptr - buf;
      termios_printf ("about to write %d chars to inbuf", n);
      if (WriteFile (get_output_handle (), (unsigned *) buf, n, &written,
		     NULL) == FALSE)
	return -1;
      bptr = buf;
    }

  /* FIXME: This does not handle incomplete lines correctly.  The data
     is lost without ever being written anywhere.  */

  if (nraw == -1)
    {
      ttyp->read_retval = -get_errno ();
      WriteFile (get_output_handle (), (unsigned *) buf, 1, &written, NULL);
    }
  return towrite;
}

DWORD
process_input (void *arg)
{
  char rawbuf[INP_BUFFER_SIZE];
  int nraw;

  while (1)
    {
      nraw = tty_master->console->read ((void *) rawbuf,
					(size_t) INP_BUFFER_SIZE);
      tty_master->process_input_to_slave (rawbuf, nraw);
    }
}

BOOL
fhandler_pty_master::hit_eof ()
{
  if (!ttyp->inuse ())
    {
      /* We have the only remaining open handle to this pty, and
	 the slave pty has been opened at least once.  We treat
	 this as EOF.  */
      termios_printf ("all other handles closed");
      return 1;
    }
  return 0;
}

/* Process tty output requests */

int
fhandler_pty_master::process_slave_output (char *buf, size_t len)
{
  size_t rlen;
  char outbuf[OUT_BUFFER_SIZE];
  unsigned int n;
  int column = 0;

again:

  if (len == 0)
    return 0;

  if (neednl_)
    {
      /* We need to return a left over \n character, resulting from
	 \r\n conversion.  Note that we already checked for FLUSHO and
	 OutputStopped at the time that we read the character, so we
	 don't check again here.  */
      buf[0] = '\n';
      neednl_ = 0;
      return 1;
    }

  /* Set RLEN to the number of bytes to read from the pipe.  */
  rlen = len;
  if (ttyp->termios.c_oflag & OPOST && ttyp->termios.c_oflag & ONLCR)
    {
      /* We are going to expand \n to \r\n, so don't read more than
	 half of the number of bytes requested.  */
      rlen /= 2;
      if (rlen == 0)
	rlen = 1;
    }
  if (rlen > sizeof outbuf)
    rlen = sizeof outbuf;

  HANDLE handle = get_input_handle ();

  /* Doing a busy wait like this is quite inefficient, but nothing
     else seems to work completely.  Windows should provide some sort
     of overlapped I/O for pipes, or something, but it doesn't.  */
  DWORD avail;
  while (1)
    {
      if (! PeekNamedPipe (handle, NULL, 0, NULL, &avail, NULL))
	{
	  if (GetLastError () == ERROR_BROKEN_PIPE)
	    return 0;
	  __seterrno ();
	  return -1;
	}
      if (avail > 0)
	break;
      if (hit_eof ())
	return 0;
      Sleep (10);
    }

  if (ReadFile (handle, outbuf, rlen, &n, NULL) == FALSE)
    {
      if (GetLastError () == ERROR_BROKEN_PIPE)
	return 0;
      __seterrno ();
      return -1;
    }

  termios_printf ("len=%u", n);

  if (ttyp->termios.c_lflag & FLUSHO)
    {
      ttyp->write_retval = n;
      if (output_done_event != NULL)
	SetEvent (output_done_event);
      goto again;
    }
  if (ttyp->OutputStopped)
    {
      WaitForSingleObject (restart_output_event, INFINITE);
    }
  if (ttyp->termios.c_oflag & OPOST)	// post-process output
    {
      char *iptr = outbuf, *optr = buf;

      while (n--)
	{
	  switch (*iptr)
	    {
	    case '\r':
	      if ((ttyp->termios.c_oflag & ONOCR) && column == 0)
		{
		  iptr++;
		  continue;
		}
	      if (ttyp->termios.c_oflag & OCRNL)
		*iptr = '\n';
	      else
		column = 0;
	      break;
	    case '\n':
	      if (ttyp->termios.c_oflag & ONLCR)
		{
		  *optr++ = '\r';
		  column = 0;
		}
	      if (ttyp->termios.c_oflag & ONLRET)
		column = 0;
	      break;
	    default:
	      column++;
	      break;
	    }

	  /* Don't store data past the end of the user's buffer.  This
	     can happen if the user requests a read of 1 byte when
	     doing \r\n expansion.  */
	  if (optr - buf >= (int) len)
	    {
	      neednl_ = 1;
	      if (*iptr != '\n' || n != 0)
		system_printf ("internal error: %d unexpected characters", n);
	      break;
	    }

	  *optr++ = *iptr++;
	}
      return optr - buf;
    }
  else					// raw output mode
    {
      memcpy (buf, outbuf, n);
      return n;
    }
}

DWORD
process_output (void *arg)
{
  char buf[OUT_BUFFER_SIZE*2];
  int n;

  while (1)
    {
      n = tty_master->process_slave_output (buf, OUT_BUFFER_SIZE);
      if (n < 0)
	{
	  termios_printf ("ReadFile %E");
	  ExitThread (0);
	}
      if (n == 0)
	{
	  /* End of file.  */
	  ExitThread (0);
	}
      n = tty_master->console->write ((void *) buf, (size_t) n);
      tty_master->ttyp->write_retval = n == -1? -get_errno (): n;
      SetEvent (tty_master->output_done_event);
    }
}


/* Process tty ioctl requests */

DWORD
process_ioctl (void *arg)
{
  while (1)
    {
      WaitForSingleObject (tty_master->ioctl_request_event, INFINITE);
      termios_printf ("ioctl() request");
      tty_master->ttyp->ioctl_retval =
      tty_master->console->ioctl (tty_master->ttyp->cmd,
			     (void *) &tty_master->ttyp->arg);
      SetEvent (tty_master->ioctl_done_event);
    }
}

/**********************************************************************/
/* Tty slave stuff */

fhandler_tty_slave::fhandler_tty_slave(int num, const char *name) :
	fhandler_tty_common (FH_TTYS, name, num)
{
  set_cb (sizeof *this);
  ttynum = num;
  /* FIXME: This is wasteful.  We should rewrite the set_name path to eliminate the
     need for double allocates. */
  unix_path_name_ = (char *) realloc (unix_path_name_, strlen(win32_path_name_) + 1);
  strcpy (unix_path_name_, win32_path_name_);
  unix_path_name_[0] = unix_path_name_[4] = '/';
  debug_printf ("unix '%s', win32 '%s'", unix_path_name_, win32_path_name_);
  inuse_event = NULL;
}

fhandler_tty_slave::fhandler_tty_slave(const char *name) :
	fhandler_tty_common (FH_TTYS, name, 0)
{
  set_cb (sizeof *this);
  debug_printf ("here");
  inuse_event = NULL;
}

int
fhandler_tty_slave::open (const char *, int flags, mode_t)
{
  ttyp = cygwin_shared->tty[ttynum];

  if (myself->ctty < 0 && !(flags & O_NOCTTY))
    {
      myself->ctty = ttynum;
      syscall_printf ("attached tty%d sid %d, pid %d, tty->pgid %d, tty->sid %d",
		      ttynum, myself->sid, myself->pid, ttyp->pgid, ttyp->sid);
      if (myself->sid == myself->pid)
	{
	  /* We are the session leader */
	  ttyp->setsid (myself->sid);
	  ttyp->setpgid (myself->pgid);
	}
      else
	myself->sid = ttyp->getsid ();
      if (ttyp->getpgid () == 0)
	ttyp->setpgid (myself->pgid);
    }

  attach_tty (ttynum);

  set_flags (flags);
  /* Create synchronisation events */
  char buf[40];

  /* output_done_event may or may not exist.  It will exist if the tty
     was opened by fhandler_tty_master::init, normally called at
     startup if use_tty is non-zero.  It will not exist if this is a
     pty opened by fhandler_pty_master::open.  In the former case, tty
     output is handled by a separate thread which controls output.  */
  __small_sprintf (buf, OUTPUT_DONE_EVENT, ttynum);
  output_done_event = OpenEvent (EVENT_ALL_ACCESS, TRUE, buf);

  __small_sprintf (buf, OUTPUT_MUTEX, ttynum);
  output_mutex = OpenMutex (MUTEX_ALL_ACCESS, TRUE, buf);
  if (output_mutex == NULL)
    {
      termios_printf ("open output mutex failed, %E");
      __seterrno ();
      return 0;
    }

  /* The ioctl events may or may not exist.  See output_done_event,
     above.  */
  __small_sprintf (buf, IOCTL_REQUEST_EVENT, ttynum);
  ioctl_request_event = OpenEvent (EVENT_ALL_ACCESS, TRUE, buf);
  __small_sprintf (buf, IOCTL_DONE_EVENT, ttynum);
  ioctl_done_event = OpenEvent (EVENT_ALL_ACCESS, TRUE, buf);

  __small_sprintf (buf, TTY_INUSE, ttynum);
  inuse_event = OpenEvent (EVENT_MODIFY_STATE, TRUE, buf);
  termios_printf ("%s = %p", buf, inuse_event);
  if (!inuse_event)
    termios_printf ("couldn't open inuse_event, %E");

  /* Duplicate tty handles.  */

  if (!ttyp->from_slave || !ttyp->to_slave)
    {
      termios_printf ("tty handles have been closed");
      set_errno (EACCES);
      return 0;
    }

  HANDLE me = GetCurrentProcess ();
  HANDLE tty_owner = OpenProcess (PROCESS_DUP_HANDLE, FALSE,
				     ttyp->master_pid);
  if (tty_owner == NULL)
    {
      termios_printf ("can't open tty(%d) handle process %d",
		      ttynum, ttyp->master_pid);
      __seterrno ();
      return 0;
    }

  HANDLE nh;
  if (!DuplicateHandle (tty_owner, ttyp->from_master, me, &nh, 0, TRUE,
			DUPLICATE_SAME_ACCESS))
    {
      termios_printf ("can't duplicate input, %E");
      __seterrno ();
      return 0;
    }
  set_handle (nh);
  termios_printf ("duplicated from_master %p->%p from tty_owner %p",
		  ttyp->from_master, nh, tty_owner);
  if (!DuplicateHandle (tty_owner, ttyp->to_master, me, &nh, 0, TRUE,
			DUPLICATE_SAME_ACCESS))
    {
      termios_printf ("can't duplicate output, %E");
      __seterrno ();
      return 0;
    }
  set_output_handle (nh);

  ttyp->slave_opened (tty_owner);
  CloseHandle (tty_owner);

  termios_printf("tty%d opened", ttynum);

  return 1;
}

void
fhandler_tty_slave::init (HANDLE f, DWORD a, mode_t)
{
  int mode = 0;

  a &= GENERIC_READ | GENERIC_WRITE;
  if (a == GENERIC_READ)
    mode = O_RDONLY;
  if (a == GENERIC_WRITE)
    mode = O_WRONLY;
  if (a == (GENERIC_READ | GENERIC_WRITE))
    mode = O_RDWR;
  open (0, mode);
}

int
fhandler_tty_slave::close (void)
{
  if (output_done_event && !CloseHandle (output_done_event))
    termios_printf ("CloseHandle (output_done_event), %E");
  if (ioctl_done_event && !CloseHandle (ioctl_done_event))
    termios_printf ("CloseHandle (ioctl_done_event), %E");
  if (ioctl_request_event && !CloseHandle (ioctl_request_event))
    termios_printf ("CloseHandle (ioctl_request_event), %E");
  if (inuse_event)
    {
      if (!CloseHandle (inuse_event))
	termios_printf ("CloseHandle (inuse_event), %E");
      inuse_event = NULL;
    }

  if (!CloseHandle (output_mutex))
    termios_printf ("CloseHandle (output_mutex), %E");
  if (!CloseHandle (get_handle ()))
    termios_printf ("CloseHandle (get_handle ()), %E");
  if (!CloseHandle (get_output_handle ()))
    termios_printf ("CloseHandle (get_output_handle ()), %E");

  termios_printf ("tty%d closed", ttynum);

  return 0;
}

int
fhandler_tty_slave::write (const void *ptr, size_t len)
{
  unsigned int towrite=len, n;
  sigset_t mask = ~0, oldmask;

  termios_printf("tty%d, write(%x, %d)", ttynum, ptr, len);

  if (myself->pgid && ttyp->getpgid () != myself->pgid &&
	myself->ctty == ttynum && (ttyp->termios.c_lflag & TOSTOP))
    {
      /* background process */
      termios_printf("bg write pgid %d, tpgid %d, ctty %d",
		     myself->pgid, ttyp->getpgid (), myself->ctty);
      if (ttyp->termios.c_lflag & TOSTOP)
	_raise (SIGTTOU);
    }
  WaitForSingleObject (output_mutex, INFINITE);
  while (len)
    {
      n = min (OUT_BUFFER_SIZE, len);
      char *buf = (char *)ptr;
      ptr = (char *) ptr + n;
      len -= n;

      if (output_done_event != NULL)
	{
	  termios_printf("tty%d waiting for output_done", ttynum);
	  sigprocmask (SIG_SETMASK, &mask, &oldmask);
	}

      if (WriteFile (output_handle_, buf, n, &n, NULL) == FALSE)
	{
	  termios_printf ("WriteFile failed, %E");
	  ReleaseMutex (output_mutex);
	  sigprocmask (SIG_SETMASK, &oldmask, NULL);
	  _raise (SIGHUP);
	  return 0;
	}

      if (output_done_event != NULL)
	{
	  WaitForSingleObject (output_done_event, INFINITE);
	  sigprocmask (SIG_SETMASK, &oldmask, NULL);
	}

      if (ttyp->write_retval < 0)
	{
	  set_errno (-ttyp->write_retval);
	  ReleaseMutex (output_mutex);
	  return -1;
	}
    }
  ReleaseMutex (output_mutex);
  return towrite;
}

int
fhandler_tty_slave::read (void *ptr, size_t len)
{
  unsigned int n;
  int totalread = 0;
  int vmin = INT_MAX;
  int vtime = 0;	/* Initialized to prevent -Wuninitialized warning */
  char buf[INP_BUFFER_SIZE];

  termios_printf("read(%x, %d) handle %d", ptr, len, get_handle ());

  if (myself->pgid && ttyp->getpgid () != myself->pgid &&
	myself->ctty == ttynum) // background process
    {
      termios_printf("bg read pgid %d, tpgid %d, ctty %d",
			myself->pgid, ttyp->getpgid (), myself->ctty);

      if (ttyp->getsid () == 0)
	{
	  /* The pty has been closed by the master.  Return an EOF
	     indication.  FIXME: There is nothing to stop somebody
	     from reallocating this pty.  I think this is the case
	     which is handled by unlockpt on a Unix system.  */
	  return 0;
	}

      /* if process is ignoring or blocks SIGTTIN, return with error */
      if ((void *)myself->getsig(SIGTTIN).sa_handler == (void *)SIG_IGN ||
	  (myself->getsigmask () & SIGTOMASK (SIGTTIN)))
	{
	  set_errno (EIO);
	  return -1;
	}
      _raise (SIGTTIN);
    }

  if (!(ttyp->termios.c_lflag & ICANON))
    {
      vmin = ttyp->termios.c_cc[VMIN];
      vtime = ttyp->termios.c_cc[VTIME];
    }
  while (len)
    {
      if (!PeekNamedPipe (get_handle (), NULL, 0, NULL, &n, NULL))
	{
	  termios_printf("PeekNamedPipe failed, %E");
	  return 0;
	  continue;
	}
      if (n == 0)
	{
	  if (get_flags () & (O_NONBLOCK | O_NDELAY))
	    {
	      if (totalread)
		goto done;
	      set_errno (EAGAIN);
	      return -1;
	    }
	  /* We can't enter to blocking Readfile - signals will be lost!
	   * So, poll the pipe for data.
	   * FIXME: try to avoid polling...
	   */
	  if (!(ttyp->termios.c_lflag & ICANON))
	    {
	      termios_printf("vmin %d vtime %d", vmin, vtime);
	      if (vmin == 0 && vtime == 0)
		return 0;		// min = 0, time = 0
	      if (vtime == 0)
		goto wait;		// min > 0, time = 0
	      if (vmin == 0)
		{			// min = 0, time > 0
		  while (vtime--)
		    {
		      PeekNamedPipe (get_handle (), NULL, 0, NULL, &n, NULL);
		      if (n)
			break;
		      Sleep(100);
		    }
		  if (vtime == 0)
		    return 0;
		}
	      else			// min > 0, time > 0
		{
		  if (totalread == 0)
		    goto wait;
		  while (vtime--)
		    {
		      PeekNamedPipe (get_handle (), NULL, 0, NULL, &n, NULL);
		      if (n)
			break;
		      Sleep(10);
		    }
		  if (vtime == 0)
		    return totalread;
		}
	    }
	}
    wait:
      termios_printf ("reading %d bytes",
		      min ((unsigned) vmin, min (len, sizeof (buf))));
      if (ReadFile (get_handle (), (unsigned *) buf,
		 min ((unsigned) vmin, min (len, sizeof (buf))), &n, NULL) == FALSE)
	{
	  termios_printf ("read failed, %E");
	  _raise (SIGHUP);
	}
      if (ttyp->read_retval < 0)	// read error
	{
	  set_errno (-ttyp->read_retval);
	  totalread = -1;
	  break;
	}
      if (ttyp->read_retval == 0)	//EOF
	break;
      len -= n;
      totalread += n;
      memcpy (ptr, buf, n);
      ptr = (char *) ptr + n;
      if (ttyp->termios.c_lflag & ICANON)
	break;
      else if (totalread >= vmin)
	break;
      continue;
    }
done:
  termios_printf ("%d=read(%x, %d)", totalread, ptr, len);
  return totalread;
}

int
fhandler_tty_common::dup (fhandler_base *child)
{
  fhandler_tty_slave *fts = (fhandler_tty_slave *) child;
  fts->ttynum = ttynum;
  fts->ttyp = ttyp;
  return 0;
}

int
fhandler_tty_slave::dup (fhandler_base *child)
{
  fhandler_tty_slave *fts = (fhandler_tty_slave *) child;
  int errind;

  termios_printf ("in fhandler_tty_slave dup");

  attach_tty (ttynum);
  this->fhandler_tty_common::dup (child);

  const HANDLE proc = GetCurrentProcess ();
  HANDLE nh;

  if (output_done_event == NULL)
    fts->output_done_event = NULL;
  else if (!DuplicateHandle (proc, output_done_event, proc,
			     &fts->output_done_event, 0, 1,
			     DUPLICATE_SAME_ACCESS))
    {
      errind = 1;
      goto err;
    }
  if (ioctl_request_event == NULL)
    fts->ioctl_request_event = NULL;
  else if (!DuplicateHandle (proc, ioctl_request_event, proc,
			     &fts->ioctl_request_event, 0, 1,
			     DUPLICATE_SAME_ACCESS))
    {
      errind = 2;
      goto err;
    }
  if (ioctl_done_event == NULL)
    fts->ioctl_done_event = NULL;
  else if (!DuplicateHandle (proc, ioctl_done_event, proc,
			     &fts->ioctl_done_event, 0, 1,
			     DUPLICATE_SAME_ACCESS))
    {
      errind = 3;
      goto err;
    }
  if (!DuplicateHandle (proc, output_mutex, proc,
			&fts->output_mutex, 0, 1,
			DUPLICATE_SAME_ACCESS))
    {
      errind = 4;
      goto err;
    }
  if (!DuplicateHandle (proc, get_handle (), proc,
			&nh, 0, 1,
			DUPLICATE_SAME_ACCESS))
    {
      errind = 5;
      goto err;
    }
  if (!DuplicateHandle (proc, get_output_handle (), proc,
			&fts->output_handle_, 0, 1,
			DUPLICATE_SAME_ACCESS))
    {
      errind = 6;
      goto err;
    }
  if (inuse_event == NULL)
    fts->inuse_event = NULL;
  else if (!DuplicateHandle (proc, inuse_event, proc,
			     &fts->inuse_event, 0, 1,
			     DUPLICATE_SAME_ACCESS))
    {
      errind = 7;
      goto err;
    }

  fts->set_handle (nh);

  return 0;

 err:
  termios_printf ("dup %d failed in DuplicateHandle, %E", errind);
  __seterrno ();
  return -1;
}

int
fhandler_tty_slave::tcgetattr (struct termios *t)
{
  *t = ttyp->termios;
  return 0;
}

int
fhandler_tty_slave::tcsetattr (int a, const struct termios *t)
{
  WaitForSingleObject (output_mutex, INFINITE);
  ttyp->termios = *t;
  ReleaseMutex (output_mutex);
  return 0;
}

int
fhandler_tty_slave::tcflush (int a)
{
  return 0;
}

int
fhandler_tty_slave::tcsetpgrp (const pid_t pgid)
{
  termios_printf ("tcsetpgrp (%d) (sid = %d tsid=%d)", pgid,
		    myself->sid, ttyp->getsid ());
  if (myself->sid != ttyp->getsid ())
    {
      set_errno (EPERM);
      return -1;
    }
  ttyp->setpgid (pgid);
  return 0;
}

int
fhandler_tty_slave::tcgetpgrp (void)
{
  return ttyp->getpgid ();
}

void
fhandler_tty_slave::send_ioctl_request (void)
{
  sigset_t mask = ~0, oldmask;

  if (ioctl_request_event == NULL || ioctl_done_event == NULL) // slave of pty
    {
      return;
    }

  sigprocmask (SIG_SETMASK, &mask, &oldmask);
  WaitForSingleObject (output_mutex, INFINITE);
  SetEvent (ioctl_request_event);
  WaitForSingleObject (ioctl_done_event, INFINITE);
  ReleaseMutex (output_mutex);
  sigprocmask (SIG_SETMASK, &oldmask, NULL);
}

int
fhandler_tty_slave::ioctl (unsigned int cmd, void *arg)
{
  termios_printf ("ioctl (%x)", cmd);

  if (myself->pgid && ttyp->getpgid () != myself->pgid &&
	 myself->ctty == ttynum && (ttyp->termios.c_lflag & TOSTOP))
    {
      /* background process */
      termios_printf("bg ioctl pgid %d, tpgid %d, ctty %d",
		     myself->pgid, ttyp->getpgid (), myself->ctty);
      _raise (SIGTTOU);
    }
  ttyp->cmd = cmd;
  ttyp->ioctl_retval = 0;
  switch (cmd)
    {
    case TIOCGWINSZ:
      ttyp->arg.winsize = ttyp->winsize;
      send_ioctl_request ();
      * (struct winsize *) arg = ttyp->arg.winsize;
      ttyp->winsize = ttyp->arg.winsize;
      break;
    case TIOCSWINSZ:
      ttyp->ioctl_retval = -1;
      ttyp->arg.winsize = * (struct winsize *) arg;
      send_ioctl_request ();
      break;
    case FIONBIO:
      if (* (int *) arg)
	set_flags (get_flags () | O_NONBLOCK);
      else
	set_flags (get_flags () & ~O_NONBLOCK);
      break;
    default:
      set_errno (EINVAL);
      return -1;
    }
  termios_printf ("%d = ioctl (%x)", ttyp->ioctl_retval, cmd);
  return ttyp->ioctl_retval;
}

/*******************************************************
 fhandler_pty_master
*/
fhandler_pty_master::fhandler_pty_master (const char *name, DWORD devtype) :
	fhandler_tty_common (devtype, name)
{
  set_cb (sizeof *this);
  ioctl_request_event = NULL;
  ioctl_done_event = NULL;
  restart_output_event = NULL;
  pktmode = neednl_ = 0;
}

int
fhandler_pty_master::open (const char *, int flags, mode_t)
{
  ttynum = cygwin_shared->tty.allocate_tty (0);
  if (ttynum < 0)
    return 0;

  cygwin_shared->tty[ttynum]->common_init (this);

  set_flags (flags);

  return 1;
}

int
fhandler_pty_master::close (void)
{
  if (output_done_event && !CloseHandle (output_done_event))
    termios_printf ("CloseHandle (output_done_event), %E");
  if (ioctl_done_event && !CloseHandle (ioctl_done_event))
    termios_printf ("CloseHandle (ioctl_done_event), %E");
  if (ioctl_request_event && !CloseHandle (ioctl_request_event))
    termios_printf ("CloseHandle (ioctl_request_event), %E");
  if (!CloseHandle (output_mutex))
    termios_printf ("CloseHandle (output_mutex), %E");
  if (!CloseHandle (restart_output_event))
    termios_printf ("CloseHandle (restart_output_event), %E");
  if (!CloseHandle (get_handle ()))
    termios_printf ("CloseHandle (get_handle ()), %E");
  if (!CloseHandle (get_output_handle ()))
    termios_printf ("CloseHandle (get_output_handle ()), %E");

  if (ttyp->master_pid == GetCurrentProcessId ())
    {
      termios_printf ("freeing tty%d (%d)", ttynum, ttyp->ntty);
      if (ttyp->to_slave)
	CloseHandle (ttyp->to_slave);
      if (ttyp->from_slave)
	CloseHandle (ttyp->from_slave);
      if (ttyp->from_master)
	CloseHandle (ttyp->from_master);
      if (ttyp->to_master)
	CloseHandle (ttyp->to_master);
      if (ttyp->inuse_event)
	CloseHandle (ttyp->inuse_event);
      ttyp->init ();
    }

  termios_printf("tty%d closed", ttynum);

  return 0;
}

int
fhandler_pty_master::write (const void *ptr, size_t len)
{
  process_input_to_slave ((char *) ptr, len);
  return len;
}

int
fhandler_pty_master::read (void *ptr, size_t len)
{
  DWORD n;
  char *cptr = (char *) ptr;

  if (! PeekNamedPipe (get_handle (), NULL, 0, NULL, &n, NULL))
    {
      if (GetLastError () == ERROR_BROKEN_PIPE)
	{
	  /* On Unix, a read from a broken pipe returns EOF.  */
	  return 0;
	}
      __seterrno ();
      return -1;
    }
  if (n == 0
      && (get_flags () & (O_NONBLOCK | O_NDELAY)) != 0)
    {
      set_errno (EAGAIN);
      return -1;
    }
  if (pktmode)
    {
      *cptr++ = TIOCPKT_DATA;
      len--;
    }
  n = process_slave_output (cptr, len);
  if (n < 0)
    return -1;
  if (output_done_event != NULL)
    SetEvent (output_done_event);
  if (pktmode && n > 0)
    n++;
  return n;
}

int
fhandler_pty_master::tcgetattr (struct termios *t)
{
  *t = cygwin_shared->tty[ttynum]->termios;
  return 0;
}

int
fhandler_pty_master::tcsetattr (int a, const struct termios *t)
{
  cygwin_shared->tty[ttynum]->termios = *t;
  return 0;
}

int
fhandler_pty_master::tcflush (int a)
{
  return 0;
}

int
fhandler_pty_master::ioctl (unsigned int cmd, void *arg)
{
  switch (cmd)
    {
      case TIOCPKT:
	pktmode = * (int *) arg;
	break;
      case TIOCGWINSZ:
	* (struct winsize *) arg = ttyp->winsize;
	break;
      case TIOCSWINSZ:
	ttyp->winsize = * (struct winsize *) arg;
	_kill (-ttyp->getpgid (), SIGWINCH);
	break;
      case FIONBIO:
	if (* (int *) arg)
	  set_flags (get_flags () | O_NONBLOCK);
	else
	  set_flags (get_flags () & ~O_NONBLOCK);
	break;
      default:
	set_errno (EINVAL);
	return -1;
    }
  return 0;
}

char *
fhandler_pty_master::ptsname (void)
{
  static char buf[32];

  __small_sprintf (buf, "/dev/tty%d", ttynum);
  return buf;
}

void
fhandler_tty_common::set_close_on_exec (int val)
{
  this->fhandler_base::set_close_on_exec (val);
  if (output_done_event)
    set_inheritance (output_done_event, val);
  if (ioctl_request_event)
    set_inheritance (ioctl_request_event, val);
  if (ioctl_done_event)
    set_inheritance (ioctl_done_event, val);
  set_inheritance (output_mutex, val);
  set_inheritance (output_handle_, val);
}

void
fhandler_tty_slave::set_close_on_exec (int val)
{
  this->fhandler_tty_common::set_close_on_exec (val);
  if (inuse_event)
    set_inheritance (inuse_event, val);
}

void
fhandler_tty_common::fixup_after_fork (HANDLE parent)
{
  this->fhandler_base::fixup_after_fork (parent);
  if (output_done_event)
    fork_fixup (parent, output_done_event);
  if (ioctl_request_event)
    fork_fixup (parent, ioctl_request_event);
  if (ioctl_done_event)
    fork_fixup (parent, ioctl_done_event);
  if (output_mutex)
    fork_fixup (parent, output_mutex);
  fork_fixup (parent, output_handle_);
}

void
fhandler_tty_slave::fixup_after_fork (HANDLE parent)
{
  this->fhandler_tty_common::fixup_after_fork (parent);
  fork_fixup (parent, inuse_event);
}

void
fhandler_pty_master::set_close_on_exec (int val)
{
  this->fhandler_tty_common::set_close_on_exec (val);
  set_inheritance (restart_output_event, val);
  if (ttyp->master_pid == GetCurrentProcessId ())
    {
      ttyp->from_slave = get_handle ();
      ttyp->to_slave = get_output_handle ();
    }
}

void
fhandler_pty_master::fixup_after_fork (HANDLE child)
{
  this->fhandler_tty_common::fixup_after_fork (child);
  if (restart_output_event)
    fork_fixup (child, restart_output_event);
}
