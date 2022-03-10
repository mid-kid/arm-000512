/* path.h: path data structures

   Copyright 1996, 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

struct suffix_info
{
  const char *name;
  int addon;
  suffix_info (const char *s, int addit = 0) {name = s, addon = addit;}
};

enum symlink_follow
{
  SYMLINK_FOLLOW,
  SYMLINK_NOFOLLOW,
  SYMLINK_IGNORE,
  SYMLINK_CONTENTS
};

class path_conv
{
  char path[MAX_PATH];
 public:

  char binary_p;
  int  symlink_p;
  int  exec_p;
  int  script_p;

  char *known_suffix;

  int error;
  DWORD devn;
  int unit;

  DWORD fileattr;

  path_conv (const char * const, symlink_follow follow_mode = SYMLINK_FOLLOW,
	     int use_full_path = 0, const suffix_info *suffixes = NULL);
  inline char *get_win32 () { return path; }
  BOOL is_device () {return devn != FH_BAD;}
  DWORD get_devn () {return devn == FH_BAD ? FH_DISK : devn;}
  short get_unitn () {return devn == FH_BAD ? 0 : unit;}
  DWORD file_attributes () {return fileattr;}
};

/* Symlink marker */
#define SYMLINK_COOKIE "!<symlink>"

/* Maximum depth of symlinks (after which ELOOP is issued).  */
#define MAX_LINK_DEPTH 10

extern suffix_info std_suffixes[];

int get_device_number (const char *name, int &unit, BOOL from_conv = FALSE);
BOOL win32_device_name (const char *src_path, char *win32_path);

/* Common macros for checking for invalid path names */
#define check_null_empty_path(src) \
  (!(src) ? EFAULT : *(src) ? 0 : ENOENT)

#define check_null_empty_path_errno(src) \
({ \
  int __err; \
  if ((__err = check_null_empty_path(src))) \
    set_errno (__err); \
  __err; \
})
