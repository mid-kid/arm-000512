/* sigproc.h

   Copyright 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#define EXIT_SIGNAL    	 0x010000
#define EXIT_REPARENTING 0x020000
#define EXIT_NOCLOSEALL  0x040000

enum procstuff
{
  PROC_ADDCHILD		= 1,	// add a new subprocess to list
  PROC_CHILDSTOPPED	= 2,	// a child stopped
  PROC_CHILDTERMINATED	= 3,	// a child died
  PROC_CLEARWAIT	= 4,	// clear all waits - signal arrived
  PROC_WAIT		= 5	// setup for wait() for subproc
};

typedef struct struct_waitq
{
  int pid;
  int options;
  int status;
  HANDLE ev;
  void *rusage;			/* pointer to potential rusage */
  struct struct_waitq *next;
  HANDLE thread_ev;
} waitq;

extern HANDLE signal_mutex;
extern HANDLE signal_arrived;

extern void __block_sig_dispatch (const char *fn, int ln);
extern void __allow_sig_dispatch (const char *fn, int ln, int synch = TRUE);

BOOL alive_parent (pinfo *);
void sig_dispatch_pending (int force = FALSE);
extern "C" void set_process_mask (sigset_t newmask, int sigsync);
int sig_handle (int);
void sig_clear (int);
void sig_set_pending (int);
int handle_sigsuspend (sigset_t);

void proc_terminate (void);
DWORD __get_signal_mutex (const char *fn, int ln, DWORD howlong = 10000);
void __release_signal_mutex (const char *fn, int ln, int waitforit);
void sigproc_init (void);
void subproc_init (void);
void sigproc_terminate (void);
BOOL proc_exists (pinfo *);
int proc_subproc (DWORD, DWORD);
int sig_send (pinfo *, int);

extern char myself_nowait_dummy[];
extern char myself_nowait_nonmain_dummy[];
extern DWORD maintid;
extern HANDLE hExeced;		// Process handle of new window
				//  process created by spawn_guts()

#define WAIT_SIG_EXITING (WAIT_OBJECT_0 + 1)

#define get_signal_mutex(n...) __get_signal_mutex (__FILE__, __LINE__ , ## n)
#define release_signal_mutex(n) __release_signal_mutex (__FILE__, __LINE__, (n))
#define allow_sig_dispatch(n) __allow_sig_dispatch (__FILE__, __LINE__, (n))
#define block_sig_dispatch() __block_sig_dispatch (__FILE__, __LINE__)

#define myself_nowait ((pinfo *)myself_nowait_dummy)
#define myself_nowait_nonmain ((pinfo *)myself_nowait_nonmain_dummy)
#define proc_register(child) \
	proc_subproc (PROC_ADDCHILD, (DWORD) (child))
