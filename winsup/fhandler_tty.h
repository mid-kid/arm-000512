/* fhandler_tty.h

   Copyright 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#ifndef _TTY_H
#define _TTY_H

#include <sys/termios.h>

#define INP_BUFFER_SIZE 256
#define OUT_BUFFER_SIZE 256
#define NTTYS		128

/* Input/Output/ioctl events */

#define OUTPUT_DONE_EVENT	"Cygwin.tty%d.output.done"
#define IOCTL_REQUEST_EVENT	"Cygwin.tty%d.ioctl.request"
#define IOCTL_DONE_EVENT	"Cygwin.tty%d.ioctl.done"
#define RESTART_OUTPUT_EVENT	"Cygwin.tty%d.output.restart"
#define OUTPUT_MUTEX		"Cygwin.tty%d.output.mutex"
#define TTY_INUSE		"Cygwin.tty%x.inuse"

class tty {
  HANDLE get_event (const char *fmt, BOOL inherit);
public:
  int ntty;
  struct termios termios;
  struct winsize winsize;

  /* ioctl requests buffer */
  int cmd;
  union {
    struct termios termios;
    struct winsize winsize;
    int value;
    pid_t pid;
  } arg;
  /* XXX_retval variables holds master's completion codes. Error are stored as
   * -ERRNO
   */
  int ioctl_retval;

  int read_retval;

  BOOL OutputStopped;
  int write_retval;

  pid_t pgid;	/* Process group ID of tty */
  pid_t sid;	/* Session ID of tty */
  HWND  hwnd;	/* Console window handle tty belongs to */

  DWORD master_pid;	/* Win32 PID of tty master process */

  HANDLE from_master, to_slave;
  HANDLE from_slave, to_master;

  HANDLE inuse_event;	// Event which exists as a flag that tty is
			// in use.  If this event does not exist, then
			// no process is currently accessing the tty.

  void init ();
  BOOL common_init (fhandler_pty_master *);
  BOOL inuse ();
  BOOL inuse_event_exists ();
  int getsid () {return sid;}
  void setsid (pid_t tsid) {sid = tsid;}
  pid_t getpgid () {return pgid;}
  void setpgid (pid_t gid) {pgid = gid;}
  void setntty (int n) {ntty = n;}
  HWND gethwnd () {return hwnd;}
  void sethwnd (HWND wnd) {hwnd = wnd;}
  int make_pipes (fhandler_pty_master *ptym);
  void slave_opened (HANDLE tty_owner);
};

class tty_list {
  tty ttys[NTTYS];

public:
  tty * operator [](int n) {return ttys + n;}
  int allocate_tty (int n); /* n non zero if allocate a tty, pty otherwise */
  int connect_tty (int);
  void terminate ();
  void init ();
};

void tty_init ();
void tty_terminate ();
int attach_tty (int);
void create_tty_master (int);
extern "C" int ttyslot (void);

#endif /* _TTY_H */
