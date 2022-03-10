/* tty.cc

   Copyright 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <errno.h>
#include <unistd.h>
#include <utmp.h>
#include "winsup.h"

extern fhandler_tty_master *tty_master;

extern "C"
int
grantpt (void)
{
  return 0;
}

extern "C"
int
unlockpt (void)
{
  return 0;
}

extern "C"
int
ttyslot (void)
{
  if (NOTSTATE (myself, PID_USETTY))
    return -1;
  return myself->ctty;
}

void
tty_init (void)
{
  if (NOTSTATE (myself, PID_USETTY))
    return;
  if (myself->ctty == -1)
    if (NOTSTATE (myself, PID_CYGPARENT))
      myself->ctty = attach_tty (myself->ctty);
    else
      return;
  if (myself->ctty == -1)
    termios_printf ("Can't attach to tty");
}

/* Create session's master tty */

void
create_tty_master (int ttynum)
{
  tty_master = (fhandler_tty_master *) dtable.build_fhandler (-1, FH_TTYM,
							     "/dev/ttym");
  if (tty_master->init (ttynum))
    api_fatal ("Can't create master tty");
  else
    {
      /* Log utmp entry */
      struct utmp our_utmp;

      bzero ((char *) &our_utmp, sizeof (utmp));
      (void) time (&our_utmp.ut_time);
      strncpy (our_utmp.ut_name, getlogin (), sizeof (our_utmp.ut_name));
      gethostname (our_utmp.ut_host, sizeof (our_utmp.ut_host));
      __small_sprintf (our_utmp.ut_line, "tty%d", ttynum);
      our_utmp.ut_type = USER_PROCESS;
      myself->ctty = ttynum;
      login (&our_utmp);
    }
}

void
tty_terminate (void)
{
  if (NOTSTATE (myself, PID_USETTY))
    return;
  cygwin_shared->tty.terminate ();
}

int
attach_tty (int num)
{
  if (num != -1)
    {
      return cygwin_shared->tty.connect_tty (num);
    }
  if (NOTSTATE (myself, PID_USETTY))
    return -1;
  return cygwin_shared->tty.allocate_tty (1);
}

void
tty_list::terminate (void)
{
  int ttynum = myself->ctty;

  /* Keep master running till there are connected clients */
  if (ttynum != -1 && ttys[ttynum].master_pid == GetCurrentProcessId ())
    {
      tty *t = ttys + ttynum;
      t->slave_opened (GetCurrentProcess ());
      /* Wait for children which rely on tty handling in this process to
	 go away */
      for (int i = 0; ; i++)
	{
	  if (!t->inuse ())
	    break;
	  if (i >= 100)
	    {
	      small_printf ("waiting for children using tty%d to terminate",
			    ttynum);
	      i = 0;
	    }
	
	  Sleep (200);
	}

      termios_printf ("tty %d master about to finish", ttynum);
      CloseHandle (t->to_slave);
      CloseHandle (t->from_slave);
      WaitForSingleObject (tty_master->hThread, INFINITE);
      t->init ();

      char buf[20];
      __small_sprintf (buf, "tty%d", ttynum);
      logout (buf);
    }
}

int
tty_list::connect_tty (int ttynum)
{
  if (ttynum < 0 || ttynum >= NTTYS)
    {
      termios_printf ("ttynum (%d) out of range", ttynum);
      return -1;
    }
  if (!ttys[ttynum].inuse_event_exists ())
    {
      termios_printf ("tty %d was not allocated", ttynum);
      return -1;
    }

  return ttynum;
}

void
tty_list::init (void)
{
  for (int i = 0; i < NTTYS; i++)
    {
      ttys[i].init ();
      ttys[i].setntty (i);
    }
}

/* Search for tty class for our console. Allocate new tty if our process is
   the only cygwin process in the current console.
   Return tty number or -1 if error.
   If flag == 0, just find a free tty.
 */
int
tty_list::allocate_tty (int flag)
{
  HWND console;

  if (!flag)
    console = NULL;
  else
    {
      char *oldtitle = new char [TITLESIZE];

      if (!oldtitle)
	{
	  termios_printf ("Can't *allocate console title buffer");
	  return -1;
	}
      if (!GetConsoleTitle (oldtitle, TITLESIZE))
	{
	  termios_printf ("Can't read console title");
	  return -1;
	}

      if (WaitForSingleObject (title_mutex, INFINITE) == WAIT_FAILED)
	{
	  termios_printf ("WFSO failed, %E");
	}

      char buf[40];

      __small_sprintf (buf, "Cygwin.find.console.%d", myself->pid);
      SetConsoleTitle (buf);
      Sleep (40);
      console = FindWindow (NULL, buf);
      SetConsoleTitle (oldtitle);
      Sleep (40);
      ReleaseMutex (title_mutex);
      if (console == NULL)
	{
	  termios_printf ("Can't find console window");
	  return -1;
	}
    }
  /* Is a tty allocated for console? */

  int freetty = -1;
  for (int i = 0; i < NTTYS; i++)
    {
      if (! ttys[i].inuse_event_exists ())	/* unused tty */
	{
	  if (freetty == -1)
	    freetty = i;
	  if (flag == 0)
	    break;
	}
      if (flag && ttys[i].gethwnd () == console)
	{
	  termios_printf ("console %x already associated with tty%d",
		console, i);
	  /* Is the master alive? */
	  HANDLE hMaster;
	  hMaster = OpenProcess (PROCESS_DUP_HANDLE, FALSE, ttys[i].master_pid);
	  if (hMaster)
	    {
	      CloseHandle (hMaster);
	      return i;
	    }
	  /* Master is dead */
	  freetty = i;
	  break;
	}
    }

  /* There is no tty allocated to console, allocate the first free found */
  if (freetty == -1)
    {
      system_printf ("No free ttys available");
      return -1;
    }
  tty *t = ttys + freetty;
  t->init ();
  t->setsid (myself->sid);
  t->setpgid (myself->pgid);
  t->sethwnd (console);

  if (flag)
    {
      termios_printf ("console %x associated with tty%d", console, freetty);
      create_tty_master (freetty);
    }
  else
    termios_printf ("tty%d allocated", freetty);
  return freetty;
}

BOOL
tty::inuse_event_exists ()
{
  HANDLE ev;
  char buf[sizeof (TTY_INUSE) + 8];

  __small_sprintf (buf, TTY_INUSE, ntty);
  if ((ev = OpenEvent (EVENT_ALL_ACCESS, TRUE, buf)))
    CloseHandle (ev);
  return ev != NULL;
}

BOOL
tty::inuse ()
{
  return inuse_event || inuse_event_exists ();
}

void
tty::init (void)
{
  OutputStopped = 0;
  sid = 0;
  pgid = 0;
  hwnd = NULL;
  to_slave = NULL;
  from_slave = NULL;
  inuse_event = NULL;
}

HANDLE
tty::get_event (const char *fmt, BOOL inherit)
{
  HANDLE hev;
  char buf[40];

  __small_sprintf (buf, fmt, ntty);
  if (!(hev = CreateEvent (inherit ? &sec_all : &sec_all_nih, FALSE, FALSE, buf)))
    {
      termios_printf ("couldn't create %s", buf);
      set_errno (ENOENT);	/* FIXME this can't be the right errno */
      return NULL;
    }

  termios_printf ("created event %s", buf);
  return hev;
}

int
tty::make_pipes (fhandler_pty_master *ptym)
{
  /* Create communication pipes */

  /* FIXME: should this be sec_none_nih? */
  if (CreatePipe (&from_master, &to_slave, &sec_all, 0) == FALSE)
    {
      termios_printf ("can't create input pipe");
      set_errno (ENOENT);
      return FALSE;
    }

  if (CreatePipe (&from_slave, &to_master, &sec_all, 0) == FALSE)
    {
      termios_printf ("can't create output pipe");
      set_errno (ENOENT);
      return FALSE;
    }
  termios_printf ("tty%d from_slave %p, to_slave %p", ntty, from_slave,
		  to_slave);
  ptym->set_handle (from_slave);
  ptym->set_output_handle (to_slave);
  return TRUE;
}

BOOL
tty::common_init (fhandler_pty_master *ptym)
{
  ptym->ttyp = this;
  if (!make_pipes (ptym))
    return FALSE;
  ptym->neednl_ = 0;

  /* Save our pid  */

  master_pid = GetCurrentProcessId ();

  /* Allow the others to open us (for handle duplication) */

  HANDLE me = GetCurrentProcess ();
  if ((os_being_run == winNT) &&
      (SetKernelObjectSecurity (me, DACL_SECURITY_INFORMATION,
			       get_null_sd ()) == FALSE))
    small_printf ("Can't set process security, %E");

  /* Create synchronisation events */

  if (!(inuse_event = get_event (TTY_INUSE, TRUE)))
    return FALSE;
  termios_printf ("got inuse_event for tty %d", ntty);

  if (!(ptym->restart_output_event = get_event (RESTART_OUTPUT_EVENT, TRUE)))
    return FALSE;

  if (ptym->get_device () != FH_TTYM)
    {
      ptym->output_done_event = ptym->ioctl_done_event =
      ptym->ioctl_request_event = NULL;
    }
  else
    {
      if (!(ptym->output_done_event = get_event (OUTPUT_DONE_EVENT, FALSE)))
	return FALSE;
      if (!(ptym->ioctl_done_event = get_event (IOCTL_DONE_EVENT, FALSE)))
	return FALSE;
      if (!(ptym->ioctl_request_event = get_event (IOCTL_REQUEST_EVENT, FALSE)))
	return FALSE;
    }

  char buf[40];
  __small_sprintf (buf, OUTPUT_MUTEX, ntty);
  if (!(ptym->output_mutex = CreateMutex (&sec_all, FALSE, buf)))
    {
      termios_printf ("can't create %s", buf);
      set_errno (ENOENT);
      return FALSE;
    }

  /* Initial termios values */

  termios.c_iflag = BRKINT | ICRNL | IXON;
  termios.c_oflag = OPOST | ONLCR;
  termios.c_cflag = B9600 | CS8 | CREAD;
  termios.c_lflag = ISIG | ICANON | ECHO | IEXTEN;
  termios.c_cc[VDISCARD]	= CFLUSH;
  termios.c_cc[VEOL]	= CEOL;
  termios.c_cc[VEOL2]	= CEOL2;
  termios.c_cc[VEOF]	= CEOF;
  termios.c_cc[VERASE]	= CERASE;
  termios.c_cc[VINTR]	= CINTR;
  termios.c_cc[VKILL]	= CKILL;
  termios.c_cc[VLNEXT]	= CLNEXT;
  termios.c_cc[VMIN]	= 1;
  termios.c_cc[VQUIT]	= CQUIT;
  termios.c_cc[VREPRINT]	= CRPRNT;
  termios.c_cc[VSTART]	= CSTART;
  termios.c_cc[VSTOP]	= CSTOP;
  termios.c_cc[VSUSP]	= CSUSP;
  termios.c_cc[VSWTC]	= CSWTCH;
  termios.c_cc[VTIME]	= 0;
  termios.c_cc[VWERASE]	= CWERASE;

  cfsetispeed (&termios, B38400);
  cfsetospeed (&termios, B38400);

  winsize.ws_col = 80;
  winsize.ws_row = 25;

  termios_printf("tty%d opened", ntty);
  return TRUE;
}

void
tty::slave_opened (HANDLE tty_owner)
{
  termios_printf ("tty%d inuse_event %p", ntty, inuse_event);
  if (inuse_event)
    {
      HANDLE nh = NULL;
      (void) DuplicateHandle (tty_owner, inuse_event, GetCurrentProcess (),
			      &nh, 0, FALSE,
			      DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
      inuse_event = NULL;
      if (nh)
	CloseHandle (nh);
      termios_printf ("tty%d inuse_event zapped", ntty);
    }
}
