/* termios.cc: termios for WIN32.

   Copyright 1996, 1997, 1998 Cygnus Solutions.

   Written by Doug Evans and Steve Chamberlain of Cygnus Support
   dje@cygnus.com, sac@cygnus.com

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <errno.h>
#include "winsup.h"
#include <sys/termios.h>

/* tcsendbreak: POSIX 7.2.2.1 */
extern "C"
int
tcsendbreak (int fd, int duration)
{
  int res = -1;

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    res = dtable[fd]->tcsendbreak (duration);

  syscall_printf ("%d = tcsendbreak (%d, %d )", res, fd, duration);
  return res;
}

/* tcdrain: POSIX 7.2.2.1 */
extern "C"
int
tcdrain (int fd)
{
  int res = -1;

  termios_printf ("tcdrain");

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    res = dtable[fd]->tcdrain ();

  syscall_printf ("%d = tcdrain (%d)", res, fd);
  return res;
}

/* tcflush: POSIX 7.2.2.1 */
extern "C"
int
tcflush (int fd, int queue)
{
  int res = -1;

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    res = dtable[fd]->tcflush (queue);

  termios_printf ("%d = tcflush (%d, %d)", res, fd, queue);
  return res;
}

/* tcflow: POSIX 7.2.2.1 */
extern "C"
int
tcflow (int fd, int action)
{
  int res = -1;

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    {
      sig_protect (here, 1);
      res = dtable[fd]->tcflow (action);
    }

  syscall_printf ("%d = tcflow (%d, %d)", res, fd, action);
  return res;
}

/* tcsetattr: POSIX96 7.2.1.1 */
extern "C"
int
tcsetattr (int fd, int a, const struct termios *t)
{
  int res = -1;

  t = __tonew_termios (t);
  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    {
      sig_protect (here, 1);
      res = dtable[fd]->tcsetattr (a, t);
    }

  termios_printf ("iflag %x, oflag %x, cflag %x, lflag %x, VMIN %d, VTIME %d",
	t->c_iflag, t->c_oflag, t->c_cflag, t->c_lflag, t->c_cc[VMIN],
	t->c_cc[VTIME]);
  termios_printf ("%d = tcsetattr (%d, %d, %x)", res, fd, a, t);
  return res;
}

/* tcgetattr: POSIX 7.2.1.1 */
extern "C"
int
tcgetattr (int fd, struct termios *in_t)
{
  int res = -1;
  struct termios *t = __makenew_termios (in_t);

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    {
      sig_protect (here, 1);
      if ((res = dtable[fd]->tcgetattr (t)) == 0)
	__toapp_termios (in_t, t); 
    }

  if (res)
    termios_printf ("%d = tcgetattr (%d, %x)", res, fd, in_t);
  else
    termios_printf ("iflag %x, oflag %x, cflag %x, lflag %x, VMIN %d, VTIME %d",
	  t->c_iflag, t->c_oflag, t->c_cflag, t->c_lflag, t->c_cc[VMIN],
	  t->c_cc[VTIME]);

  return res;
}

/* tcgetpgrp: POSIX 7.2.3.1 */
extern "C"
int
tcgetpgrp (int fd)
{
  int res = -1;

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    res = dtable[fd]->tcgetpgrp ();

  termios_printf ("%d = tcgetpgrp (%d)", res, fd);
  return res;
}

/* tcsetpgrp: POSIX 7.2.4.1 */
extern "C"
int
tcsetpgrp (int fd, pid_t pgid)
{
  int res = -1;

  if (dtable.not_open (fd))
    set_errno (EBADF);
  else if (!dtable[fd]->is_tty ())
    set_errno (ENOTTY);
  else
    res = dtable[fd]->tcsetpgrp (pgid);

  termios_printf ("%d = tcsetpgrp (%d, %x)", res, fd, pgid);
  return res;
}

/* NIST PCTS requires not macro-only implementation */
#undef cfgetospeed
#undef cfgetispeed
#undef cfsetospeed
#undef cfsetispeed

/* cfgetospeed: POSIX96 7.1.3.1 */
extern "C"
speed_t
cfgetospeed (struct termios *tp)
{
  return __tonew_termios(tp)->c_ospeed;
}

/* cfgetispeed: POSIX96 7.1.3.1 */
extern "C"
speed_t
cfgetispeed (struct termios *tp)
{
  return __tonew_termios(tp)->c_ispeed;
}

/* cfsetospeed: POSIX96 7.1.3.1 */
extern "C"
int
cfsetospeed (struct termios *in_tp, speed_t speed)
{
  struct termios *tp = __tonew_termios (in_tp);
  tp->c_ospeed = speed;
  __toapp_termios (in_tp, tp);
  return 0;
}

/* cfsetispeed: POSIX96 7.1.3.1 */
extern "C"
int
cfsetispeed (struct termios *in_tp, speed_t speed)
{
  struct termios *tp = __tonew_termios (in_tp);
  tp->c_ispeed = speed;
  __toapp_termios (in_tp, tp);
  return 0;
}
