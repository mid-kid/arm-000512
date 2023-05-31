/* winsup.h: main Cygwin header file.

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#define __INSIDE_CYGWIN__

#define strlen __builtin_strlen
#define strcpy __builtin_strcpy
#define memcpy __builtin_memcpy
#define memcmp __builtin_memcmp
// #define memset __builtin_memset

#include <sys/types.h>
#include <sys/strace.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

#include <windows.h>

/* Used for runtime OS check/decisions. */
enum os_type {winNT = 1, win95, win98, win32s, unknown};
extern os_type os_being_run;

#include <cygwin/version.h>

#define TITLESIZE 1024
#define MAX_USER_NAME 20

#include "thread.h"
#include "shared.h"
#include "fhandler.h"
#include "path.h"
#include <sys/cygwin.h>
#include "debug.h"

class hinfo;

extern HANDLE hMainThread;

/* Now that pinfo has been defined, include... */
#include "sigproc.h"

/********************** Application Interface **************************/

/* This lives in the app and is initialized before jumping into the DLL.
   It should only contain stuff which the user's process needs to see, or
   which is needed before the user pointer is initialized, or is needed to
   carry inheritance information from parent to child.  Note that it cannot
   be used to carry inheritance information across exec!

   Remember, this structure is linked into the application's executable.
   Changes to this can invalidate existing executables, so we go to extra
   lengths to avoid having to do it.

   When adding/deleting members, remember to adjust {public,internal}_reserved.
   The size of the class shouldn't change [unless you really are prepared to
   invalidate all existing executables].  The program does a check (using
   SIZEOF_PER_PROCESS) to make sure you remember to make the adjustment.
*/

class per_process
{
 public:
  char *initial_sp;

  /* The offset of these 3 values can never change. */
  /* magic_biscuit is the size of this class and should never change. */
  DWORD magic_biscuit;
  DWORD dll_major;
  DWORD dll_minor;

  struct _reent **impure_ptr_ptr;
  char ***envptr;

  /* Used to point to the memory machine we should use.  Usually these
     point back into the dll, but they can be overridden by the user. */
  void *(*malloc)(size_t);
  void (*free)(void *);
  void *(*realloc)(void *, size_t);

  int *fmode_ptr;

  int (*main)(int, char **, char **);
  void (**ctors)(void);
  void (**dtors)(void);

  /* For fork */
  void *data_start;
  void *data_end;
  void *bss_start;
  void *bss_end;

  void *(*calloc)(size_t, size_t);
  /* For future expansion of values set by the app. */
  void *public_reserved[4];

  /* The rest are *internal* to cygwin.dll.
     Those that are here because we want the child to inherit the value from
     the parent (which happens when bss is copied) are marked as such. */

  /* non-zero of ctors have been run.  Inherited from parent. */
  int run_ctors_p;

  /* These will be non-zero if the above (malloc,free,realloc) have been
     overridden. */
  /* FIXME: not currently used */
  int __imp_malloc;
  int __imp_free;
  int __imp_realloc;

  /* Heap management.  Inherited from parent. */
  void *base;			/* bottom of the heap */
  void *ptr;			/* current index into heap */
  size_t size;			/* current size of heap */

  HANDLE reserved1;		/* unused */

  /* Non-zero means the task was forked.  The value is the pid.
     Inherited from parent. */
  int forkee;

  void *hmodule;

  DWORD api_major;		/* API version that this program was */
  DWORD api_minor;		/*  linked with */
  /* For future expansion, so apps won't have to be relinked if we
     add an item. */
#ifdef _MT_SAFE
  ResourceLocks *resourcelocks;
  MTinterface *threadinterface;
  void *internal_reserved[6];
#else
  void *internal_reserved[8];
#endif
};

extern per_process *user_data; /* Pointer into application's static data */

/* We use the following to test that sizeof hasn't changed.  When adding
   or deleting members, insert fillers or use the reserved entries.
   Do not change this value. */
#define SIZEOF_PER_PROCESS (42 * 4)

class hinfo
{
  fhandler_base **fds;
  int first_fd_for_open;
public:
  size_t size;
  void clearout (int fd);
  hinfo () {first_fd_for_open = 3;}
  int extend (int howmuch);
  void fixup_after_fork ();
  fhandler_base *build_fhandler (int fd, DWORD dev, const char *name,
				 int unit = -1);
  fhandler_base *build_fhandler (int fd, const char *name, HANDLE h);
  int not_open (int n);
  int find_unused_handle (int start);
  int find_unused_handle () { return find_unused_handle (first_fd_for_open);}
  void release (int fd);
  void init_std_file_from_handle (int fd, HANDLE handle, DWORD access, const char *name);
  int dup2 (int oldfd, int newfd);
  int linearize_fd_array (unsigned char *buf, int buflen);
  LPBYTE de_linearize_fd_array (LPBYTE buf);
  fhandler_base *operator [](int fd) { return fds[fd]; }
  select_record *select_read (int fd, select_record *s);
  select_record *select_write (int fd, select_record *s);
  select_record *select_except (int fd, select_record *s);
};

/******************* Host-dependent constants **********************/
/* Portions of the cygwin DLL require special constants whose values
   are dependent on the host system.  Rather than dynamically
   determine those values whenever they are required, initialize these
   values once at process start-up. */

class host_dependent_constants
{
 public:
  void init (void);

  /* Used by fhandler_disk_file::lock which needs a platform-specific
     upper word value for locking entire files. */
  DWORD win32_upper;

  /* fhandler_base::open requires host dependent file sharing
     attributes. */
  int shared;
};

extern host_dependent_constants host_dependent;

/* Events/mutexes */
extern HANDLE pinfo_mutex;
extern HANDLE title_mutex;



/*************************** Per Thread ******************************/

class per_thread
{
  DWORD tls;
  int clear_on_fork_p;
public:
  per_thread (int forkval = 1) {tls = TlsAlloc (); clear_on_fork_p = forkval;}
  DWORD get_tls () {return tls;}
  int clear_on_fork () {return clear_on_fork_p;}
  
  virtual void *get () {return TlsGetValue (get_tls ());}
  virtual size_t size () {return 0;}
  virtual void set (void *s = NULL) {TlsSetValue (get_tls (), s);}
  virtual void *create () {
    void *s = new char [size ()];
    set (s);
    return s;
  }
};

class per_thread_waitq : public per_thread
{
public:
  per_thread_waitq() : per_thread (0) {}
  void *get () {return (waitq *) this->per_thread::get ();}
  void *create () {return (waitq *) this->per_thread::create ();}
  size_t size () {return sizeof (waitq);}
};

class per_thread_strace_protect : public per_thread
{
public:
  per_thread_strace_protect() : per_thread (1) {}
  int val () {
    return (int) this->per_thread::get ();
  }
  void set (int n) {this->per_thread::set ((void *) n);}
  void *create () {return NULL;}
  size_t size () {return sizeof (int);}
};

extern per_thread_waitq waitq_storage;
extern per_thread_strace_protect strace_protect;

extern per_thread *threadstuff[];

/**************************** Convenience ******************************/

#define NO_COPY __attribute__((section(".data_cygwin_nocopy")))

/* Used when treating / and \ as equivalent. */
#define SLASH_P(ch) \
    ({ \
	char __c = (ch); \
	((__c) == '/' || (__c) == '\\'); \
    })

/* Convert a signal to a signal mask */
#define SIGTOMASK(sig)	(1<<((sig) - signal_shift_subtract))
extern unsigned int signal_shift_subtract;

#ifdef NOSTRACE
#define MARK() 0
#else
#define MARK() mark (__FILE__,__LINE__)
#endif

#if DEBUG_NEST_ON!=0
#define in(x) { debug_printf ("mark in %s:%d %s\n",__FILE__,__LINE__, x); d++;}
#define out(x) { d--; debug_printf ("mark out %s:%d %s\n",__FILE__,__LINE__,x);}
#else
#define in(x)
#define out(x)
#endif

#define api_fatal(fmt, args...) \
    ({\
      __api_fatal ("%P: *** " fmt, ## args); \
    })

#undef issep
#define issep(ch) (strchr (" \t\n\r", (ch)) != NULL)

#define isdirsep SLASH_P
#define isabspath(p) \
  (isdirsep (*(p)) || (isalpha (*(p)) && (p)[1] == ':'))

#define alloca(x) __builtin_alloca (x)
#define strlen __builtin_strlen
#define strcpy __builtin_strcpy
#define memcpy __builtin_memcpy
#define memcmp __builtin_memcmp
// #define memset __builtin_memset

/**************************** Initialization/Termination ******************************/

/* cygwin .dll initialization */
void dll_crt0 (per_process *);

/* dynamically loaded dll initialization */
extern "C" int dll_dllcrt0 (HMODULE,per_process*);

/* dynamically loaded dll initialization for non-cygwin apps */
extern "C" int dll_noncygwin_dllcrt0 (HMODULE, per_process *);

/* exit the program */
extern "C" void	do_exit (int) __attribute__ ((noreturn));

/* Initialize the environment */
void environ_init (void);

/* Heap management. */
void heap_init (void);
void malloc_init (void);

/* fd table */
void dtable_init (void);
void hinfo_init (void);
extern hinfo dtable;

/* UID/GID */
void uinfo_init (void);

/* various events */
void events_init (void);
void events_terminate (void);

void close_all_files (void);

/* Strace facility.  See strace.cc and sys/strace.h. */
void strace_init (const char *);

/* Invisible window initialization/termination. */
HANDLE gethwnd (void);
void window_terminate (void);

/* Globals that handle initialization of winsock in a child process. */
void winsock_init (void);
extern int number_of_sockets;

/**************************** Miscellaneous ******************************/

const char * find_exec (const char *name, char *buf, const char *winenv = "PATH=",
			int null_if_notfound = 0, const char **known_suffix = NULL);

/* File manipulation */
BOOL get_file_attribute (const char *, int *);
BOOL set_file_attribute (const char *, int);
void set_std_handle (int);
int writable_directory (const char *file);
int stat_dev (DWORD, int, unsigned long, struct stat *);

unsigned long hash_path_name (unsigned long hash, const char *name);

void mark (const char *, int);

int spawn_guts (HANDLE hToken, const char *prog_arg,
		   const char *const *argv, const char *const envp[],
		   pinfo *child, int mode);

/* For mmaps across fork(). */
int recreate_mmaps_after_fork (void *);
void set_child_mmap_ptr (pinfo *);

/* String manipulation */
char *strccpy (char *s1, const char **s2, char c);
int strcasematch (const char *s1, const char *s2);
int strncasematch (const char *s1, const char *s2, size_t n);
char *strcasestr (const char *searchee, const char *lookfor);

/* Printf type functions */
void __api_fatal (const char *, ...) __attribute__ ((noreturn));
extern "C" int __small_sprintf (char *dst, const char *fmt, ...);
extern "C" int __small_vsprintf (char *dst, const char *fmt, va_list ap);

/* Time related */
long totime_t (FILETIME * ptr);
void totimeval (struct timeval *dst, FILETIME * src, int sub, int flag);
long to_time_t (FILETIME * ptr);

/* pinfo table manipulation */
#ifndef lock_pinfo_for_update
int lock_pinfo_for_update (DWORD timeout);
#endif
void unlock_pinfo (void);

/* Retrieve a security descriptor that allows all access */
SECURITY_DESCRIPTOR *get_null_sd (void);

int get_id_from_sid (PSID);

int NTReadEA (const char *file, const char *attrname, char *buf, int len);
BOOL NTWriteEA (const char *file, const char *attrname, char *buf, int len);

void set_console_title (char *);

void fill_rusage (struct rusage *, HANDLE);
void add_rusage (struct rusage *, struct rusage *);

/**************************** Exports ******************************/

extern "C" {
int cygwin_select (int , fd_set *, fd_set *, fd_set *,
		     struct timeval *to);
int kill_pgrp (pid_t, int);
int _kill (int, int);
int _raise (int sig);

int getdtablesize ();
void setdtablesize (int);

extern char _data_start__, _data_end__, _bss_start__, _bss_end__;
extern void (*__CTOR_LIST__) (void);
extern void (*__DTOR_LIST__) (void);
};

/*************************** Unsorted ******************************/

/* The size of the console title */
#define TITLESIZE 1024

#define WM_ASYNCIO	0x8000		// WM_APP

/* Note that MAX_PATH is defined in the windows headers */
/* There is also PATH_MAX and MAXPATHLEN.
   PATH_MAX is from Posix and does *not* include the trailing NUL.
   MAXPATHLEN is from Unix.

   Thou shalt use MAX_PATH throughout.  It avoids the NUL vs no-NUL
   issue and is neither of the Unixy ones [so we can punt on which
   one is the right one to use].  */

/* Initial and increment values for cygwin's fd table */
#define NOFILE_INCR    32

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/reent.h>

#define STD_RBITS S_IRUSR | S_IRGRP | S_IROTH
#define STD_WBITS S_IWUSR
#define STD_XBITS S_IXUSR | S_IXGRP | S_IXOTH

#define O_NOSYMLINK 0x080000
#define O_DIROPEN   0x100000

#ifdef __cplusplus
}
#endif

/*************************** Environment ******************************/

/* The structure below is used to control conversion to/from posix-style
 * file specs.  Currently, only PATH and HOME are converted, but PATH
 * needs to use a "convert path list" function while HOME needs a simple
 * "convert to posix/win32".  For the simple case, where a calculated length
 * is required, just return MAX_PATH.  *FIXME*
 */
struct win_env
  {
    const char *name;
    size_t namelen;
    char *posix;
    char *native;
    int (*toposix) (const char *, char *);
    int (*towin32) (const char *, char *);
    int (*posix_len) (const char *);
    int (*win32_len) (const char *);
    void add_cache (const char *in_posix, const char *in_native = NULL);
    const char * get_native () {return native ? native + namelen : NULL;}
  };

win_env *getwinenv (const char *name, const char *posix = NULL);

char *winenv (const char * const *);
extern char **__cygwin_environ;

/* The title on program start. */
extern char *old_title;
extern BOOL display_title;

/*************************** errno manipulation ******************************/

void seterrno_from_win_error (const char *file, int line, int code);
void seterrno (const char *, int line);

#define __seterrno() seterrno (__FILE__, __LINE__)
#define __seterrno_from_win_error(val) seterrno_from_win_error (__FILE__, __LINE__, val)
#undef errno
#define errno dont_use_this_since_were_in_a_shared library
#define set_errno(val) (_impure_ptr->_errno = (val))
#define get_errno()  (_impure_ptr->_errno)

class save_errno
  {
    int saved;
  public:
    save_errno () {saved = get_errno ();}
    save_errno (int what) {saved = get_errno (); set_errno (what); }
    void set (int what) {set_errno (what); saved = what;}
    void reset () {saved = get_errno ();}
    ~save_errno () {set_errno (saved);}
  };

extern const char *__sp_fn;
extern int __sp_ln;
#define sig_protect(what, val) \
  __sp_fn = __FILE__; __sp_ln = __LINE__; __sig_protect what (val, __sp_fn, __sp_ln)

class __sig_protect
  {
    int sync;
  public:
    __sig_protect(int syncset = TRUE, const char *fn = __FILE__, int ln = __LINE__) {sync = syncset; __block_sig_dispatch (fn, ln);}
    ~__sig_protect () {save_errno here; allow_sig_dispatch (sync);}
  };
