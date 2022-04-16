/* dcrt0.cc -- essentially the main() for the Cygwin dll

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include "winsup.h"
#include "glob.h"
#include "exceptions.h"
#include "dll_init.h"

#define MAX_AT_FILE_LEVEL 10

static per_process dummy_user_data = {0};
per_process NO_COPY *user_data = &dummy_user_data;

per_thread_waitq NO_COPY waitq_storage;
per_thread_strace_protect NO_COPY strace_protect;

per_thread NO_COPY *threadstuff[] = {&waitq_storage, &strace_protect, NULL};

BOOL display_title = FALSE;
BOOL strip_title_path = FALSE;
BOOL allow_glob = TRUE;

/* Used in SIGTOMASK for generating a bit for insertion into a sigset_t.
   This is subtracted from the signal number prior to shifting the bit.
   In older versions of cygwin, the signal was used as-is to shift the
   bit for masking.  So, we'll temporarily detect this and set it to zero
   for programs that are linked using older cygwins.  This is just a stopgap
   measure to allow an orderly transfer to the new, correct sigmask method. */
unsigned int signal_shift_subtract = 1;

extern "C"
{
  /* This is an exported copy of environ which can be used by DLLs
     which use cygwin.dll.  */
  char **__cygwin_environ;
  /* __progname used in getopt error message */
  char *__progname;
  struct _reent reent_data;
};

static void dll_crt0_1 ();

char *old_title = NULL;
char title_buf[TITLESIZE + 1];

static void
do_global_dtors (void)
{
  if (user_data->dtors)
    {
      void (**pfunc)() = user_data->dtors;
      while (*++pfunc)
	(*pfunc) ();
    }
}

void
do_global_ctors (void (**in_pfunc)(), int force)
{
  if (!force)
    {
      if (user_data->forkee || user_data->run_ctors_p)
	return;		// inherit constructed stuff from parent pid
      user_data->run_ctors_p = 1;
    }

  /* Run ctors backwards, so skip the first entry and find how many
     there are, then run them. */

  void (**pfunc)() = in_pfunc;

  while (*++pfunc)
    ;
  while (--pfunc > in_pfunc)
    (*pfunc) ();

  if (user_data)
      atexit (do_global_dtors);
}

/* remember the type of Win32 OS being run for future use. */
os_type NO_COPY os_being_run;

/* set_os_type: Set global variable os_being_run with type of Win32
   operating system being run.  This information is used internally
   to manage the inconsistency in Win32 API calls between Win32 OSes. */
/* Cygwin internal */
static void
set_os_type ()
{
  OSVERSIONINFO os_version_info;
  os_version_info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

  GetVersionEx (&os_version_info);

  switch (os_version_info.dwPlatformId)
    {
      case VER_PLATFORM_WIN32_NT:
	os_being_run = winNT;
	break;
      case VER_PLATFORM_WIN32_WINDOWS:
	if (os_version_info.dwMinorVersion == 0)
	  os_being_run = win95;
	else /* os_version_info.dwMinorVersion == 10 */
	  os_being_run = win98;
	break;
      case VER_PLATFORM_WIN32s:
	os_being_run = win32s;
	break;
      default:
	os_being_run = unknown;
	break;
    }
}

host_dependent_constants NO_COPY host_dependent;

/* Constructor for host_dependent_constants.  */

void
host_dependent_constants::init (void)
{
  /* fhandler_disk_file::lock needs a platform specific upper word
     value for locking entire files.

     fhandler_base::open requires host dependent file sharing
     attributes.  */

  switch (os_being_run)
    {
    case winNT:
      win32_upper = 0xffffffff;
      shared = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      break;

    case win98:
    case win95:
    case win32s:
      win32_upper = 0x00000000;
      shared = FILE_SHARE_READ | FILE_SHARE_WRITE;
      break;

    default:
      api_fatal ("unrecognized system type");
    }
}

/* Save the program name.  It's used in debugging messages and by
   the fork code (forking spawns a copy of us).  Copy it into a temp and
   then into the final spot because debugging messages use
   myself->progname. Try getting the absolute path from the
   module handle, if this fails get the name from the path.
   This call references $PATH so we can't do this until the environment
   vars are set up.  */
/* FIXME: What if argv[0] is relative, $PATH changes, and then the program
   tries to do a fork?  */

static void
getprogname (char *argv0)
{
  char tmp[MAX_PATH];

  if (user_data->hmodule != 0)
    {
      if (GetModuleFileName (user_data->hmodule, tmp, MAX_PATH) == 0)
	find_exec (argv0, tmp);
    }
  else
    find_exec (argv0, tmp);
  strcpy (myself->progname, tmp);
}

/*
 * Replaces -@file in the command line with the contents of the file.
 * There may be multiple -@file's in a single command line
 * A \-@file is replaced with -@file so that echo \-@foo would print
 * -@foo and not the contents of foo.
 */
static int
insert_file (char *name, char *&cmd)
{
  HANDLE f;
  DWORD size;

  f = CreateFile (name + 1,
		  GENERIC_READ,		 /* open for reading	*/
		  FILE_SHARE_READ,       /* share for reading	*/
		  &sec_none_nih,	 /* no security		*/
		  OPEN_EXISTING,	 /* existing file only	*/
		  FILE_ATTRIBUTE_NORMAL, /* normal file		*/
		  NULL);		 /* no attr. template	*/

  if (f == INVALID_HANDLE_VALUE)
    {
      debug_printf ("couldn't open file '%s', %E", name);
      return FALSE;
    }

  /* This only supports files up to about 4 billion bytes in
     size.  I am making the bold assumption that this is big
     enough for this feature */
  size = GetFileSize (f, NULL);
  if (size == 0xFFFFFFFF)
    {
      debug_printf ("couldn't get file size for '%s', %E", name);
      return FALSE;
    }

  int new_size = strlen (cmd) + size + 2;
  char *tmp = (char *) malloc (new_size);
  if (!tmp)
    {
      debug_printf ("malloc failed, %E");
      return FALSE;
    }

  /* realloc passed as it should */
  DWORD rf_read;
  BOOL rf_result;
  rf_result = ReadFile (f, tmp, size, &rf_read, NULL);
  CloseHandle (f);
  if (!rf_result || (rf_read != size))
    {
      debug_printf ("ReadFile failed, %E");
      return FALSE;
    }

  tmp[size++] = ' ';
  strcpy (tmp + size, cmd);
  cmd = tmp;
  return TRUE;
}

#define isquote(c) \
  ({ \
    char ch = c; \
    (ch == '"' || ch == '\''); \
  })

/* Step over a run of characters delimited by quotes */
static __inline char *
quoted (char *word, char *cmd, int winshell)
{
  char *p;
  char quote = *cmd;

  /* If this is being run from a Windows shell then we have
     to preserve quotes for globify to play with later. */
  if (winshell)
    {
      while (*++cmd)
	if ((p = strchr (cmd, quote)) == NULL)
	  {
	    cmd = strchr (cmd, '\0');	// no closing quote
	    break;
	  }
	else if (p[1] == quote)
	  {
	    *p++ = '\\';
	    cmd = p;			// a quoted quote
	  }
	else
	  {
	    cmd = p + 1;		// point to after end
	    break;
	  }
      return cmd;
    }

  /* When running as a child of a cygwin process, the quoted
     characters should have been placed here by spawn_guts, so
     we'll just pinch them out of the command string unless
     they're quoted with a preceding \ */
  strcpy (cmd, cmd + 1);
  while (*cmd)
    {
      if (*cmd != quote)
	cmd++;
      else if (cmd[1] == quote)
	strcpy (cmd++, cmd + 1);
      else
	{
	  strcpy (cmd, cmd + 1);
	  break;
	}
    }
  return cmd;
}

/* Perform a glob on word if it contains wildcard characters.
   Also quote every character between quotes to force glob to
   treat the characters literally. */
static int
globify (char *word, char **&argv, int &argc, int &argvlen)
{
  if (strpbrk (word, "?*[\"\'~(){}~") == NULL)
    return 0;

  int n = 0;
  char *p, *s;

  /* We'll need more space if there are quoting characters in
     word.  If that is the case, doubling the size of the
     string should provide more than enough space. */
  if (strpbrk (word, "'\""))
    n = strlen (word);
  char pattern[strlen (word) + n + 1];

  /* Fill pattern with characters from word, quoting any
     characters found within quotes. */
  for (p = pattern, s = word; *s != '\000'; s++, p++)
    if (!isquote (*s))
      *p = *s;
    else
      {
	char quote = *s;
	while (*++s && *s != quote)
	  {
	    if (*s == '\\' && s[1] == quote)
	      s++;
	    *p++ = '\\';
	    *p++ = *s;
	  }
	if (*s == quote)
	  p--;
	if (*s == '\0')
	    break;
      }

  *p = '\0';

  glob_t gl;
  gl.gl_offs = 0;

  /* Attempt to match the argument.  Return just word (minus quoting) if no match. */
  if (glob (pattern, GLOB_TILDE | GLOB_NOCHECK | GLOB_BRACE | GLOB_QUOTE, NULL, &gl) || !gl.gl_pathc)
    return 0;

  /* Allocate enough space in argv for the matched filenames. */
  n = argc;
  if ((argc += gl.gl_pathc) > argvlen)
    {
      argvlen = argc + 10;
      argv = (char **) realloc (argv, (1 + argvlen) * sizeof (argv[0]));
    }

  /* Copy the matched filenames to argv. */
  char **gv = gl.gl_pathv;
  char **av = argv + n;
  while (*gv)
    {
      debug_printf ("argv[%d] = '%s'\n", n++, *gv);
      *av++ = *gv++;
    }

  /* Clean up after glob. */
  free (gl.gl_pathv);
  return 1;
}

/* Build argv, argc from string passed from Windows.  */

static void
build_argv (char *cmd, char **&argv, int &argc, int winshell)
{
  int argvlen = 0;
  char *alloc_cmd = NULL;	// command allocated by insert_file
  int nesting = 0;		// monitor "nesting" from insert_file

  argc = 0;
  argvlen = 0;
  argv = NULL;

  /* Scan command line until there is nothing left. */
  while (*cmd)
    {
      /* Ignore spaces */
      if (issep (*cmd))
	{
	  cmd++;
	  continue;
	}

      /* Found the beginning of an argument. */
      char *word = cmd;
      char *sawquote = NULL;
      while (*cmd)
	{
	  if (*cmd != '"' && (!winshell || *cmd != '\''))
	    cmd++;		// Skip over this character
	  else
	    /* Skip over characters until the closing quote */
	    {
	      sawquote = cmd;
	      cmd = quoted (word, cmd, winshell);
	    }
	  if (issep (*cmd))	// End of argument if space
	    break;
	}
      if (*cmd)
	*cmd++ = '\0';		// Terminate `word'

      /* Possibly look for @file construction assuming that this isn't
	 the very first argument and the @ wasn't quoted */
      if (argc && sawquote != word && *word == '@')
	{
	  if (++nesting > MAX_AT_FILE_LEVEL)
	    api_fatal ("Too many levels of nesting for %s", word);
	  if (insert_file (word, cmd))
	    {
	      if (alloc_cmd)
		free (alloc_cmd);	// Free space from previous insert_file
	      alloc_cmd = cmd;		//  and remember it for next time.
	      continue;			// There's new stuff in cmd now
	    }
	}

      /* See if we need to allocate more space for argv */
      if (argc >= argvlen)
	{
	  argvlen = argc + 10;
	  argv = (char **) realloc (argv, (1 + argvlen) * sizeof (argv[0]));
	}

      /* Add word to argv file after (optional) wildcard expansion. */
      if (!winshell || !argc || !globify (word, argv, argc, argvlen))
	{
	  debug_printf ("argv[%d] = '%s'\n", argc, word);
	  argv[argc++] = word;
	}
    }

  argv[argc] = NULL;
  debug_printf ("argv[%d] = '%s'\n", argc, argv[argc]);
}

/* sanity and sync check */
void
check_sanity_and_sync (per_process *p)
{
  /* Sanity check to make sure developers didn't change the per_process    */
  /* struct without updating SIZEOF_PER_PROCESS [it makes them think twice */
  /* about changing it].						   */
  if (sizeof (per_process) != SIZEOF_PER_PROCESS)
    {
      api_fatal ("per_process sanity check failed");
    }

  /* Make sure that the app and the dll are in sync. */

  /* Complain if older than last incompatible change */
  if (p->dll_major < CYGWIN_VERSION_DLL_EPOCH)
    api_fatal ("cygwin DLL and APP are out of sync -- DLL version mismatch %d < %d",
	       p->dll_major, CYGWIN_VERSION_DLL_EPOCH);

  /* magic_biscuit != 0 if using the old style version numbering scheme.  */
  if (p->magic_biscuit != SIZEOF_PER_PROCESS)
    api_fatal ("Incompatible cygwin .dll -- incompatible per_process info %d != %d",
	       p->magic_biscuit, SIZEOF_PER_PROCESS);

  /* Complain if incompatible API changes made */
  if (p->api_major != cygwin_version.api_major)
    api_fatal ("cygwin DLL and APP are out of sync -- API version mismatch %d < %d",
	       p->api_major, cygwin_version.api_major);

  if (CYGWIN_VERSION_DLL_MAKE_COMBINED (p->dll_major, p->dll_minor) <=
      CYGWIN_VERSION_DLL_BAD_SIGNAL_MASK)
    signal_shift_subtract = 0;
}

static NO_COPY STARTUPINFO si;
# define ciresrv ((struct child_info_fork *)(si.lpReserved2))
child_info_fork NO_COPY *child_proc_info = NULL;
static MEMORY_BASIC_INFORMATION sm;

#define EBP	6
#define ESP	7

extern void __inline__
alloc_stack_hard_way (child_info_fork *ci)
{
  void *new_stack_pointer;
  MEMORY_BASIC_INFORMATION m;

  if (!VirtualAlloc (ci->stacktop,
		     (DWORD) ci->stackbottom - (DWORD) ci->stacktop,
		     MEM_RESERVE, PAGE_NOACCESS))
    api_fatal ("fork: can't reserve memory for stack %p - %p, %E",
		ci->stacktop, ci->stackbottom);

  new_stack_pointer = (void *) ((LPBYTE) ci->stackbottom - ci->stacksize);

  if (!VirtualAlloc (new_stack_pointer, ci->stacksize, MEM_COMMIT,
		     PAGE_EXECUTE_READWRITE))
    api_fatal ("fork: can't commit memory for stack %p(%d), %E",
	       new_stack_pointer, ci->stacksize);
  if (!VirtualQuery ((LPCVOID) new_stack_pointer, &m, sizeof m))
    api_fatal ("fork: couldn't get new stack info, %E");
  m.BaseAddress = (LPVOID)((DWORD)m.BaseAddress - 1);
  if (!VirtualAlloc ((LPVOID) m.BaseAddress, 1, MEM_COMMIT,
		     PAGE_EXECUTE_READWRITE|PAGE_GUARD))
    api_fatal ("fork: couldn't allocate new stack guard page %p, %E",
	       m.BaseAddress);
  if (!VirtualQuery ((LPCVOID) m.BaseAddress, &m, sizeof m))
    api_fatal ("fork: couldn't get new stack info, %E");
  ci->stacktop = m.BaseAddress;
}

/* extend the stack prior to fork longjmp */

extern void __inline__
alloc_stack (child_info_fork *ci)
{
  /* FIXME: adding 16384 seems to avoid a stack copy problem during
     fork on Win95, but I don't know exactly why yet. DJ */
  volatile char b[ci->stacksize + 16384];

  if (ci->type == PROC_FORK)
    ci->stacksize = 0;		// flag to fork not to do any funny business
  else
    {
      if (!VirtualQuery ((LPCVOID) &b, &sm, sizeof sm))
	api_fatal ("fork: couldn't get stack info, %E");

      if (sm.AllocationBase != ci->stacktop)
	alloc_stack_hard_way (ci);
      else
	ci->stacksize = 0;
    }

  return;
}

/* These must be static due to the way we have to deal with forked
   processes. */
static NO_COPY LPBYTE info = NULL;
static NO_COPY int mypid = 0;
static int argc = 0;
static char **argv = NULL;

#ifdef _MT_SAFE
  ResourceLocks _reslock NO_COPY;
  MTinterface _mtinterf NO_COPY;
#endif

/* Take over from libc's crt0.o and start the application.  */
static void
dll_crt0_1 ()
{
  /* According to onno@stack.urc.tue.nl, the exception handler record must
     be on the stack.  */
  /* FIXME: Verify forked children get their exception handler set up ok. */
  exception_list cygwin_except_entry;

  do_global_ctors (&__CTOR_LIST__, TRUE);

  regthread ("main", GetCurrentThreadId ());

  check_sanity_and_sync (user_data);

  /* Nasty static stuff needed by newlib -- point to a local copy of
     the reent stuff.
     Note: this MUST be done here (before the forkee code) as the
     fork copy code doesn't copy the data in libccrt0.cc (that's why we
     pass in the per_process struct into the .dll from libccrt0). */

  *(user_data->impure_ptr_ptr) = &reent_data;
  _impure_ptr = &reent_data;

#ifdef _MT_SAFE
  user_data->resourcelocks = &_reslock;
  user_data->resourcelocks->Init();

  user_data->threadinterface = &_mtinterf;
  user_data->threadinterface->Init0();
#endif

  /* Set the os_being_run global. */
  set_os_type ();

  /* If we didn't call SetFileApisToOEM, console I/O calls would use a
     different codepage than other Win32 API calls.  In some languages
     (not English), this would result in "cat > filename" creating a file
     by a different name than if CreateFile was used to create filename.
     SetFileApisToOEM prevents this problem by making all calls use the
     OEM codepage. */

  SetFileApisToOEM ();

  /* Initialize the host dependent constants object. */
  host_dependent.init ();

  /* Initialize the cygwin subsystem if this is the first process,
     or attach to the shared data structure if it's already running. */
  shared_init ();

  if (mypid)
    myself = cygwin_shared->p[mypid];

  /* Initialize events. */
  events_init ();

  (void) SetErrorMode (SEM_FAILCRITICALERRORS);

  /* Initialize the heap. */
  heap_init ();

  /* Initialize SIGSEGV handling, etc...  Because the exception handler
     references data in the shared area, this must be done after
     shared_init. */
  init_exceptions (&cygwin_except_entry);

  if (user_data->forkee)
    {
      /* If we've played with the stack, stacksize != 0.  That means that
	 fork() was invoked from other than the main thread.  Make sure that
	 frame pointer is referencing the new stack so that the OS knows what
	 to do when it needs to increase the size of the stack.

	 NOTE: Don't do anything that involves the stack until you've completed
	 this step. */
      if (ciresrv->stacksize)
	{
	  asm ("movl %0,%%fs:4" : : "r" (ciresrv->stackbottom));
	  asm ("movl %0,%%fs:8" : : "r" (ciresrv->stacktop));
	}

      longjmp (ciresrv->jmp, ciresrv->cygpid);
    }

  pinfo_init (info);		/* Initialize our process table entry. */

  if (!old_title && GetConsoleTitle (title_buf, TITLESIZE))
      old_title = title_buf;

  /* Nasty static stuff needed by newlib - initialize it.
     Note that impure_ptr has already been set up to point to this above
     NB. This *MUST* be done here, just after the forkee code as some
     of the calls below (eg. uinfo_init) do stdio calls - this area must
     be set to zero before then. */

#ifdef _MT_SAFE
  user_data->threadinterface->ClearReent();
  user_data->threadinterface->Init1();
#else
  memset (&reent_data, 0, sizeof (reent_data));
  reent_data._errno = 0;
  reent_data._stdin =  reent_data.__sf + 0;
  reent_data._stdout = reent_data.__sf + 1;
  reent_data._stderr = reent_data.__sf + 2;
#endif

  char *line = GetCommandLineA ();
  CharToOem (line, line);

  line = strcpy ((char *) alloca (strlen (line) + 1), line);

  /* Set new console title if appropriate. */

  if (display_title)
    {
      char *cp = line;
      if (strip_title_path)
	for (char *ptr = cp; *ptr && *ptr != ' '; ptr++)
	  if (isdirsep (*ptr))
	    cp = ptr + 1;
      set_console_title (cp);
    }

  /* Allocate dtable */
  dtable_init ();

  /* Initialize signal/subprocess handling. */
  sigproc_init ();

  /* Connect to tty. */
  tty_init ();

  /* Set up standard fds in file descriptor table. */
  hinfo_init ();

  /* Initialize uid, gid. */
  uinfo_init ();

  /* Initialize Winsock if parent process used sockets since we
     might be inheriting socket fds as a result of a fork or exec. */
  if (ISSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  syscall_printf ("Application CYGWIN version: %d.%d, api: %d.%d",
		  user_data->dll_major, user_data->dll_minor,
		  user_data->api_major, user_data->api_minor);
  syscall_printf ("CYGWIN DLL version: %d.%d, api: %d.%d",
		  cygwin_version.dll_major, cygwin_version.dll_minor,
		  cygwin_version.api_major, cygwin_version.api_minor);

  /* Scan the command line and build argv.  Expand wildcards if not called from
     another cygwin process. */
  build_argv (line, argv, argc, NOTSTATE (myself, PID_CYGPARENT) && allow_glob);

  /* Convert argv[0] to posix rules if it's currently blatantly
     win32 style. */
  if ((strchr (argv[0], ':')) || (strchr (argv[0], '\\')))
    {
      char *new_argv0 = (char *) alloca (MAX_PATH);
      cygwin_conv_to_posix_path (argv[0], new_argv0);
      argv[0] = new_argv0;
    }

  getprogname (argv[0]);
  /* Set up __progname for getopt error call. */
  __progname = argv[0];

  /* Call init of loaded dlls. */
  DllList::the().initAll();

  set_errno (0);
  debug_printf ("user_data->main %p", user_data->main);
  if (user_data->main)
    exit (user_data->main (argc, argv, *user_data->envptr));
}

/* Wrap the real one, otherwise gdb gets confused about
   two symbols with the same name, but different addresses.

   UPTR is a pointer to global data that lives on the libc side of the
   line [if one distinguishes the application from the dll].  */

void
dll_crt0 (per_process *uptr)
{
  /* Set the local copy of the pointer into the user space. */
  user_data = uptr;

  GetStartupInfo (&si);
  if (si.cbReserved2 >= EXEC_MAGIC_SIZE && !ciresrv->zero)
    switch (ciresrv->type)
      {
	case PROC_EXEC:
	case PROC_SPAWN:
	  info = si.lpReserved2 + ciresrv->cb;
	  mypid = ciresrv->cygpid;
	  cygwin_shared_h = ciresrv->shared_h;
	  child_proc_info = ciresrv;
	  break;
	case PROC_FORK:
	case PROC_FORK1:
	  user_data->forkee = ciresrv->cygpid;
	  user_data->size = ciresrv->heapsize;
	  user_data->base = ciresrv->heapbase;
	  user_data->ptr = ciresrv->heapptr;
	  cygwin_shared_h = ciresrv->shared_h;
	  mypid = ciresrv->cygpid;
	  child_proc_info = ciresrv;
	  alloc_stack (ciresrv);		// may never return
	  break;
      }
  dll_crt0_1 ();
}

extern "C" void
__main (void)
{
  do_global_ctors (user_data->ctors, FALSE);
}

void
do_exit (int status)
{
  BOOL cleanup_pinfo;
  UINT n = (UINT) status;

  syscall_printf ("do_exit (%d)", n);

  if (!(n & EXIT_REPARENTING))
    {
      signal (SIGCHLD, SIG_IGN);
      signal (SIGHUP, SIG_IGN);
      signal (SIGINT, SIG_IGN);
      signal (SIGQUIT, SIG_IGN);
    }

  if (n & EXIT_NOCLOSEALL)
    n &= ~EXIT_NOCLOSEALL;
  else
    close_all_files ();

  sigproc_terminate ();

  if (n & EXIT_REPARENTING)
    {
      n &= ~EXIT_REPARENTING;
      cleanup_pinfo = FALSE;
    }
  else
    {
      myself->stopsig = 0;
      cleanup_pinfo = TRUE;

      /* restore console title */
      if (old_title && display_title)
	set_console_title (old_title);

      /* Kill orphaned children on group leader exit */
      if (myself->pid == myself->pgid)
	{
	  sigproc_printf ("%d == pgrp %d, send SIG{HUP,CONT} to stopped children",
			  myself->pid, myself->pgid);
	  kill_pgrp (myself->pgid, -SIGHUP);
	}

      /* Kill the foreground process group on session leader exit */
      if (getpgrp () > 0 && myself->pid == myself->sid && myself->ctty != -1)
	{
	  tty *ttyp = cygwin_shared->tty[myself->ctty];
	  sigproc_printf ("%d == sid %d, send SIGHUP to children",
			  myself->pid, myself->sid);

	  if (ttyp->getsid () == myself->sid)
	    kill (-ttyp->getpgid (), SIGHUP);
	}
      tty_terminate ();
    }

  window_terminate ();
  fill_rusage (&myself->rusage_self, GetCurrentProcess ());

  shared_terminate ();
  events_terminate ();		// Does not close pinfo_mutex

  if (hExeced)
    {
      debug_printf ("Killing(%d) non-cygwin process, handle %p", n, hExeced);
      TerminateProcess (hExeced, n);
      ForceCloseHandle (hExeced);
    }

#ifndef NOSTRACE
  if (strace() & _STRACE_EXITDUMP)
    {
      sigproc_printf ("calling ExitProcess %d", n);
      strace_dump ();
    }
#endif
  if (cleanup_pinfo)
    myself->record_death ();	// Locks pinfo mutex
  else
    sigproc_printf ("not cleanup_pinfo");
  sigproc_printf ("calling ExitProcess %d", n);
  minimal_printf ("winpid %d, exit %d", GetCurrentProcessId (), n);
  ExitProcess (n & ~EXIT_SIGNAL); // Terminate process and release pinfo mutex
}

void _exit (int n)
{
  do_exit ((DWORD) n & 0xffff);
}

void
__api_fatal (const char *fmt, ...)
{
  char buf[4096];
  va_list ap;

  va_start (ap, fmt);
  __small_vsprintf (buf + 8, fmt, ap);
  va_end (ap);
  small_printf ("%s\n", buf);

  /* We are going down without mercy.  Make sure we reset
     our process_state. */
  if (user_data != NULL)
    {
      sigproc_terminate ();
      myself->record_death_nolock ();
    }
  ExitProcess (1);
}
