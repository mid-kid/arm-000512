/* strace.cc: system/windows tracing

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "winsup.h"

#define PROTECT(x) x[sizeof(x)-1] = 0
#define CHECK(x) if (x[sizeof(x)-1] != 0) { small_printf("array bound exceeded %d\n", __LINE__); ExitProcess(1); }

/* 'twould be nice to declare this in winsup.h but winsup.h doesn't require
   stdarg.h, so we declare it here instead. */

#ifndef NOSTRACE

static long long hires_frequency = 0;
static int hires_initted = 0;

static int strace_microseconds()
{
  static int first_microsec = 0;
  int microsec;
  if (!hires_initted)
    {
      hires_initted = 1;
      QueryPerformanceFrequency ((LARGE_INTEGER *) &hires_frequency);
      if (hires_frequency == 0)
	  hires_initted = 2;
    }
  if (hires_initted == 2)
    {
      int count = GetTickCount ();
      microsec = count * 1000;
    }
  else
    {
      long long thiscount;
      QueryPerformanceCounter ((LARGE_INTEGER *) &thiscount);
      thiscount = (long long) (((double) thiscount/(double) hires_frequency)
			       * 1000000.0);
      microsec = thiscount;
    }
  if (first_microsec == 0)
    first_microsec = microsec;
  return microsec - first_microsec;
}

/* Whether strace is going to stderr.  */
static int strace_stderr = 1;

/* Mutex so output from several processes doesn't get intermixed.  */
static HANDLE strace_mutex = NULL;

static DWORD strace_keep = 0;

/* Non-zero if we're to flush the output buffer after every message.
   This slows things down quite a bit, but is useful when a SIGSEGV
   causes program termination before all the output has been printed.  */

void
strace_open (const char *fn)
{
  HANDLE h;
  DWORD flags = FILE_ATTRIBUTE_NORMAL;

  if ((myself->strace_mask & _STRACE_NOMUTEX))
    flags |= FILE_FLAG_WRITE_THROUGH;

  if (myself->strace_file != NULL &&
      myself->strace_file != GetStdHandle (STD_ERROR_HANDLE))
    CloseHandle (myself->strace_file);

  /* Use OPEN_ALWAYS so forks don't clobber each other.  */
  h = CreateFileA (fn,
		   GENERIC_WRITE,
		   /* FILE_SHARE_READ: Let other tasks read the trace
		      while it's in progress.  */
		   FILE_SHARE_READ | FILE_SHARE_WRITE,
		   &sec_none,
		   OPEN_ALWAYS,
		   FILE_ATTRIBUTE_NORMAL,
		   0);
  if (h == INVALID_HANDLE_VALUE)
    {
      small_printf ("Unable to open trace file %s, using stderr.", fn);
      strace_stderr = TRUE;
    }
  else
    {
      myself->strace_file = h;
      strace_stderr = FALSE;
    }
}

/* Environment variable `STRACE' is used to turn on tracing.
   Its format is n[:cache][,filename]
   where N is one or more flags (see sys/strace.h) expressed in decimal, or
   in 0x notation for hexadecimal, 0n notation for octal.  A value of 1
   turns on all tracing.

   CACHE is the optional number of lines of information to keep in memory.
   Writing the log messages to memory causes strace_printf to execute more
   quickly, which helps resolve timing issues.  The cache is flushed when the
   user types 'CTRL-T' (see tty.cc), or if _STRACE_EXITDUMP is set
   (see sys/strace.h).

   FILENAME is an optional file to use instead of STD_ERROR_HANDLE.

   Normally tracing output isn't flushed as it slows things immensely.
   However, it can (I think) cause tracing data to be lost of the program
   crashes at the wrong time.  To turn on flushing, use _STRACE_FLUSH.

   Stracing can be turned on after the program is running.  This currently
   requires the program to be ready for it (env var strace is set).  This is
   done so that the trace mutex and file aren't opened for *every* process.
   To enable stracing to be turned on later, but without turning it on
   immediately, set env var strace to 0.  */

void
strace_init (const char *buf)
{
  /* The value of myself->strace_mask is inherited from the parent
     and is copied over before we start */

  if (buf && *buf)
    {
      if (myself->strace_file != NULL &&
	  myself->strace_file != GetStdHandle (STD_ERROR_HANDLE))
	(void)CloseHandle (myself->strace_file);
      myself->strace_file = GetStdHandle (STD_ERROR_HANDLE);
      strace_stderr = 1;

      char *p;

      myself->strace_mask = strtol (buf, &p, 0);

      if (*p == ':')
	{
	  strace_keep = strtol (p + 1, &p, 0);
	  myself->strace_mask |= _STRACE_CACHE;
	}

      if (*p == ',')
	strace_open (p + 1);
    }
}

void
release_strace_mutex ()
{
  while (ReleaseMutex (strace_mutex))
    continue;
}

/* The reason for the mutex is to keep the debugging output from several
   processes from intermixing.  However, it's not critical and we
   certainly don't want to wait too long if the system breaks down. */
void
get_strace_mutex ()
{
  DWORD rc;
  if (strace () & _STRACE_NOMUTEX)
    return;
  if (!strace_mutex)
      strace_mutex = CreateMutex (&sec_none, FALSE, shared_name ("strace", 0));
  if (strace_mutex &&
      (rc = WaitForSingleObject (strace_mutex, 100)) != WAIT_OBJECT_0)
    /*small_printf ("strace WFSO(%p) failed %d, %E", strace_mutex, r)*/;
}

struct msglist
  {
    struct msglist *next;
    char buf[0];
  };

static struct msglist NO_COPY headlist = {0};

static struct msglist NO_COPY *endlist = &headlist;
static DWORD NO_COPY inqueue = 0;

/* sprintf analog for use by output routines. */
static int
strace_vsprintf (char *buf, const char *infmt, va_list ap)
{
  int count;
  char *fmt, *unkfmt;
  char fmtbuf[80], unkfmtbuf[80];
  static int nonewline = FALSE;

#ifndef STRACE_HHMMSS
  static int lmicrosec = 0;
  int microsec = strace_microseconds ();
  int dmicrosec = lmicrosec ? microsec - lmicrosec : 0;
  lmicrosec = microsec;

  __small_sprintf (fmt = fmtbuf, "%5d %7d [%s] %s ",
		   dmicrosec, microsec, threadname (0), "%s %d");
  __small_sprintf (unkfmt = unkfmtbuf, "%6d %7d [%s] %s ",
		   dmicrosec, microsec, threadname (0),
		   "(unknown)");
#else
  int ss, mm;
  int t = time (NULL);
  ss = t % 60; t /= 60;
  mm = t % 60; t /= 60;
  t = t % 24;
  __small_sprintf (fmt = fmtbuf, "%02d:%02d:%02d [%s] %s ", t, mm, ss,
		   threadname (0), "%s %d");
  __small_sprintf (unkfmt = unkfmtbuf, "%02d:%02d:%02d [%s] %s ", t, mm, ss,
		   threadname (0), "***");
#endif

  if (nonewline)
    {
      count = 0;
      if (strncmp (infmt, "%F: ", 4) == 0)
	{
	  infmt += 4;
	  (void) va_arg (ap, char *);
	}
    }
  else if (!user_data)
    count = __small_sprintf (buf, unkfmt);
  else
    {
      char *p, progname[sizeof (myself->progname)];
      static BOOL NO_COPY output_path_once = FALSE;
      if (!output_path_once)
	output_path_once = !!(p = myself->progname);
      else
	{
	  if ((p = strrchr (myself->progname, '\\')) != NULL)
	    p++;
	  else
	    p = myself->progname;
	  strcpy (progname, p);
	  if ((p = strrchr (progname, '.')) != NULL)
	    *p = '\000';
	  p = progname;
	}
      count = __small_sprintf (buf, fmt, p && *p ? p : "(unknown)",
			       myself->pid);
    }

  count += __small_vsprintf (buf + count, infmt, ap);
  char *p;
  for (p = buf + count; p > buf; p--)
    switch (p[-1])
      {
	case '\n':
	  p[-1] = '\0';
	  break;
	case '\b':
	  *--p = '\0';
	   nonewline = TRUE;
	  goto done;
	default:
	  goto addnl;
      }

addnl:
  *p++ = '\n';
  *p = '\0';
  nonewline = FALSE;

done:
  return p - buf;
}

/* Write to strace file or strace queue. */
static void
strace_write (const char *buf, int count)
{
  get_strace_mutex ();
  if (!strace_keep)
    {
      DWORD done;
      (void) SetFilePointer (myself->strace_file, 0, 0, FILE_END);
      WriteFile (myself->strace_file, buf, count, &done, 0);
      if (myself->strace_mask & _STRACE_FLUSH)
	FlushFileBuffers (myself->strace_file);
    }
  else
    {
      struct msglist *m = NULL;
      if (++inqueue >= strace_keep)
	{
	  inqueue = strace_keep - 1;
	  m = headlist.next;
	  headlist.next = m->next;
	  HeapFree (GetProcessHeap (), 0, (LPVOID) m);
	}
      m = (struct msglist *)HeapAlloc (GetProcessHeap (), 0,
				       sizeof (*m) + strlen (buf) + 1);
      strcpy (m->buf, buf);
      endlist->next = m;
      (endlist = m)->next = NULL;
    }
  release_strace_mutex ();
}

/* Printf function used when tracing system calls.
   Warning: DO NOT SET ERRNO HERE! */

void
strace_printf (const char *fmt,...)
{
  DWORD err = GetLastError ();
  if (myself->strace_file && !strace_protect.val ())
    {
      strace_protect.set (1);
      int count;
      va_list ap;
      char buf[10000];
      DWORD lasterr = GetLastError ();

      PROTECT(buf);
      va_start (ap, fmt);
      count = strace_vsprintf (buf, fmt, ap);
      va_end (ap);
      CHECK(buf);

      strace_write (buf, count);
      SetLastError (lasterr);
      strace_protect.set (0);
    }
  SetLastError (err);
}

void
strace_dump (void)
{
  struct msglist *m, *n;
  DWORD done;

  get_strace_mutex ();
  if (GetFileType (myself->strace_file) == FILE_TYPE_DISK)
    SetFilePointer (myself->strace_file, 0, 0, FILE_END);
  for (m = headlist.next; m != NULL; m = n)
    {
      WriteFile (myself->strace_file, m->buf, strlen (m->buf), &done, 0);
      n = m->next;
      HeapFree (GetProcessHeap (), 0, (LPVOID) m);
    }

  headlist.next = NULL;
  endlist = &headlist;
  inqueue = 0;
  if (myself->strace_mask & _STRACE_FLUSH)
    FlushFileBuffers (myself->strace_file);
  release_strace_mutex ();
}

void mark (const char *fn, int i)
{
#if 0
register int x asm ("r2");
register int y asm ("r31");
int ox = x;
int oy = y;
alloca (100000);
#endif

  if (0 )
    {
      char b[2000];
      DWORD c;
      PROTECT(b);
      c =  __small_sprintf (b,"%s:%d %x %x %x\n", fn, i, &i);

      WriteFile (GetStdHandle (STD_ERROR_HANDLE), b, c, &c, 0);
#if 0
      c =  __small_sprintf (b,"AGAIN %s:%d %x %x %x\n", fn, i, &i, x, y);

      WriteFile (GetStdHandle (STD_ERROR_HANDLE),
		 b, c, &c, 0);
#endif
      CHECK(b);

    }
}

static const struct tab
{
  int v;
  const char *n;
}
ta[] =
{
  {  WM_NULL, "WM_NULL"  },
  {  WM_CREATE, "WM_CREATE"  },
  {  WM_DESTROY, "WM_DESTROY"  },
  {  WM_MOVE, "WM_MOVE"  },
  {  WM_SIZE, "WM_SIZE"  },
  {  WM_ACTIVATE, "WM_ACTIVATE"  },
  {  WM_SETFOCUS, "WM_SETFOCUS"  },
  {  WM_KILLFOCUS, "WM_KILLFOCUS"  },
  {  WM_ENABLE, "WM_ENABLE"  },
  {  WM_SETREDRAW, "WM_SETREDRAW"  },
  {  WM_SETTEXT, "WM_SETTEXT"  },
  {  WM_GETTEXT, "WM_GETTEXT"  },
  {  WM_GETTEXTLENGTH, "WM_GETTEXTLENGTH"  },
  {  WM_PAINT, "WM_PAINT"  },
  {  WM_CLOSE, "WM_CLOSE"  },
  {  WM_QUERYENDSESSION, "WM_QUERYENDSESSION"  },
  {  WM_QUIT, "WM_QUIT"  },
  {  WM_QUERYOPEN, "WM_QUERYOPEN"  },
  {  WM_ERASEBKGND, "WM_ERASEBKGND"  },
  {  WM_SYSCOLORCHANGE, "WM_SYSCOLORCHANGE"  },
  {  WM_ENDSESSION, "WM_ENDSESSION"  },
  {  WM_SHOWWINDOW, "WM_SHOWWINDOW"  },
  {  WM_WININICHANGE, "WM_WININICHANGE"  },
  {  WM_DEVMODECHANGE, "WM_DEVMODECHANGE"  },
  {  WM_ACTIVATEAPP, "WM_ACTIVATEAPP"  },
  {  WM_FONTCHANGE, "WM_FONTCHANGE"  },
  {  WM_TIMECHANGE, "WM_TIMECHANGE"  },
  {  WM_CANCELMODE, "WM_CANCELMODE"  },
  {  WM_SETCURSOR, "WM_SETCURSOR"  },
  {  WM_MOUSEACTIVATE, "WM_MOUSEACTIVATE"  },
  {  WM_CHILDACTIVATE, "WM_CHILDACTIVATE"  },
  {  WM_QUEUESYNC, "WM_QUEUESYNC"  },
  {  WM_GETMINMAXINFO, "WM_GETMINMAXINFO"  },
  {  WM_PAINTICON, "WM_PAINTICON"  },
  {  WM_ICONERASEBKGND, "WM_ICONERASEBKGND"  },
  {  WM_NEXTDLGCTL, "WM_NEXTDLGCTL"  },
  {  WM_SPOOLERSTATUS, "WM_SPOOLERSTATUS"  },
  {  WM_DRAWITEM, "WM_DRAWITEM"  },
  {  WM_MEASUREITEM, "WM_MEASUREITEM"  },
  {  WM_DELETEITEM, "WM_DELETEITEM"  },
  {  WM_VKEYTOITEM, "WM_VKEYTOITEM"  },
  {  WM_CHARTOITEM, "WM_CHARTOITEM"  },
  {  WM_SETFONT, "WM_SETFONT"  },
  {  WM_GETFONT, "WM_GETFONT"  },
  {  WM_SETHOTKEY, "WM_SETHOTKEY"  },
  {  WM_GETHOTKEY, "WM_GETHOTKEY"  },
  {  WM_QUERYDRAGICON, "WM_QUERYDRAGICON"  },
  {  WM_COMPAREITEM, "WM_COMPAREITEM"  },
  {  WM_COMPACTING, "WM_COMPACTING"  },
  {  WM_WINDOWPOSCHANGING, "WM_WINDOWPOSCHANGING"  },
  {  WM_WINDOWPOSCHANGED, "WM_WINDOWPOSCHANGED"  },
  {  WM_POWER, "WM_POWER"  },
  {  WM_COPYDATA, "WM_COPYDATA"  },
  {  WM_CANCELJOURNAL, "WM_CANCELJOURNAL"  },
  {  WM_NCCREATE, "WM_NCCREATE"  },
  {  WM_NCDESTROY, "WM_NCDESTROY"  },
  {  WM_NCCALCSIZE, "WM_NCCALCSIZE"  },
  {  WM_NCHITTEST, "WM_NCHITTEST"  },
  {  WM_NCPAINT, "WM_NCPAINT"  },
  {  WM_NCACTIVATE, "WM_NCACTIVATE"  },
  {  WM_GETDLGCODE, "WM_GETDLGCODE"  },
  {  WM_NCMOUSEMOVE, "WM_NCMOUSEMOVE"  },
  {  WM_NCLBUTTONDOWN, "WM_NCLBUTTONDOWN"  },
  {  WM_NCLBUTTONUP, "WM_NCLBUTTONUP"  },
  {  WM_NCLBUTTONDBLCLK, "WM_NCLBUTTONDBLCLK"  },
  {  WM_NCRBUTTONDOWN, "WM_NCRBUTTONDOWN"  },
  {  WM_NCRBUTTONUP, "WM_NCRBUTTONUP"  },
  {  WM_NCRBUTTONDBLCLK, "WM_NCRBUTTONDBLCLK"  },
  {  WM_NCMBUTTONDOWN, "WM_NCMBUTTONDOWN"  },
  {  WM_NCMBUTTONUP, "WM_NCMBUTTONUP"  },
  {  WM_NCMBUTTONDBLCLK, "WM_NCMBUTTONDBLCLK"  },
  {  WM_KEYFIRST, "WM_KEYFIRST"  },
  {  WM_KEYDOWN, "WM_KEYDOWN"  },
  {  WM_KEYUP, "WM_KEYUP"  },
  {  WM_CHAR, "WM_CHAR"  },
  {  WM_DEADCHAR, "WM_DEADCHAR"  },
  {  WM_SYSKEYDOWN, "WM_SYSKEYDOWN"  },
  {  WM_SYSKEYUP, "WM_SYSKEYUP"  },
  {  WM_SYSCHAR, "WM_SYSCHAR"  },
  {  WM_SYSDEADCHAR, "WM_SYSDEADCHAR"  },
  {  WM_KEYLAST, "WM_KEYLAST"  },
  {  WM_INITDIALOG, "WM_INITDIALOG"  },
  {  WM_COMMAND, "WM_COMMAND"  },
  {  WM_SYSCOMMAND, "WM_SYSCOMMAND"  },
  {  WM_TIMER, "WM_TIMER"  },
  {  WM_HSCROLL, "WM_HSCROLL"  },
  {  WM_VSCROLL, "WM_VSCROLL"  },
  {  WM_INITMENU, "WM_INITMENU"  },
  {  WM_INITMENUPOPUP, "WM_INITMENUPOPUP"  },
  {  WM_MENUSELECT, "WM_MENUSELECT"  },
  {  WM_MENUCHAR, "WM_MENUCHAR"  },
  {  WM_ENTERIDLE, "WM_ENTERIDLE"  },
  {  WM_CTLCOLORMSGBOX, "WM_CTLCOLORMSGBOX"  },
  {  WM_CTLCOLOREDIT, "WM_CTLCOLOREDIT"  },
  {  WM_CTLCOLORLISTBOX, "WM_CTLCOLORLISTBOX"  },
  {  WM_CTLCOLORBTN, "WM_CTLCOLORBTN"  },
  {  WM_CTLCOLORDLG, "WM_CTLCOLORDLG"  },
  {  WM_CTLCOLORSCROLLBAR, "WM_CTLCOLORSCROLLBAR"  },
  {  WM_CTLCOLORSTATIC, "WM_CTLCOLORSTATIC"  },
  {  WM_MOUSEFIRST, "WM_MOUSEFIRST"  },
  {  WM_MOUSEMOVE, "WM_MOUSEMOVE"  },
  {  WM_LBUTTONDOWN, "WM_LBUTTONDOWN"  },
  {  WM_LBUTTONUP, "WM_LBUTTONUP"  },
  {  WM_LBUTTONDBLCLK, "WM_LBUTTONDBLCLK"  },
  {  WM_RBUTTONDOWN, "WM_RBUTTONDOWN"  },
  {  WM_RBUTTONUP, "WM_RBUTTONUP"  },
  {  WM_RBUTTONDBLCLK, "WM_RBUTTONDBLCLK"  },
  {  WM_MBUTTONDOWN, "WM_MBUTTONDOWN"  },
  {  WM_MBUTTONUP, "WM_MBUTTONUP"  },
  {  WM_MBUTTONDBLCLK, "WM_MBUTTONDBLCLK"  },
  {  WM_MOUSELAST, "WM_MOUSELAST"  },
  {  WM_PARENTNOTIFY, "WM_PARENTNOTIFY"  },
  {  WM_ENTERMENULOOP, "WM_ENTERMENULOOP"  },
  {  WM_EXITMENULOOP, "WM_EXITMENULOOP"  },
  {  WM_MDICREATE, "WM_MDICREATE"  },
  {  WM_MDIDESTROY, "WM_MDIDESTROY"  },
  {  WM_MDIACTIVATE, "WM_MDIACTIVATE"  },
  {  WM_MDIRESTORE, "WM_MDIRESTORE"  },
  {  WM_MDINEXT, "WM_MDINEXT"  },
  {  WM_MDIMAXIMIZE, "WM_MDIMAXIMIZE"  },
  {  WM_MDITILE, "WM_MDITILE"  },
  {  WM_MDICASCADE, "WM_MDICASCADE"  },
  {  WM_MDIICONARRANGE, "WM_MDIICONARRANGE"  },
  {  WM_MDIGETACTIVE, "WM_MDIGETACTIVE"  },
  {  WM_MDISETMENU, "WM_MDISETMENU"  },
  {  WM_DROPFILES, "WM_DROPFILES"  },
  {  WM_MDIREFRESHMENU, "WM_MDIREFRESHMENU"  },
  {  WM_CUT, "WM_CUT"  },
  {  WM_COPY, "WM_COPY"  },
  {  WM_PASTE, "WM_PASTE"  },
  {  WM_CLEAR, "WM_CLEAR"  },
  {  WM_UNDO, "WM_UNDO"  },
  {  WM_RENDERFORMAT, "WM_RENDERFORMAT"  },
  {  WM_RENDERALLFORMATS, "WM_RENDERALLFORMATS"  },
  {  WM_DESTROYCLIPBOARD, "WM_DESTROYCLIPBOARD"  },
  {  WM_DRAWCLIPBOARD, "WM_DRAWCLIPBOARD"  },
  {  WM_PAINTCLIPBOARD, "WM_PAINTCLIPBOARD"  },
  {  WM_VSCROLLCLIPBOARD, "WM_VSCROLLCLIPBOARD"  },
  {  WM_SIZECLIPBOARD, "WM_SIZECLIPBOARD"  },
  {  WM_ASKCBFORMATNAME, "WM_ASKCBFORMATNAME"  },
  {  WM_CHANGECBCHAIN, "WM_CHANGECBCHAIN"  },
  {  WM_HSCROLLCLIPBOARD, "WM_HSCROLLCLIPBOARD"  },
  {  WM_QUERYNEWPALETTE, "WM_QUERYNEWPALETTE"  },
  {  WM_PALETTEISCHANGING, "WM_PALETTEISCHANGING"  },
  {  WM_PALETTECHANGED, "WM_PALETTECHANGED"  },
  {  WM_HOTKEY, "WM_HOTKEY"  },
  {  WM_PENWINFIRST, "WM_PENWINFIRST"  },
  {  WM_PENWINLAST, "WM_PENWINLAST"  },
  {  WM_ASYNCIO, "ASYNCIO"  },
  {  0, 0  }};

void _strace_wm (int message, int word, int lon)
{
  if (strace () & _STRACE_WM)
    {
      int i;

      for (i = 0; ta[i].n; i++)
	{
	  if (ta[i].v == message)
	    {
	      strace_printf ("wndproc %d %s %d %d", message, ta[i].n, word, lon);
	      return;
	    }
	}
      strace_printf ("wndproc %d unknown  %d %d", message, word, lon);
    }
}

/* Print a message on stderr (bypassing anything that could prevent the
   message from being printed, for example a buggy or corrupted stdio).
   This is used, for example, to print diagnostics of fatal errors.  */

void
__system_printf (const char *fmt,...)
{
  char buf[6000];
  va_list ap;
  int count;

  PROTECT (buf);
  va_start (ap, fmt);
  count = strace_vsprintf (buf, fmt, ap);
  va_end (ap);
  CHECK (buf);

#ifndef NOSTRACE
  if (!strace_stderr)
    strace_write (buf, count);
#endif

  DWORD done;
  WriteFile (GetStdHandle (STD_ERROR_HANDLE), buf, count, &done, 0);
  FlushFileBuffers (GetStdHandle (STD_ERROR_HANDLE));
}

void
small_printf (const char *fmt,...)
{
  char buf[2000];
  va_list ap;
  DWORD done;
  int count;

  PROTECT (buf);
  va_start (ap, fmt);
  count = __small_vsprintf (buf, fmt, ap);
  va_end (ap);
  CHECK (buf);

  WriteFile (GetStdHandle (STD_ERROR_HANDLE), buf, count, &done, 0);
  FlushFileBuffers (GetStdHandle (STD_ERROR_HANDLE));
}

#else

/* empty functions for when strace is disabled */

void
strace_init (const char *buf)
{}

extern "C" {
void _strace_wm (int message, int word, int lon)
{}
}
#endif /*NOSTRACE*/
