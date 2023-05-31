/* shared.h: shared info for cygwin

   Copyright 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

/********************* Deletion Queue Class *************************/

/* First pass at a file deletion queue structure.

   We can't keep this list in the per-process info, since
   one process may open a file, and outlive a process which
   wanted to unlink the file - and the data would go away.

   Perhaps the FILE_FLAG_DELETE_ON_CLOSE would be ok,
   but brief experimentation didn't get too far.
*/   

#define MAX_DELQUEUES_PENDING 100

class delqueue_list 
{
  char name[MAX_DELQUEUES_PENDING][MAX_PATH];
  char inuse[MAX_DELQUEUES_PENDING];
  int empty;

public:
  void init ();  
  void queue_file (const char *dosname);
  void process_queue ();
};

/************************* Process Table ****************************/

/* Signal constants (have to define them here, unfortunately) */

enum
{
  __SIGFLUSH	    = -3,
  __SIGSTRACE	    = -2,
  __SIGSUSPEND	    = -1,
  __SIGCHILDSTOPPED =  0,
  __SIGOFFSET	    =  3
};

class pinfo
{
 public:

  /* If hProcess is set, it's because it came from a
     CreateProcess call.  This means it's process relative
     to the thing which created the process.  That's ok because
     we only use this handle from the parent. */
  HANDLE hProcess;

  HANDLE parent_alive;

  /* dwProcessId contains the processid used for sending signals.  It
   * will be reset in a child process when it is capable of receiving
   * signals.
   */
  DWORD dwProcessId;

  /* User information.
     The information is derived from the GetUserName system call,
     with the name looked up in /etc/passwd and assigned a default value
     if not found.  This data resides in the shared data area (allowing
     tasks to store whatever they want here) so it's for informational
     purposes only. */
  uid_t uid;	/* User ID */
  gid_t gid;	/* Group ID */
  pid_t pgid;	/* Process group ID */
  pid_t sid;	/* Session ID */
  int ctty;	/* Control tty */
  mode_t umask;

  /* Non-zero if process was stopped by a signal. */
  char stopsig;

  struct sigaction& getsig(int);
  void copysigs(pinfo *);
  sigset_t& getsigmask(); 
  void setsigmask(sigset_t);
  LONG* getsigtodo(int);
  HANDLE getthread2signal();
  void setthread2signal(void *);

  /* Resources used by process. */
  long start_time;
  struct rusage rusage_self;
  struct rusage rusage_children;

  /* Used to spawn a child for fork(), among other things. */
  char progname[MAX_PATH];

  #define PINFO_ZERO ((((pinfo *) NULL)->progname + 1) - ((char *) NULL))

  /* Anything below this point is not zeroed automatically by allocate_pid */

  /* The pid stays the same, while the hProcess moves due to execs. */
  pid_t pid;
  /* Parent process id.  */
  pid_t ppid;

  /* Pointer to strace mask, 0 if not set yet.  See strace.cc. */
  DWORD strace_mask;
  HANDLE strace_file;

  /* Various flags indicating the state of the process.  See PID_
     constants below. */
  DWORD process_state;

  /* Pointer to mmap'ed areas for this process.  Set up by fork. */
  void *mmap_ptr;

  void record_death (void);
  void record_death_nolock (void);
private:
  struct sigaction sigs[NSIG];
  sigset_t sig_mask;		/* one set for everything to ignore. */
  LONG _sigtodo[NSIG + __SIGOFFSET];
//  #define sigtodo(p, sig) (((p)->_sigtodo + __SIGOFFSET) + (sig))
#ifdef _MT_SAFE
  ThreadItem* thread2signal;  // NULL means means thread any other means a pthread
#endif
};

#define ISSTATE(p, f)	(!!((p)->process_state & f))
#define NOTSTATE(p, f)	(!((p)->process_state & f))

/* Flags associated with process_state */
enum
{
  PID_NOT_IN_USE       = 0x0000, // Free entry.
  PID_IN_USE	       = 0x0001, // Entry in use.
  PID_ZOMBIE	       = 0x0002, // Child exited: no parent wait.
  PID_STOPPED	       = 0x0004, // Waiting for SIGCONT.
  PID_TTYIN	       = 0x0008, // Waiting for terminal input.
  PID_TTYOU	       = 0x0010, // Waiting for terminal output.
  PID_ORPHANED	       = 0x0020, // Member of an orphaned process group.
  PID_ACTIVE	       = 0x0040, // Pid accepts signals.
  PID_CYGPARENT	       = 0x0080, // Set if parent was a cygwin app.
  PID_SPLIT_HEAP       = 0x0100, // Set if the heap has been split,
				 //  which means we can't fork again.
  PID_UNUSED1	       = 0x0200, // available
  PID_SOCKETS_USED     = 0x0400, // Set if process uses Winsock.
  PID_INITIALIZING     = 0x0800, // Set until ready to receive signals.
  PID_USETTY	       = 0x1000  // Setting this enables or disables cygwin's
				 //  tty support.  This is inherited by
				 //  all execed or forked processes.
};

#define PSIZE 128

class pinfo_list
{
 private:
  pinfo *get_empty_pinfo (void);

 public:
  int next_pid;
  pinfo vec[PSIZE];
  char lock_info[MAX_PATH + 1];
  pinfo *operator[] (pid_t x);
  int size (void) { return PSIZE; }
  pinfo *lookup_by_handle (HANDLE *);
  pinfo *allocate_pid (void);
  void init (void);
};

void pinfo_init (PBYTE);
pinfo *procinfo (int n);

enum
{
  PROC_MAGIC = 0xaf01f000,
  PROC_FORK = PROC_MAGIC + 1,
  PROC_EXEC = PROC_MAGIC + 2,
  PROC_SPAWN = PROC_MAGIC + 3,
  PROC_FORK1 = PROC_MAGIC + 4	// Newer versions provide stack
				// location information
};

#define EXEC_MAGIC_SIZE sizeof(child_info)
class child_info
{
public:
  DWORD zero;		// must be zeroed
  DWORD cb;		// size of this record
  DWORD type;		// type of record
  int cygpid;		// cygwin pid of child process
  HANDLE subproc_ready;	// used for synchronization with parent
  HANDLE shared_h;
};

class child_info_fork: public child_info
{
public:
  HANDLE forker_finished;// for synchronization with child
  DWORD stacksize;	// size of parent stack
  size_t heapsize;
  void *heapbase;
  void *heapptr;
  jmp_buf jmp;		// where child will jump to
  void *stacktop;	// location of parent stack
  void *stackbottom;	// actual amount to reserve for stack
};

extern child_info_fork *child_proc_info;

/* Process info for this process */
extern pinfo *myself;

/************************* Registry Access ****************************/

class reg_key
{
private:

  HKEY key;

public:

  void init (const char *name, HKEY r, REGSAM access);
  int open (const char *name, HKEY r, REGSAM access);

  reg_key (HKEY toplev, REGSAM access, ...);
  reg_key (REGSAM access, ...);
  reg_key (REGSAM access = KEY_ALL_ACCESS);

  void *operator new (size_t, void *p) {return p;}

  void build_reg (HKEY key, REGSAM access, va_list av);

  int error() {return key == (HKEY) INVALID_HANDLE_VALUE;}

  int kill (const char *child);

  HKEY get_key ();
  int get_int (const char *,int def);
  int get_string (const char *, char *buf, size_t len, const char *def);
  int getone_string (char *dst, const char *name, int len);
  int set_string (const char *,const char *);
  int set_int (const char *, int val);
  int fillone_string (char *dst, const char *name, const char *def, int max);
  int setone_string (const char *src, const char *name);
  int setone_int (const char *key, int val);

  ~reg_key ();
};

/************************* Mount Table ****************************/

/* Mount table entry */

class mount_item
{
public:
  /* FIXME: Nasty static allocation.  Need to have a heap in the shared
     area [with the user being able to configure at runtime the max size].  */

  /* Win32-style mounted partition source ("C:\foo\bar").
     native_path[0] == 0 for unused entries.  */
  char native_path[MAX_PATH];
  int native_pathlen;

  /* POSIX-style mount point ("/foo/bar") */
  char posix_path[MAX_PATH];
  int posix_pathlen;

  int flags;

  void init (const char *dev, const char *path, int flags);

  struct mntent *getmntent ();
};

/* Warning: Decreasing this value will cause cygwin.dll to ignore existing
   higher numbered registry entries.  Don't change this number willy-nilly.
   What we need is to have a more dynamic allocation scheme, but the current
   scheme should be satisfactory for a long while yet.  */
#define MAX_MOUNTS 30

class mount_info
{
  int posix_sorted[MAX_MOUNTS];
  int native_sorted[MAX_MOUNTS];
public:
  int nmounts;
  mount_item mount[MAX_MOUNTS];

  /* Strings used by getmntent(). */
  char mnt_type[20];
  char mnt_opts[20];
  char mnt_fsname[MAX_PATH];
  char mnt_dir[MAX_PATH];

  /* automount_prefix is used as the root of the path automatically
   added to the mount table when a Win32 drive can't be translated
   to a POSIX path via any other method.  automount_flags are the
   default flags for the automounts. */
  char automount_prefix[MAX_PATH];
  int automount_flags;

  /* Increment when setting up a reg_key if mounts area had to be
     created so we know when we need to import old mount tables. */
  int had_to_create_mount_areas;

  void init ();
  int add_item (const char *dev, const char *path, int flags);
  int del_item (const char *path, int flags);

  void from_registry ();
  void from_v1_registry ();
  int add_reg_mount (const char * native_path, const char * posix_path,
		      const int mountflags);
  int del_reg_mount (const char * posix_path, const int mountflags);

  int binary_win32_path_p (const char *path);
  int conv_to_win32_path (const char *src_path, char *win32_path,
			  char *full_win32_path, DWORD &devn, int &unit);
  int conv_to_posix_path (const char *src_path, char *posix_path,
			  int keep_rel_p);
  struct mntent *getmntent (int x);

  int write_automount_info_to_registry (const char *automount_prefix, const int flags);

  void import_v1_mounts ();

private:

  void sort ();
  void read_mounts (reg_key r);
  void read_v1_mounts (reg_key r, const int which);
  void mount_slash ();
  void to_registry ();
  int mount_areas_exist_p ();

  void build_automount_mountpoint_path (const char *src, char *dst);
  void slash_drive_to_win32_path (const char *path, char *buf, int trailing_slash_p);
  void read_automount_info_from_registry ();
};

/************************* TTY Support ****************************/

/* tty tables */

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

#include <sys/termios.h>

class tty_min
{
public:
  pid_t pgid;
  int OutputStopped;
  int read_retval;
  pid_t getpgid () {return pgid;}
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

  int write_retval;
};

class fhandler_pty_master;

class tty: public tty_min {
  HANDLE get_event (const char *fmt, BOOL inherit);
public:
  int ntty;

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

/************************* Shared Info ****************************/
/* Data accessible to all tasks */

class shared_info
{
  int inited;

public:
  pinfo_list p;

  /* FIXME: Doesn't work if more than one user on system. */
  mount_info mount;

  int heap_chunk_in_mb;
  int heap_chunk_size (void);

  tty_list tty;
  delqueue_list delqueue;
  void initialize (void);
};

/* Various types of security attributes for use in Create* functions. */
extern SECURITY_ATTRIBUTES sec_none, sec_none_nih, sec_all, sec_all_nih;

extern shared_info *cygwin_shared;
extern HANDLE cygwin_shared_h;

void shared_init (void);
void shared_terminate (void);

/* This is for programs that want to access the shared data. */
extern "C" class shared_info *cygwin_getshared (void);

char *shared_name (const char *, int);

struct cygwin_version_info
{
  unsigned short api_major;
  unsigned short api_minor;
  unsigned short dll_major;
  unsigned short dll_minor;
  unsigned short shared_data;
  unsigned short mount_registry;
  const char *dll_build_date;
  const char shared_id[sizeof (CYGWIN_VERSION_DLL_IDENTIFIER) + 64];
};

extern cygwin_version_info cygwin_version;
extern const char *cygwin_version_strings;
