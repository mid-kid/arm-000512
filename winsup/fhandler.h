/* fhandler.h

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#ifndef _FHANDLER_H_
#define _FHANDLER_H_

#include <sys/ioctl.h>

/* Classes

  Code is located in fhandler.cc unless another file name is given.

  fhandler_base		  normal I/O

     fhandler_disk_file
     fhandler_serial      Adds vmin and vtime.
     fhandler_dev_null    Not really I/O

     fhandler_dev_raw     (fhandler_raw.cc)
     fhandler_dev_floppy  (fhandler_floppy.cc)
     fhandler_dev_tape    (fhandler_tape.cc)

     fhandler_pipe
     fhandler_socket      (net.cc)

     fhandler_tty_slave   (tty.cc)
     fhandler_pty_master  (tty.cc)
     fhandler_tty_master  (tty.cc)

     fhandler_console     Out with ansi control. (console.cc)

     fhandler_windows	  Windows messages I/O (fhandler_windows.cc)

     fhandler_proc	  Interesting possibility, not implemented yet
*/

enum
{
  FH_RBINARY = 0x00001000,	/* binary read mode */
  FH_WBINARY = 0x00002000,	/* binary write mode */
  FH_CLOEXEC = 0x00004000,	/* close-on-exec */
  FH_RAVALID = 0x00010000,	/* read-ahead valid */
  FH_APPEND  = 0x00020000,	/* always append */
  FH_ASYNC   = 0x00040000,	/* async I/O */
  FH_HADEOF  = 0x00080000,	/* EOF seen */

  FH_SYMLINK = 0x00100000,	/* is a symlink */
  FH_EXECABL = 0x00200000,	/* file looked like it would run:
				 * ends in .exe or .bat or begins with #! */
  FH_W95LSBUG= 0x00400000,	/* set when lseek is called as a flag that
				 * _write should check if we've moved beyond
				 * EOF, zero filling if so. */
  FH_NOFRNAME= 0x00800000,	/* Set if shouldn't free unix_path_name_ and
				   windows_path_name_ on destruction. */
  FH_NOEINTR = 0x01000000,	/* Set if I/O should be uninterruptible. */
  FH_FFIXUP  = 0x02000000,	/* Set if need to fixup after fork. */

  /* Device flags */

  /* Slow devices */
  FH_CONSOLE = 0x00000001,	/* is a console */
  FH_CONIN   = 0x00000002,	/* console input */
  FH_CONOUT  = 0x00000003,	/* console output */
  FH_TTYM    = 0x00000004,	/* is a tty master */
  FH_TTYS    = 0x00000005,	/* is a tty slave */
  FH_PTYM    = 0x00000006,	/* is a pty master */
  FH_SERIAL  = 0x00000007,	/* is a serial port */
  FH_PIPE    = 0x00000008,	/* is a pipe */
  FH_PIPER   = 0x00000009,	/* read end of a pipe */
  FH_PIPEW   = 0x0000000a,	/* write end of a pipe */
  FH_SOCKET  = 0x0000000b,	/* is a socket */
  FH_WINDOWS = 0x0000000c,	/* is a window */

  FH_SLOW    = 0x00000010,	/* "slow" device if below this */

  /* Fast devices */
  FH_DISK    = 0x00000010,	/* is a disk */
  FH_FLOPPY  = 0x00000011,	/* is a floppy */
  FH_TAPE    = 0x00000012,	/* is a tape */
  FH_NULL    = 0x00000013,	/* is the null device */

  FH_NDEV    = 0x00000014,	/* Maximum number of devices */
  FH_DEVMASK = 0x00000fff,	/* devices live here */
  FH_BAD     = 0xffffffff
};

#define FHDEVN(n)	((n) & FH_DEVMASK) 
#define FHISSETF(x)	(!!(status_ & FH_##x))
#define FHSETF(x)	(status_ |= FH_##x)
#define FHCLEARF(x)	(status_ &= ~FH_##x)
#define FHCONDSETF(n, x) ((n) ? FHSETF (x) : FHCLEARF (x))

#define FHSTATOFF	0

extern const char *windows_device_names[];
#define __fmode (*(user_data->fmode_ptr))

class select_record;
class path_conv;
class fhandler_disk_file;

class fhandler_base
{
private:
  DWORD status_;
public:
  int cb;
private:
  int access_;
  HANDLE handle_;

  int rpos_; /* Used in text reading */
  int rsize_;

  char readahead_;		/* used for crlf conversion in text files */

  unsigned long namehash_;	/* hashed filename, used as inode num */

  /* Full unix path name of this file */
  /* File open flags from open () and fcntl () calls */
  int openflags_;

protected:
  char *unix_path_name_;
  char *win32_path_name_;

public:
  void set_name (const char *unix, const char *win32 = NULL, int unit = 0);

  virtual fhandler_base& operator =(fhandler_base &x)
  {
    memcpy (this, &x, sizeof *this);
    unix_path_name_ = x.unix_path_name_ ? strdup (x.unix_path_name_) : NULL;
    win32_path_name_ = x.win32_path_name_ ? strdup (x.win32_path_name_) : NULL;
    return *this;
  };
  fhandler_base (DWORD dev, const char *name = 0, int unit = 0);
  virtual ~fhandler_base ();

  /* Non-virtual simple accessor functions. */
  void set_handle (HANDLE);

  void set_cb (size_t size) { cb = size; }
  DWORD get_device () { return status_ & FH_DEVMASK; }
  virtual int get_unit () { return 0; }
  virtual BOOL is_slow () { return get_device () < FH_SLOW; }

  int get_access () { return access_; }
  void set_access (int x) { access_ = x; }

  int get_async () { return FHISSETF (ASYNC); }
  void set_async (int x) { FHCONDSETF (x, ASYNC); }

  int get_flags () { return openflags_; }
  void set_flags (int x) { openflags_ = x; }

  int get_w_binary () { return FHISSETF (WBINARY); }
  int get_r_binary () { return FHISSETF (RBINARY); }

  void set_w_binary (int b) { FHCONDSETF (b, WBINARY); }
  void set_r_binary (int b) { FHCONDSETF (b, RBINARY); }

  int get_r_no_interrupt () { return FHISSETF (NOEINTR); }
  void set_r_no_interrupt (int b) { FHCONDSETF (b, NOEINTR); }

  int get_close_on_exec () { return FHISSETF (CLOEXEC); }
  int set_close_on_exec_flag (int b) { return FHCONDSETF (b, CLOEXEC); }

  void set_check_win95_lseek_bug (int b = 1) { FHCONDSETF (b, W95LSBUG); }
  int get_check_win95_lseek_bug () { return FHISSETF (W95LSBUG); }

  int get_need_fork_fixup () { return FHISSETF (FFIXUP); }
  void set_need_fork_fixup () { FHSETF (FFIXUP); }

  virtual void set_close_on_exec (int val);
  virtual void fixup_after_fork (HANDLE parent);

  int get_symlink_p () { return FHISSETF (SYMLINK); }
  void set_symlink_p (int val) { FHCONDSETF (val, SYMLINK); }
  void set_symlink_p () { FHSETF (SYMLINK); }

  int get_execable_p () { return FHISSETF (EXECABL); }
  void set_execable_p (int val) { FHCONDSETF (val, EXECABL); }
  void set_execable_p () { FHSETF (EXECABL); }

  int get_append_p () { return FHISSETF (APPEND); }
  void set_append_p (int val) { FHCONDSETF (val, APPEND); }
  void set_append_p () { FHSETF (APPEND); }

  int get_readahead_valid () { return FHISSETF (RAVALID); }
  void set_readahead_valid (int val) { FHCONDSETF (val, RAVALID); }
  void set_readahead_valid (int val, char value) {
    FHCONDSETF (val, RAVALID);
    readahead_ = value;
  }
  void set_readahead_valid () { FHSETF (RAVALID); }

  int no_free_names () { return FHISSETF (NOFRNAME); }
  void set_no_free_names (int val) { FHCONDSETF (val, NOFRNAME); }
  void set_no_free_names () { FHSETF (NOFRNAME); }

  const char *get_name () { return unix_path_name_; }
  const char *get_win32_name () { return win32_path_name_; }
  unsigned long get_namehash () { return namehash_; }


  /* fixup fd possibly non-inherited handles after fork */
  void fork_fixup (HANDLE parent, HANDLE &h);

  /* Potentially overridden virtual functions. */
  virtual int open (const char *, int flags, mode_t mode = 0) {
    return open (flags, mode);
  }
  virtual int open (int flags, mode_t mode = 0);
  virtual int close ();
  virtual int fstat (struct stat *buf);
  virtual int ioctl (unsigned int cmd, void *);
  virtual char const * ttyname () { return get_name(); }
  virtual int read (void *ptr, size_t len);
  virtual int write (const void *ptr, size_t len);
  virtual off_t lseek (off_t offset, int whence);
  virtual int lock (int, struct flock *);
  virtual void dump ();
  virtual int dup (fhandler_base *child);

  void *operator new (size_t, void *p) {return p;}

  virtual void init (HANDLE, DWORD, mode_t);

  virtual int tcflush (int);
  virtual int tcsendbreak (int);
  virtual int tcdrain ();
  virtual int tcflow (int);
  virtual int tcsetattr (int a, const struct termios *t);
  virtual int tcgetattr (struct termios *t);
  virtual int tcsetpgrp (const pid_t pid);
  virtual int tcgetpgrp ();
  virtual int is_tty () { return 0; }
  virtual BOOL is_device () { return TRUE; }
  virtual char *ptsname () { return NULL;}
  virtual class fhandler_socket *is_socket () { return 0; }
  virtual class fhandler_console *is_console () { return 0; }
  virtual int is_windows () {return 0; }

  virtual int raw_read (void *ptr, size_t ulen);
  virtual int raw_write (const void *ptr, size_t ulen);

  /* Function to save state of a fhandler_base into memory. */
  virtual int linearize(unsigned char *);
  /* Function to de-linearize into a fd */
  virtual int de_linearize(const unsigned char *);

  /* Virtual accessor functions to hide the fact
     that some fd's have two handles. */
  virtual HANDLE get_handle () const { return handle_; }
  virtual HANDLE get_input_handle () const { return handle_; }
  virtual HANDLE get_output_handle () const { return handle_; }
  virtual BOOL hit_eof () {return FALSE;}
  virtual select_record *select_read (select_record *s);
  virtual select_record *select_write (select_record *s);
  virtual select_record *select_except (select_record *s);
  virtual int ready_for_read (DWORD howlong, int ignra);
  virtual const char * get_native_name () {
    return windows_device_names[FHDEVN (status_)];
  }
};

class fhandler_socket: public fhandler_base
{
public:
  fhandler_socket (const char *name = 0);
  fhandler_socket (unsigned int, const char *name = 0);
  ~fhandler_socket ();
  int get_socket () const { return (int) get_handle(); }
  fhandler_socket * is_socket () { return this; }
  int write (const void *ptr, size_t len);
  int read (void *ptr, size_t len);
  int ioctl (unsigned int cmd, void *);
  off_t lseek (off_t offset, int whence) { return 0; }
  int close ();

  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);
};

class fhandler_pipe: public fhandler_base
{
public:
  fhandler_pipe (const char *name = 0);
  off_t lseek (off_t offset, int whence);
  BOOL is_slow () { return os_being_run == winNT; }

  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);
};

class fhandler_dev_raw: public fhandler_base
{
protected:
  char *devbuf;
  size_t devbufsiz;
  size_t devbufstart;
  size_t devbufend;
  int eom_detected    : 1;
  int eof_detected    : 1;
  int lastblk_to_read : 1;
  int is_writing      : 1;
  int has_written     : 1;
  int unit;

  virtual void clear(void);
  virtual int writebuf(void);

  /* returns not null, if `win_error' determines an end of media condition */
  virtual int is_eom(int win_error) = 0;
  /* returns not null, if `win_error' determines an end of file condition */
  virtual int is_eof(int win_error) = 0;

  fhandler_dev_raw (DWORD dev, const char *name, int unit);

public:
  ~fhandler_dev_raw (void);

  /* Function to save state of a fhandler_base into memory. */
  virtual int linearize(unsigned char *);
  /* Function to de-linearize into a fd */
  virtual int de_linearize(const unsigned char *);

  virtual int open (const char *path, int flags, mode_t mode = 0);
  virtual int close (void);

  virtual int raw_read (void *ptr, size_t ulen);
  virtual int raw_write (const void *ptr, size_t ulen);

  virtual int fstat (struct stat *buf);
  virtual int ioctl (unsigned int cmd, void *buf);
};

class fhandler_dev_floppy: public fhandler_dev_raw
{
protected:
  virtual int is_eom(int win_error);
  virtual int is_eof(int win_error);

public:
  fhandler_dev_floppy (const char *name, int unit);

  virtual int open (const char *path, int flags, mode_t mode = 0);
  virtual int close (void);

  virtual off_t lseek (off_t offset, int whence);

  virtual int ioctl (unsigned int cmd, void *buf);
  int get_native_name_with_unit ();
};

class fhandler_dev_tape: public fhandler_dev_raw
{
  int norewind;
  int lasterr;

protected:
  virtual void clear (void);

  virtual int is_eom (int win_error);
  virtual int is_eof (int win_error);

public:
  fhandler_dev_tape (const char *name, int unit);

  /* Function to save state of a fhandler_base into memory. */
  virtual int linearize (unsigned char *);
  /* Function to de-linearize into a fd */
  virtual int de_linearize (const unsigned char *);

  virtual int open (const char *path, int flags, mode_t mode = 0);
  virtual int close (void);

  virtual off_t lseek (off_t offset, int whence);

  virtual int fstat (struct stat *buf);
  virtual int ioctl (unsigned int cmd, void *buf);
  int get_native_name_with_unit ();

private:
  int tape_write_marks (int marktype, DWORD len);
  int tape_get_pos (unsigned long *ret);
  int tape_set_pos (int mode, long count, BOOLEAN sfm_func = FALSE);
  int tape_erase (int mode);
  int tape_prepare (int action);
  BOOLEAN tape_get_feature (DWORD parm);
  int tape_get_blocksize (long *min, long *def, long *max, long *cur);
  int tape_set_blocksize (long count);
  int tape_status (struct mtget *get);
  int tape_compression (long count);
};

/* Standard disk file */

class fhandler_disk_file: public fhandler_base
{
private:
  int check_execable_p (const char *path);

public:
  fhandler_disk_file (const char *name);

  int open (const char *path, int flags, mode_t mode = 0);
  int open (path_conv& real_path, int flags, mode_t mode);
  int close ();
  int lock (int, struct flock *);
  BOOL is_device () { return FALSE; }
  int fstat (struct stat *buf);
  const char *get_native ();
};

class fhandler_serial: public fhandler_base
{
private:
  unsigned int vmin_;			/* from termios */
  unsigned int vtime_;			/* from termios */
  pid_t pgrp_;

public:
  int overlapped_armed;
  OVERLAPPED io_status;

  /* Constructor */
  fhandler_serial (const char *name, DWORD devtype = FH_SERIAL, int unit = 0);

  int open (const char *path, int flags, mode_t mode);
  int close ();
  void init (HANDLE h, DWORD a, mode_t flags);
  void overlapped_setup ();
  int dup (fhandler_base *child);
  int raw_read (void *ptr, size_t ulen);
  int raw_write (const void *ptr, size_t ulen);
  int tcsendbreak (int);
  int tcdrain ();
  int tcflow (int);
  int tcsetattr (int a, const struct termios *t);
  int tcgetattr (struct termios *t);
  off_t lseek (off_t offset, int whence) { return 0; }
  int tcflush (int);
  void dump ();
  int is_tty () { return 1; }
  void fixup_after_fork (HANDLE parent);
  int de_linearize(const unsigned char *);

  /* We maintain a pgrp so that tcsetpgrp and tcgetpgrp work, but we
     don't use it for permissions checking.  fhandler_tty_slave does
     permission checking on pgrps.  */
  virtual int tcgetpgrp () { return pgrp_; }
  virtual int tcsetpgrp (const pid_t pid) { pgrp_ = pid; return 0; }
  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);

  const char *get_native ();
};

/* This is a input and output console handle */
class fhandler_console: public fhandler_serial
{
  void set_output_handle (HANDLE h) { output_handle_ = h; }
private:

/* Output state */

  // enum {normal, gotesc, gotsquare, gotarg1, gotcommand} state;
#define normal 1
#define gotesc 2
#define gotsquare 3
#define gotarg1 4
#define gotrsquare 5
#define gotcommand 6
#define gettitle 7
#define eattitle 8
#define MAXARGS 10
  int state_;
  int args_[MAXARGS];
  int nargs_;

/* Extra output handle */
  HANDLE output_handle_;
  void set_close_on_exec (int val);
  void fixup_after_fork (HANDLE parent);
  DWORD default_color;

/* Output calls */

  BOOL fillin_info ();
  void clear_screen (int, int, int, int);
  void scroll_screen (int, int, int, int, int, int);
  void cursor_set (BOOL, int, int);
  void cursor_get (int *, int *);
  void cursor_rel (int, int);
  void get_info ();
  const unsigned char * write_normal (unsigned const char*, unsigned const char *);
  void char_command (char);
  int output_tcsetattr (int a, const struct termios *t);

/* Input state */
  /* Bits are..
     IGNCR  ignore carriage return on input - whether we nuke '\r's
     the default for this is set by wheter the file is opened
     in text or binary mode.

     INLCR  translate NL to CR on input

     IUCLC  map uppercase characters to lowercase on input
  */
  int iflag_;
  int lflag_;

/* Input calls */
  int igncr_enabled ();
  int input_tcsetattr (int a, const struct termios *t);

public:
  fhandler_console (const char *name);

  fhandler_console* is_console () { return this; }

  int open (const char *path, int flags, mode_t mode=0);

  int write (const void *ptr, size_t len);
  int read (void *ptr, size_t len);
  int close ();

  int tcflush (int);
  int tcsetattr (int a, const struct termios *t);
  int tcgetattr (struct termios *t);

  /* Special dup as we must dup two handles */
  int dup (fhandler_base *child);

  int ioctl (unsigned int cmd, void *);
  void init (HANDLE, DWORD, mode_t);

  int read1 (char *buf, size_t lenin, DWORD &copied_chars);

  HANDLE get_output_handle() const { return output_handle_; }
  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);
};

class tty;

class fhandler_tty_common: public fhandler_base
{
public:
  tty *ttyp;
  void set_output_handle (HANDLE h) { output_handle_ = h; }
  fhandler_tty_common (DWORD dev, const char *name = 0, int unit = 0) :
    fhandler_base (dev, name, unit) {}
  HANDLE output_done_event;	// Raised by master when tty's output buffer
				// written. Write status in tty::write_retval.
  HANDLE ioctl_request_event;	// Raised by slave to perform ioctl() request.
				// Ioctl() request in tty::cmd/arg.
  HANDLE ioctl_done_event;	// Raised by master on ioctl() completion.
				// Ioctl() status in tty::ioctl_retval.
  HANDLE output_mutex;

  HANDLE output_handle_;	// Extra output handle;

  int ttynum;			// Master tty num.
  virtual int dup (fhandler_base *child);

  int get_unit () { return ttynum; }
  void set_close_on_exec (int val);
  void fixup_after_fork (HANDLE parent);
  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);
};

class fhandler_tty_slave: public fhandler_tty_common
{
  void send_ioctl_request ();

public:
  HANDLE inuse_event;	// used to indicate that a tty is in use

  /* Constructor */
  fhandler_tty_slave (const char *name);
  fhandler_tty_slave (int, const char *name);

  int open (const char *path, int flags, mode_t mode=0);
  int write (const void *ptr, size_t len);
  int read (void *ptr, size_t len);
  int close ();
  void init (HANDLE, DWORD, mode_t);

  int tcsetattr (int a, const struct termios *t);
  int tcgetattr (struct termios *t);
  int tcsetpgrp (const pid_t pid);
  int tcgetpgrp ();
  int tcflush (int);
  int ioctl (unsigned int cmd, void *);

  off_t lseek (off_t offset, int whence) { return 0; }
  int is_tty () { return 1; }

  /* Special dup */
  int dup (fhandler_base *child);
  HANDLE get_output_handle() const { return output_handle_; }
  void set_close_on_exec (int val);
  void fixup_after_fork (HANDLE parent);
};

class fhandler_pty_master: public fhandler_tty_common
{
  int pktmode;			// non-zero if pty in a packet mode.
public:
  HANDLE restart_output_event;
  int neednl_;			// Next read should start with \n

  /* Constructor */
  fhandler_pty_master (const char *name, DWORD devtype = FH_PTYM);

  int process_input_to_slave (const char *rptr, int nraw);
  int process_slave_output (char *buf, size_t len);
  void doecho (const void *str, unsigned len);

  int open (const char *path, int flags, mode_t mode=0);
  int write (const void *ptr, size_t len);
  int read (void *ptr, size_t len);
  int close ();

  int tcsetattr (int a, const struct termios *t);
  int tcgetattr (struct termios *t);
  int tcflush (int);
  int ioctl (unsigned int cmd, void *);

  off_t lseek (off_t offset, int whence) { return 0; }
  int is_tty () { return 1; }
  char *ptsname ();

  HANDLE get_output_handle() const { return output_handle_; }

  void set_close_on_exec (int val);
  void fixup_after_fork (HANDLE parent);
  BOOL hit_eof ();
};

class fhandler_tty_master: public fhandler_pty_master
{
public:
  /* Constructor */
  fhandler_tty_master (const char *name);
  fhandler_base *console;	// device handler to perform real i/o.
  HANDLE hThread;		// process_output thread handle.

  int init (int);
};

class fhandler_dev_null: public fhandler_base
{
public:
  fhandler_dev_null (const char *name);

  void dump ();
  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
};

class fhandler_windows: public fhandler_base
{
private:
  HWND hWnd_;	// the window whose messages are to be retrieved by read() call
  int method_;  // write method (Post or Send)
public:
  fhandler_windows (const char *name = 0);
  int is_windows (void) { return 1; }
  int open (const char *path, int flags, mode_t mode=0);
  int write (const void *ptr, size_t len);
  int read (void *ptr, size_t len);
  int ioctl (unsigned int cmd, void *);
  off_t lseek (off_t offset, int whence) { return 0; }
  int close (void) { return 0; }

  void set_close_on_exec (int val);
  void fixup_after_fork (HANDLE parent);
  select_record *select_read (select_record *s);
  select_record *select_write (select_record *s);
  select_record *select_except (select_record *s);
  int ready_for_read (DWORD howlong, int ignra);
};

#if 0
/* You can't do this */
typedef union
{
  fhandler_normal normal;
  fhandler_dev_null dev_null;
  fhandler bare;
  fhandler_serial tty;
} fhandler_union;
#else
#define fhandler_union fhandler_console
#endif

struct select_record
  {
    int fd;
    HANDLE h;
    fhandler_base *fh;
    BOOL saw_error;
    BOOL windows_handle;
    BOOL read_ready, write_ready, except_ready;
    BOOL read_selected, write_selected, except_selected;
    select_record (fhandler_base *in_fh = NULL) {memset (this, 0, sizeof(select_record)); fh = in_fh;}
    int (*startup) (select_record *me, class select_stuff *stuff);
    int (*poll) (select_record *me, fd_set *readfds, fd_set *writefds,
		 fd_set *exceptfds);
    int (*verify) (select_record *me, fd_set *readfds, fd_set *writefds,
		   fd_set *exceptfds);
    void (*cleanup) (select_record *me, class select_stuff *stuff);
    struct select_record *next;
  };

class select_stuff
  {
  public:
    ~select_stuff ();
    BOOL always_ready, windows_used;
    int total;
    select_record start;
    void *device_specific[FH_NDEV];
     
    int test_and_set (int i, fd_set *readfds, fd_set *writefds,
		       fd_set *exceptfds);
    int poll (fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
    int wait (fd_set *readfds, fd_set *writefds, fd_set *exceptfds, DWORD ms);
  };

uid_t get_file_owner (const char *);
gid_t get_file_group (const char *);

void set_inheritance (HANDLE &h, int val);

#endif /* _FHANDLER_H_ */
