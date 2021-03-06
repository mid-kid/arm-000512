/* spawn.cc

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <process.h>
#include <sys/wait.h>
#include <errno.h>
#include "winsup.h"
#include <ctype.h>
#include "paths.h"

#define LINE_BUF_CHUNK (MAX_PATH * 2)

suffix_info std_suffixes[] =
{
  suffix_info (".exe", 1), suffix_info ("", 1),
  suffix_info (".com"), suffix_info (".cmd"),
  suffix_info (".bat"), suffix_info (".dll"),
  suffix_info (NULL)
};

/* Add .exe to PROG if not already present and see if that exists.
   If not, return PROG (converted from posix to win32 rules if necessary).
   The result is always BUF.

   Returns (possibly NULL) suffix */

static const char *
perhaps_suffix (const char *prog, char *buf)
{
  char *ext;

  debug_printf ("prog '%s'", prog);
  path_conv temp (prog, SYMLINK_FOLLOW, 1, std_suffixes);
  strcpy (buf, temp.get_win32 ());

  if (temp.file_attributes () & FILE_ATTRIBUTE_DIRECTORY)
    ext = NULL;
  else if (temp.known_suffix)
    ext = buf + (temp.known_suffix - temp.get_win32 ());
  else
    ext = strchr (buf, '\0');

  debug_printf ("buf %s, suffix found '%s'", buf, ext);
  return ext;
}

/* Find an executable name, possibly by appending known executable
   suffixes to it.  The win32-translated name is placed in 'buf'.
   Any found suffix is returned in known_suffix.

   If the file is not found and !null_if_not_found then the win32 version
   of name is placed in buf and returned.  Otherwise the contents of buf
   is undefined and NULL is returned.  */

const char *
find_exec (const char *name, char *buf, const char *mywinenv,
	   int null_if_notfound, const char **known_suffix)
{
  const char *suffix = "";
  debug_printf ("find_exec (%s)", name);
  char *retval = buf;

  /* Check to see if file can be opened as is first.
     Win32 systems always check . first, but PATH may not be set up to
     do this. */
  if ((suffix = perhaps_suffix (name, buf)) != NULL)
    goto out;

  win_env *winpath;
  const char *path;
  char tmp[MAX_PATH];

  /* Return the error condition if this is an absolute path or if there
     is no PATH to search. */
  if (strchr (name, '/') || strchr (name, '\\') ||
      isalpha (name[0]) && name[1] == ':' ||
      !(winpath = getwinenv (mywinenv)) ||
      !(path = winpath->get_native ()) ||
      *path == '\0')
    goto errout;

  debug_printf ("%s%s", mywinenv, path);

  /* Iterate over the specified path, looking for the file with and
     without executable extensions. */
  do
    {
      char *eotmp = strccpy (tmp, &path, ';');
      /* An empty path or '.' means the current directory, but we've
	 already tried that.  */
      if (tmp[0] == '\0' || (tmp[0] == '.' && tmp[1] == '\0'))
	continue;

      *eotmp++ = '\\';
      strcpy (eotmp, name);

      debug_printf ("trying %s", tmp);

      if ((suffix = perhaps_suffix (tmp, buf)) != NULL)
	goto out;
    }
  while (*path && *++path);

errout:
  /* Couldn't find anything in the given path.
     Take the appropriate action based on null_if_not_found. */
  if (null_if_notfound)
    retval = NULL;
  else
    strcpy (buf, path_conv (name).get_win32 ());

out:
  debug_printf ("%s = find_exec (%s)", buf, name);
  if (known_suffix)
    *known_suffix = suffix ?: strchr (buf, '\0');
  return retval;
}

/* Utility for spawn_guts.  */

static HANDLE
handle (int n, int direction)
{
  fhandler_base *fh = dtable[n];

  if (!fh)
    return INVALID_HANDLE_VALUE;
  if (fh->get_close_on_exec ())
    return INVALID_HANDLE_VALUE;
  if (direction == 0)
    return fh->get_input_handle ();
  return fh->get_output_handle ();
}

/* Cover function for CreateProcess.

   This function is used by both the routines that search $PATH and those
   that do not.  This should work out ok as according to the documentation,
   CreateProcess only searches $PATH if PROG has no directory elements.

   Spawning doesn't fit well with Posix's fork/exec (one can argue the merits
   of either but that's beside the point).  If we're exec'ing we want to
   record the child pid for fork.  If we're spawn'ing we don't want to do
   this.  It is up to the caller to handle both cases.

   The result is the process id.  The handle of the created process is
   stored in H.
*/

HANDLE NO_COPY hExeced;
DWORD NO_COPY exec_exit;

int
iscmd (const char *argv0, const char *what)
{
  int n;
  n = strlen (argv0) - strlen (what);
  return n >= 0 && strcasecmp (argv0 + n, what) == 0 &&
	 (n == 0 || isdirsep (argv0[n - 1]));
}

class linebuf
{
  size_t alloc;
public:
  size_t ix;
  char *buf;
  linebuf ()
  {
    ix = 0;
    alloc = 0;
    buf = NULL;
  }
  ~linebuf () {if (buf) free (buf);}
  void add (const char *what, int len);
  void add (const char *what) {add (what, strlen (what));}
  void prepend (const char *what, int len);
};

void
linebuf::add (const char *what, int len)
{
  size_t newix;
  if ((newix = ix + len) >= alloc)
    {
      alloc += LINE_BUF_CHUNK + len;
      buf = (char *) realloc (buf, alloc + 1);
    }
  memcpy (buf + ix, what, len);
  ix = newix;
  buf[ix] = '\0';
}

void
linebuf::prepend (const char *what, int len)
{
  int buflen;
  size_t newix;
  if ((newix = ix + len) >= alloc)
    {
      alloc += LINE_BUF_CHUNK + len;
      buf = (char *) realloc (buf, alloc + 1);
      buf[ix] = '\0';
    }
  if ((buflen = strlen (buf)))
      memmove (buf + len, buf, buflen + 1);
  memcpy (buf, what, len);
  ix += len;
}

int
spawn_guts (HANDLE hToken, const char * prog_arg, const char *const *argv,
	    const char *const envp[], pinfo *child, int mode)
{
  int i;
  BOOL rc;
  int argc;

  hExeced = NULL;

  if (prog_arg == NULL)
    {
      syscall_printf ("prog_arg is NULL");
      set_errno(EINVAL);
      return -1;
    }

  syscall_printf ("spawn_guts (%.132s)", prog_arg);

  if (argv == NULL)
    {
      syscall_printf ("argv is NULL");
      set_errno(EINVAL);
      return (-1);
    }

  /* CreateProcess takes one long string that is the command line (sigh).
     We need to quote any argument that has whitespace or embedded "'s.  */

  for (argc = 0; argv[argc]; argc++)
    /* nothing */;

  char *real_path;
  char real_path_buf[MAX_PATH];

  sig_protect (starting_here, 0);
  linebuf one_line;

  if (argc == 3 && argv[1][0] == '/' && argv[1][1] == 'c' &&
      (iscmd (argv[0], "command.com") || iscmd (argv[0], "cmd.exe")))
    {
      one_line.add (argv[0]);
      one_line.add (" ");
      one_line.add (argv[1]);
      one_line.add (" ");
      real_path = NULL;
      one_line.add (argv[2]);
      strcpy (real_path_buf, argv[0]);
      goto skip_arg_parsing;
    }

  real_path = real_path_buf;

  const char *saved_prog_arg;
  const char *newargv0, **firstarg;
  const char *ext;

  if ((ext = perhaps_suffix (prog_arg, real_path)) == NULL)
    {
      set_errno (ENOENT);
      return -1;
    }

  saved_prog_arg = prog_arg;
  newargv0 = argv[0];
  firstarg = &newargv0;

  /* If the file name ends in either .exe, .com, .bat, or .cmd we assume
     that it is NOT a script file */
  while (*ext == '\0')
    {
      HANDLE hnd = CreateFileA (real_path, 
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				&sec_none_nih,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				0);
      if (hnd == INVALID_HANDLE_VALUE)
	{
	  __seterrno ();
	  return -1;
	}

      DWORD done;

      char buf[2 * MAX_PATH + 1];
      buf[0] = buf[1] = buf[2] = buf[sizeof(buf) - 1] = '\0';
      if (! ReadFile (hnd, buf, sizeof (buf) - 1, &done, 0))
	{
	  CloseHandle (hnd);
	  __seterrno ();
	  return -1;
	}

      CloseHandle (hnd);

      if (buf[0] == 'M' && buf[1] == 'Z')
	break;

      debug_printf ("%s is a script", prog_arg);

      char *ptr, *pgm, *arg1;

      if (buf[0] != '#' || buf[1] != '!')
	{
	  strcpy (buf, "sh");         /* shell script without magic */
	  pgm = buf;
	  ptr = arg1 = buf + 2;
	}
      else
	{
	  pgm = buf + 2;
	  pgm += strspn (pgm, " \t");
	  for (ptr = pgm, arg1 = NULL;
	       *ptr && *ptr != '\r' && *ptr != '\n';
	       ptr++)
	    if (!arg1 && (*ptr == ' ' || *ptr == '\t'))
	      {
		arg1 = ptr;
		*arg1++ = '\0';
		ptr = (arg1 += strspn (arg1, " \t"));
	      }

	
	  *ptr = '\0';
	}

      char buf2[MAX_PATH + 1];

      /* pointers:
       * pgm	interpreter name
       * arg1	optional string
       * ptr	end of string
       */ 

      if (!arg1)
	one_line.prepend (" ", 1);
      else
	{
	  one_line.prepend ("\" ", 2);
	  one_line.prepend (arg1, strlen (arg1));
	  one_line.prepend (" \"", 2);
	}

      find_exec (pgm, real_path, "PATH=", 0, &ext);
      cygwin_conv_to_posix_path (real_path, buf2);
      one_line.prepend (buf2, strlen (buf2));

      /* If script had absolute path, add it to script name now!
       * This is necessary if script has been found via PATH.
       * For example, /usr/local/bin/tkman started as "tkman":
       * #!/usr/local/bin/wish -f
       * ...
       * We should run /usr/local/bin/wish -f /usr/local/bin/tkman,
       * but not /usr/local/bin/wish -f tkman!
       * We don't modify anything, if script has qulified path.
       */
      if (firstarg)
	*firstarg = saved_prog_arg;

      debug_printf ("prog_arg '%s', copy '%s'", prog_arg, one_line.buf);
      firstarg = NULL;
    }
  
  for (; *argv; argv++)
    {
      char *p = NULL;
      const char *a = newargv0 ?: *argv;

      newargv0 = NULL;
      int len = strlen (a);
      if (len != 0 && !(p = strpbrk (a, " \t\n\r\"")))
	one_line.add (a, len);
      else
	{
	  one_line.add ("\"", 1);
	  for (; p; a = p, p = strchr (p, '"'))
	    {
	      one_line.add (a, ++p - a);
	      if (p[-1] == '"')
		one_line.add ("\"", 1);
	    }
	  if (*a)
	    one_line.add (a);
	  one_line.add ("\"", 1);
	}
      one_line.add (" ", 1);
    }

  if (one_line.ix)
    one_line.buf[one_line.ix - 1] = '\0';
  else
    one_line.add ("", 1);

skip_arg_parsing:
  PROCESS_INFORMATION pi = {0};

  STARTUPINFO si = {0};
  si.lpReserved = NULL;
  si.lpDesktop = NULL;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = handle (0, 0); /* Get input handle */
  si.hStdOutput = handle (1, 1); /* Get output handle */
  si.hStdError = handle (2, 1); /* Get output handle */
  si.cb = sizeof (si);

  /* Pass fd table to a child */

  int len = dtable.linearize_fd_array (0, 0);
  if (len == -1)
    {
      system_printf ("FATAL error in linearize_fd_array");
      return -1;
    }
  int titlelen = 1 + (old_title && mode == _P_OVERLAY ? strlen (old_title) : 0);
  si.cbReserved2 = len + titlelen + sizeof(child_info);
  si.lpReserved2 = (LPBYTE) alloca (si.cbReserved2);

# define ciresrv ((child_info *)si.lpReserved2)
  memset (ciresrv, 0, sizeof(*ciresrv));
  ciresrv->cb = sizeof *ciresrv;
  ciresrv->cygpid = child->pid;
  ciresrv->shared_h = cygwin_shared_h;
  HANDLE spr = NULL;
  if (mode != _P_OVERLAY)
    ciresrv->type = PROC_SPAWN;
  else
    {
      spr = ciresrv->subproc_ready = CreateEvent(&sec_all, TRUE, FALSE, NULL);
      ProtectHandle (spr);
      ciresrv->type = PROC_EXEC;
    }

  LPBYTE resrv = si.lpReserved2 + sizeof *ciresrv;
# undef ciresrv

  if (dtable.linearize_fd_array (resrv, len) < 0)
    {
      system_printf ("FATAL error in second linearize_fd_array");
      return -1;
    }

  if (titlelen > 1)
    strcpy ((char *) resrv + len, old_title);
  else
    resrv[len] = '\0';

  /* We print the translated program and arguments here so the user can see
     what was done to it.  */
  syscall_printf ("spawn_guts (%s, %.132s)", real_path, one_line.buf);

  int flags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED |
	      GetPriorityClass (GetCurrentProcess ());

  if (mode == _P_DETACH)
    flags |= DETACHED_PROCESS;
  else
    {
      /* If we don't have a console, then don't create a console for
	 the child either.  */
      HANDLE console_handle = CreateFileA ("CONOUT$", GENERIC_WRITE,
					   FILE_SHARE_WRITE, &sec_none_nih,
					   OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL, NULL);
      if (console_handle != INVALID_HANDLE_VALUE
	  && console_handle != NULL)
	CloseHandle (console_handle);
      else
	flags |= DETACHED_PROCESS;
    }

  /* Build windows style environment list */
  char *envblock = winenv (envp);

  if (hToken)
    {
      /* allow the child to interact with our window station/desktop */
      HANDLE hwst, hdsk;
      SECURITY_INFORMATION dsi = DACL_SECURITY_INFORMATION;
      DWORD n;
      char wstname[1024];
      char dskname[1024];

      hwst = GetProcessWindowStation();
      SetUserObjectSecurity(hwst, &dsi, get_null_sd ());
      GetUserObjectInformation(hwst, UOI_NAME, wstname, 1024, &n);
      hdsk = GetThreadDesktop(GetCurrentThreadId());
      SetUserObjectSecurity(hdsk, &dsi, get_null_sd ());
      GetUserObjectInformation(hdsk, UOI_NAME, dskname, 1024, &n);
      strcat (wstname, "\\");
      strcat (wstname, dskname);
      si.lpDesktop = wstname;
      /* force the new process to reread /etc/passwd and /etc/group */
      myself->uid = USHRT_MAX;
      rc = CreateProcessAsUser (hToken,
		       real_path,	/* image name - with full path */
		       one_line.buf,	/* what was passed to exec */
		       &sec_all_nih,	/* process security attrs */
		       &sec_all_nih,	/* thread security attrs */
		       TRUE,	/* inherit handles from parent */
		       flags,
		       envblock,/* environment */
		       0,	/* use current drive/directory */
		       &si,
		       &pi);
    }
  else
    rc = CreateProcessA (real_path,	/* image name - with full path */
		       one_line.buf,	/* what was passed to exec */
		       &sec_all_nih,	/* process security attrs */
		       &sec_all_nih,	/* thread security attrs */
		       TRUE,	/* inherit handles from parent */
		       flags,
		       envblock,/* environment */
		       0,	/* use current drive/directory */
		       &si,
		       &pi);

  free (envblock);

  /* Set errno now so that debugging messages from it appear before our
     final debugging message [this is a general rule for debugging
     messages].  */
  if (!rc)
    __seterrno ();

  ProtectHandle (pi.hProcess);
  ProtectHandle (pi.hThread);

  /* We print the original program name here so the user can see that too.  */
  syscall_printf ("%d = spawn_guts (%s, %.132s)",
		  rc ? pi.dwProcessId : (unsigned int) -1,
		  prog_arg, one_line.buf);

  if (!rc)
    {
      if (spr)
	ForceCloseHandle (spr);
      return -1;
    }

  /* Set up child's signal handlers */
  for (i = 0; i < NSIG; i++)
    {
      child->getsig(i).sa_mask = 0;
      if (myself->getsig(i).sa_handler != SIG_IGN || (mode != _P_OVERLAY))
	child->getsig(i).sa_handler = SIG_DFL;
    }

  if (number_of_sockets)
    child->process_state |= PID_SOCKETS_USED;
  else
    myself->process_state &= ~PID_SOCKETS_USED;

  if (mode == _P_OVERLAY)
    {
      hExeced = pi.hProcess;
      close_all_files ();
      strcpy (child->progname, real_path_buf);
      proc_terminate ();
    }
  else
    {
      child->dwProcessId = pi.dwProcessId;
      child->hProcess = pi.hProcess;
      child->process_state |= PID_INITIALIZING;
      proc_register (child);
    }

  sigproc_printf ("spawned windows pid %d", pi.dwProcessId);
#ifndef NOSTRACE
  if (strace() & _STRACE_EXITDUMP)
    strace_dump ();
#endif
  /* Start the child running */
  ResumeThread (pi.hThread);
  ForceCloseHandle (pi.hThread);

  if (hToken)
    CloseHandle (hToken);

  DWORD res;

  if (mode == _P_OVERLAY)
    {
      BOOL exited;
      HANDLE waitbuf[3] = {pi.hProcess, signal_arrived, spr};
      int nwait = 3;

      ResetEvent (signal_arrived);
      SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_HIGHEST);
      res = 0;
      DWORD timeout = INFINITE;
      exec_exit = 1;
      exited = FALSE;
  waitfor:
      switch (WaitForMultipleObjects (nwait, waitbuf, FALSE, timeout))
	{
	case WAIT_TIMEOUT:
	  syscall_printf ("WFMO timed out after signal");
	  if (WaitForSingleObject (pi.hProcess, 0) != WAIT_OBJECT_0)
	    {
	      sigproc_printf ("subprocess still alive after signal");
	      res = exec_exit;
	    }
	  else
	    {
	      sigproc_printf ("subprocess exited after signal");
	case WAIT_OBJECT_0:
	      sigproc_printf ("subprocess exited");
	      if (!GetExitCodeProcess (pi.hProcess, &res))
		res = exec_exit;
	      exited = TRUE;
	     }
	  if (nwait > 2)
	    if (WaitForSingleObject (spr, 1) == WAIT_OBJECT_0)
	      res |= EXIT_REPARENTING;
	    else if (!(res & EXIT_REPARENTING))
	      {
		(void) dtable.de_linearize_fd_array (resrv);
		close_all_files ();
	      }
	  break;
	case WAIT_OBJECT_0 + 1:
	  ResetEvent (signal_arrived);
	  sigproc_printf ("signal arrived");
	  timeout = 10;
	  goto waitfor;
	case WAIT_OBJECT_0 + 2:
	  res = EXIT_REPARENTING;
	  ForceCloseHandle (spr);
	  if (!myself->parent_alive)
	    {
	      nwait = 1;
	      sigproc_terminate ();
	      goto waitfor;
	    }
	  break;
	case WAIT_FAILED:
	  DWORD r;
	  system_printf ("wait failed: nwait %d, pid %d, winpid %d, %E",
			 nwait, myself->pid, myself->dwProcessId);
	  system_printf ("waitbuf[0] %p %d", waitbuf[0],
			 GetHandleInformation (waitbuf[0], &r));
	  system_printf ("waitbuf[1] %p = %d", waitbuf[1],
			 GetHandleInformation (waitbuf[1], &r));
	  set_errno (ECHILD);
	  return -1;
	}

      if (nwait > 2)
	ForceCloseHandle (spr);

      sigproc_printf ("res = %x", res);

      if (res & EXIT_REPARENTING)
	{
	  /* Try to reparent child process.
	   * Make handles to child available to parent process and exit with
	   * EXIT_REPARENTING status. Wait() syscall in parent will then wait
	   * for newly created child.
	   */
	  lock_pinfo_for_update (INFINITE);
	  if (alive_parent (myself))
	    {
	      pinfo  *parent = procinfo (myself->ppid);
	      sigproc_printf ("parent = %p", parent);
	      HANDLE hP = OpenProcess (PROCESS_ALL_ACCESS, FALSE,
				       parent->dwProcessId);
	      sigproc_printf ("parent's handle = %d", hP);
	      res = 0;
	      if (hP)
		{
		  res = DuplicateHandle (GetCurrentProcess (), pi.hProcess,
					hP, &myself->hProcess, 0, FALSE,
					DUPLICATE_SAME_ACCESS);
		  sigproc_printf ("Dup hP %d", res);
		}
	      if (!res)
		{
		  system_printf ("Reparent failed");
		  system_printf ("my dwProcessId %d, myself->dwProcessId %d",
				 GetCurrentProcessId(), myself->dwProcessId);
		  system_printf ("myself->process_state %x",
				 myself->process_state);
		  system_printf ("myself->hProcess %x", myself->hProcess);
		}
	      unlock_pinfo ();
	      res = EXIT_REPARENTING;
	      ForceCloseHandle (hExeced);
	      hExeced = NULL;
	    }
	}
      else if (exited)
	{
	  ForceCloseHandle (hExeced);
	  hExeced = NULL; // stop do_exit from attempting to terminate subproc
	}

      do_exit (res | EXIT_NOCLOSEALL);
    }

  if (mode == _P_WAIT)
    {
      waitpid (child->pid, (int *) &res, 0);
    }
  else if (mode == _P_DETACH)
    {
      /* Lose all memory of this child. */
      res = 0;
    }
  else if ((mode == _P_NOWAIT) || (mode == _P_NOWAITO))
    {
      res = child->pid;
    }

  return (int) res;
}

extern "C"
int
cwait (int *result, int pid, int)
{
  return waitpid (pid, result, 0);
}

/*
 * Helper function for spawn runtime calls.
 * Doesn't search the path.
 */

static int
_spawnve (int mode, const char *path, const char *const *argv,
						const char *const *envp)
{
  pinfo *child;
  int ret;

  syscall_printf ("_spawnve (%s, %s, %x)", path, argv[0], envp);

  switch (mode)
    {
      case _P_OVERLAY:
	/* We do not pass _P_SEARCH_PATH here. execve doesn't search PATH.*/
	/* Just act as an exec if _P_OVERLAY set. */
	spawn_guts (NULL, path, argv, envp, myself, mode);
	/* Errno should be set by spawn_guts.  */
	ret = -1;
	break;
      case _P_NOWAIT:
      case _P_NOWAITO:
      case _P_WAIT:
      case _P_DETACH:
	lock_pinfo_for_update (INFINITE);
	child = cygwin_shared->p.allocate_pid ();
	if (!child)
	  {
	    set_errno (EAGAIN);
	    syscall_printf ("-1 = spawnve (), process table full");
	    unlock_pinfo ();
	    return -1;
	  }
	strcpy (child->progname, path);
	child->ppid = myself->pid;
	child->uid = myself->uid;
	child->gid = myself->gid;
	child->pgid = myself->pgid;
	child->sid = myself->sid;
	child->ctty = myself->ctty;
	child->umask = myself->umask;
	child->strace_mask = myself->strace_mask;
	child->strace_file = myself->strace_file;
	child->process_state |= PID_INITIALIZING;
	unlock_pinfo ();
	subproc_init ();
	ret = spawn_guts (NULL, path, argv, envp, child, mode);
	if (ret == -1)
	  child->process_state = PID_NOT_IN_USE;
	break;
      default:
	set_errno (EINVAL);
	ret = -1;
	break;
    }
  return ret;
}

/*
 * spawn functions as implemented in the MS runtime library.
 * Most of these based on (and copied from) newlib/libc/posix/execXX.c
 */

extern "C"
int
spawnl (int mode, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char *argv[256];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
      argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  va_end (args);

  return _spawnve (mode, path, (char * const  *) argv, *user_data->envptr);
}

extern "C"
int
spawnle (int mode, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char * const *envp;
  const char *argv[256];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  envp = va_arg (args, const char * const *);
  va_end (args);

  return _spawnve (mode, path, (char * const *) argv, (char * const *) envp);
}

extern "C"
int
spawnlp (int mode, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char *argv[256];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
      argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  va_end (args);

  return spawnvpe (mode, path, (char * const *) argv, *user_data->envptr);
}

extern "C"
int
spawnlpe (int mode, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char * const *envp;
  const char *argv[256];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  envp = va_arg (args, const char * const *);
  va_end (args);

  return spawnvpe (mode, path, (char * const *) argv, envp);
}

extern "C"
int
spawnv (int mode, const char *path, const char * const *argv)
{
  return _spawnve (mode, path, argv, *user_data->envptr);
}

extern "C"
int
spawnve (int mode, const char *path, char * const *argv,
					     const char * const *envp)
{
  return _spawnve (mode, path, argv, envp);
}

extern "C"
int
spawnvp (int mode, const char *path, const char * const *argv)
{
  return spawnvpe (mode, path, argv, *user_data->envptr);
}

extern "C"
int
spawnvpe (int mode, const char *file, const char * const *argv,
					     const char * const *envp)
{
  char buf[MAXNAMLEN];
  return _spawnve (mode, find_exec (file, buf), argv, envp);
}
