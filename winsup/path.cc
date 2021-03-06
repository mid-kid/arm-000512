/* path.cc: path support.

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

/* This module's job is to
   - convert between POSIX and Win32 style filenames,
   - support the `mount' functionality,
   - support symlinks for files and directories

   Pathnames are handled as follows:

   - / is equivalent to \
   - Paths beginning with // (or \\) are not translated (i.e. looked
     up in the mount table) and are assumed to be UNC path names.
   - Paths containing a : are not translated (paths like
   /foo/bar/baz:qux: don't make much sense but having the rule written
   this way allows one to use strchr).

   The goal in the above set of rules is to allow both POSIX and Win32
   flavors of pathnames without either interfering.  The rules are
   intended to be as close to a superset of both as possible.

   A possible future enhancement would be to allow people to
   disable/enable the mount table handling to support pure Win32
   pathnames.  Hopefully this won't be needed.  The suggested way to
   do this would be an environment variable because
   a) we need something that is inherited from parent to child,
   b) environment variables can be passed from the DOS shell to a
   cygwin app,
   c) it allows disabling the feature on an app by app basis within
   the same session (whereas playing about with the registry wouldn't
   -- without getting too complicated).  Example:
   CYGWIN=pathrules[=@]{win32,posix}.  If CYGWIN=pathrules=win32,
   mount table handling is disabled.  [The intent is to have CYGWIN be
   a catchall for tweaking various cygwin.dll features].

   Note that you can have more than one path to a file.  The mount
   table is always prefered when translating Win32 paths to POSIX
   paths.  Win32 paths in mount table entries may be UNC paths or
   standard Win32 paths starting with <drive-letter>:

   In converting from a Win32 to a POSIX pathname, if there is no
   mount point that will allow the conversion to take place, a user
   mount point will be automatically created under
   automount_prefix/<drive> and the translation will be redone, this
   time successfully.

   Text vs Binary issues are not considered here in path style
   decisions.

   / and \ are treated as equivalent.  One or the other is prefered in
   certain situations (e.g. / is preferred in result of getcwd, \ is
   preferred in arguments to Win32 api calls), but this code will
   translate as necessary.

   Apps wishing to translate to/from pure Win32 and POSIX-like
   pathnames can use cygwin_foo.

   Removing mounted filesystem support would simplify things greatly,
   but having it gives us a mechanism of treating disk that lives on a
   UNIX machine as having UNIX semantics [it allows one to edit a text
   file on that disk and not have cr's magically appear and perhaps
   break apps running on UNIX boxes].  It also useful to be able to
   layout a hierarchy without changing the underlying directories.

   The semantics of mounting file systems is not intended to precisely
   follow normal UNIX systems.

   Each DOS drive is defined to have a current directory.  Supporting
   this would complicate things so for now things are defined so that
   c: means c:\.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <mntent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "winsup.h"
#include <ctype.h>

static int symlink_check_one (const char *path, char *buf, int buflen,
			      DWORD& fileattr, int *exec,
			      const suffix_info *suffixes,
			      char *&found_suffix);
static int normalize_win32_path (const char *cwd, const char *src, char *dst);
static char *getcwd_inner (char *buf, size_t ulen, int posix_p);
static void slashify (const char *src, char *dst, int trailing_slash_p);
static void backslashify (const char *src, char *dst, int trailing_slash_p);
static void nofinalslash (const char *src, char *dst);
static int path_prefix_p_ (const char *path1, const char *path2, int len1);
static int slash_unc_prefix_p (const char *path);
static int get_current_directory_name ();

static NO_COPY const char escape_char = '^';

/********************** Path Helper Functions *************************/

#define path_prefix_p(p1, p2, l1) \
       ((tolower(*(p1))==tolower(*(p2))) && \
       path_prefix_p_(p1, p2, l1))

#define SYMLINKATTR(x) \
  (((x) & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)) == \
   FILE_ATTRIBUTE_SYSTEM)

/* Return non-zero if PATH1 is a prefix of PATH2.
   Both are assumed to be of the same path style and / vs \ usage.
   Neither may be "".
   LEN1 = strlen (PATH1).  It's passed because often it's already known.

   Examples:
   /foo/ is a prefix of /foo  <-- may seem odd, but desired
   /foo is a prefix of /foo/
   / is a prefix of /foo/bar
   / is not a prefix of foo/bar
   foo/ is a prefix foo/bar
   /foo is not a prefix of /foobar
*/

static int
path_prefix_p_ (const char *path1, const char *path2, int len1)
{
  /* Handle case where PATH1 has trailing '/' and when it doesn't.  */
  if (SLASH_P (path1[len1 - 1]))
    len1--;

  if (len1 == 0)
    return SLASH_P (path2[0]) && !SLASH_P (path2[1]);

  if (!strncasematch (path1, path2, len1))
    return 0;

  return SLASH_P (path2[len1]) || path2[len1] == 0 || path1[len1 - 1] == ':';
}

/* Convert an arbitrary path SRC to a pure Win32 path, suitable for
   passing to Win32 API routines.

   If an error occurs, `error' is set to the errno value.
   Otherwise it is set to 0.

   follow_mode values:
	SYMLINK_FOLLOW	    - convert to PATH symlink points to
	SYMLINK_NOFOLLOW    - convert to PATH of symlink itself
	SYMLINK_IGNORE	    - do not check PATH for symlinks
	SYMLINK_CONTENTS    - just return symlink contents
*/

path_conv::path_conv (const char *src, symlink_follow follow_mode,
		      int use_full_path, const suffix_info *suffixes)
{
  /* This array is used when expanding symlinks.  It is MAX_PATH * 2
     in length so that we can hold the expanded symlink plus a
     trailer.  */
  char work_buf[MAX_PATH * 3 + 3];
  char tmp_buf[MAX_PATH];
  char path_buf[MAX_PATH];

  char *rel_path, *full_path;

  if ((error = check_null_empty_path (src)))
    return;

  if (use_full_path)
    rel_path = path_buf, full_path = this->path;
  else
    rel_path = this->path, full_path = path_buf;

  char *sym_buf = work_buf + MAX_PATH + 1;
  /* This loop handles symlink expansion.  */
  int loop = 0;
  symlink_p = 0;
  exec_p = 0;
  binary_p = 0;
  known_suffix = NULL;
  fileattr = (DWORD) -1;
  for (;;)
    {
      /* Must look up path in mount table, etc.  */
      error = cygwin_shared->mount.conv_to_win32_path (src, rel_path,
						       full_path,
						       devn, unit);
      if (error != 0)
	return;
      if (devn == FH_BAD)
	binary_p = cygwin_shared->mount.binary_win32_path_p (full_path);
      else
	{
	  binary_p = TRUE;		/* FIXME: probably not right */
	  fileattr = 0;
	  return;
	}

      /* Eat trailing slashes */
      char *tail = strchr (full_path, '\0');
      while (tail > full_path && (*--tail == '\\'))
	*tail = '\0';

      if (follow_mode == SYMLINK_IGNORE)
	{
	  fileattr = GetFileAttributesA (path);
	  return;
	}

      /* Make a copy of the path that we can munge up */
      char path_copy[strlen (full_path) + 2];
      strcpy (path_copy, full_path);

      tail = path_copy + 1 + (tail - full_path);   // Point to end of copy

      *sym_buf = '\0';			// Paranoid

      /* Scan path_copy from right to left looking either for a symlink
	 or an actual existing file.  If an existing file is found, just
	 return.  If a symlink is found exit the for loop.
	 Also: be careful to preserve the errno returned from
	 symlink_check_one as the caller may need it. */
      /* FIXME: Do we have to worry about multiple \'s here? */
      int component = 0;		// Number of translated components
      DWORD attr;
      for (;;)
	{
	  save_errno s (0);
	  int is_ex;
	  int *ep = component ? &is_ex : &exec_p;
	  const suffix_info *suff;
	  /* Don't allow symlink_check_one to set anything in the path_conv
	     class if we're working on an inner component of the path */
	  if (component)
	    {
	      ep = &is_ex;
	      suff = NULL;
	    }
	  else
	    {
	      ep = &exec_p;
	      suff = suffixes;
	    }
	  int len = symlink_check_one (path_copy, sym_buf, MAX_PATH, attr,
				       ep, suff, known_suffix);

	  /* If symlink_check_one found an existing non-symlink file, then
	     it returns a length of 0 and sets errno to EINVAL.  It also sets
	     any suffix found into `sym_buf'. */
	  if (!len && get_errno () == EINVAL)
	    {
	      if (component == 0)
		{
		  fileattr = attr;
		  if (follow_mode == SYMLINK_CONTENTS)
		    return;
		  else if (*sym_buf)
		    {
		      known_suffix = strchr (this->path, '\0');
		      strcpy (known_suffix, sym_buf);
		    }
		  else if (known_suffix)
		    known_suffix = this->path + (known_suffix - path_copy);
		}
	      return;	// file found
	    }
	  /* Found a symlink if len > 0.  If component == 0, then the
	     src path itself was a symlink.  If !follow_mode then
	     we're done.  Otherwise we have to insert the path found
	     into the full path that we are building and perform all of
	     these operations again on the newly derived path. */
	  else if (len > 0)
	    {
	      if (component == 0)
		{
		  if (follow_mode != SYMLINK_FOLLOW)
		    {
		      symlink_p = 1;	// last component of path's a symlink.
		      fileattr = attr;
		      if (follow_mode == SYMLINK_CONTENTS)
			  strcpy (path, sym_buf);
		      return;
		    }
		}
	      break;
	    }

	  s.reset ();      // remember errno from symlink_check_one

	  if (!(tail = strrchr (path_copy, '\\')) ||
	      (tail > path_copy && tail[-1] == ':'))
	    return;        // all done

	  /* Haven't found a valid pathname component yet.
	     Pinch off the tail and try again. */
	  *tail = '\0';
	  component++;
	}

      /* Arrive here if above loop detected a symlink. */
      if (++loop > MAX_LINK_DEPTH)
	{
	  error = ELOOP;   // Eep.
	  return;
	}

      tail = full_path + (tail - path_copy);
      int taillen = strlen (tail);
      int buflen = strlen (sym_buf);
      if (buflen + taillen > MAX_PATH)
	  {
	    error = ENAMETOOLONG;
	    strcpy (path, "::ENAMETOOLONG::");
	    return;
	  }

      /* Copy tail of full_path to discovered symlink. */
      char *p;
      for (p = sym_buf + buflen; *tail; tail++)
	*p++ = *tail == '\\' ? '/' : *tail;
      *p = '\0';

      /* If symlink referred to an absolute path, then we
	 just use sym_buf and loop.  Otherwise tack the head of
	 path_copy before sym_buf and translate it back from a
	 Win32-style path to a POSIX-style one. */
      if (isabspath (sym_buf))
	src = sym_buf;
      else if (!(tail = strrchr (path_copy, '\\')))
	system_printf ("problem parsing %s - '%s'", src, full_path);
      else
	{
	  int headlen = 1 + tail - path_copy;
	  p = sym_buf - headlen;
	  memcpy (p, path_copy, headlen);
	  error = cygwin_shared->mount.conv_to_posix_path (p, tmp_buf, 1);
	  if (error)
	    break;
	  src = tmp_buf;
	}
    }
}

#define deveq(s) (strcasematch (name, (s)))
#define deveqn(s, n) (strncasematch (name, (s), (n)))

static __inline int
digits (const char *name)
{
  char *p;
  int n = strtol(name, &p, 10);

  return p > name && !*p ? n : -1;
}

const char *windows_device_names[] =
{
  NULL,
  "\\dev\\console",
  "conin",
  "conout",
  "\\dev\\ttym",
  "\\dev\\tty%d",
  "\\dev\\ptym",
  "com%d",
  "\\dev\\pipe",
  "\\dev\\piper",
  "\\dev\\pipew",
  "\\dev\\socket",
  "\\dev\\windows",

  NULL, NULL, NULL,

  "\\dev\\disk",
  "\\dev\\fd%d",
  "\\dev\\st%d",
  "nul",
};

static int
get_raw_device_number (const char *uxname, const char *w32path, int &unit)
{
  DWORD devn = FH_BAD;

  if (strncasecmp (w32path, "\\\\.\\tape", 8) == 0)
    {
      devn = FH_TAPE;
      unit = digits (w32path + 8);
      // norewind tape devices have leading n in name
      if (! strncasecmp (uxname, "/dev/n", 6))
	unit += 128;
    }
  else if (isalpha (w32path[4]) && w32path[5] == ':')
    {
      devn = FH_FLOPPY;
      unit = tolower (w32path[4]) - 'a';
    }
  else if (strncasecmp (w32path, "\\\\.\\physicaldrive", 17) == 0)
    {
      devn = FH_FLOPPY;
      unit = digits (w32path + 17) + 128;
    }
  return devn;
}

int
get_device_number (const char *name, int &unit, BOOL from_conv)
{
  DWORD devn = FH_BAD;
  unit = 0;

  if ((*name == '/' && deveqn ("/dev/", 5)) ||
      (*name == '\\' && deveqn ("\\dev\\", 5)))
    {
      name += 5;
      if (deveq ("tty"))
	{
	  if (NOTSTATE (myself, PID_USETTY) && myself->ctty < 0)
	    devn = FH_CONSOLE;
	  else if (myself->ctty >= 0)
	    {
	      unit = myself->ctty;
	      devn = FH_TTYS;
	    }
	}
      else if (deveqn ("tty", 3) && (unit = digits (name + 3)) >= 0)
	devn = FH_TTYS;
      else if (deveq ("ttym"))
	devn = FH_TTYM;
      else if (deveq ("ptmx"))
	devn = FH_PTYM;
      else if (deveq ("windows"))
	devn = FH_WINDOWS;
      else if (deveq ("conin"))
	devn = FH_CONIN;
      else if (deveq ("conout"))
	devn = FH_CONOUT;
      else if (deveq ("null"))
	devn = FH_NULL;
      else if (deveqn ("com", 3) && (unit = digits (name + 3)) >= 0)
	devn = FH_SERIAL;
      else if (deveq ("pipe") || deveq ("piper") || deveq ("pipew"))
	devn = FH_PIPE;
      else if (deveq ("tcp") || deveq ("udp") || deveq ("streamsocket")
	       || deveq ("dgsocket"))
	devn = FH_SOCKET;
      else if (! from_conv)
	devn = get_raw_device_number (name - 5,
				      path_conv (name - 5,
						 SYMLINK_IGNORE).get_win32 (),
				      unit);
    }
  else if (deveqn ("com", 3) && (unit = digits (name + 3)) >= 0)
    devn = FH_SERIAL;

  return devn;
}

/* Return TRUE if src_path is a Win32 device name, FALSE otherwise. */
/* FIXME: right? */

BOOL
win32_device_name_p (const char *src_path, char *win32_path,
		   DWORD &devn, int &unit)
{
  const char *devfmt;

  devn = get_device_number (src_path, unit, TRUE);

  if (devn == FH_BAD)
    return FALSE;

  if ((devfmt = windows_device_names[FHDEVN (devn)]) == NULL)
    return FALSE;
  __small_sprintf (win32_path, devfmt, unit);
  return TRUE;
}

/* Normalize a POSIX path.
   \'s are converted to /'s in the process.
   All duplicate /'s, except for 2 leading /'s, are deleted.
   The result is 0 for success, or an errno error value.  */

static __inline int
normalize_posix_path (const char *cwd, const char *src, char *dst)
{
  const char *src_start = src;
  char *dst_start = dst;

  if (! SLASH_P (src[0]))
    {
      if (strlen (cwd) + 1 + strlen (src) >= MAX_PATH)
	{
	  debug_printf ("ENAMETOOLONG = normalize_posix_path (%s)", src);
	  return ENAMETOOLONG;
	}
      strcpy (dst, cwd);
      dst = strchr (dst, '\0');
      if (dst > (dst_start + 1))
	*dst++ = '/';
    }
  /* Two leading /'s?  If so, preserve them.  */
  else if (SLASH_P (src[1]))
    {
      *dst++ = '/';
      *dst++ = '/';
      src += 2;
      if (SLASH_P(*src))
	{ /* Starts with three or more slashes - reset. */
	  dst = dst_start;
	  *dst++ = '/';
	  src = src_start + 1;
	}
    }

  while (*src)
    {
      /* Strip runs of /'s.  */
      if (SLASH_P (*src))
	{
	  *dst++ = '/';
	  src++;
	  while (SLASH_P(*src))
	    src++;
	}
      /* Ignore "./".  */
      else if (src[0] == '.' && SLASH_P (src[1])
	       && (src == src_start || SLASH_P (src[-1])))
	{
	  src += 2;
	  while(SLASH_P(src[0]))
	    src++;
	}
      /* Backup if "..".  */
      else if (src[0] == '.' && src[1] == '.'
	       /* dst must be greater than dst_start */
	       && isdirsep (dst[-1])
	       && (SLASH_P (src[2]) || src[2] == 0))
	{
	  /* Back up over /, but not if it's the first one.  */
	  if (dst > dst_start + 1)
	    dst--;
	  /* Now back up to the next /.  */
	  while (dst > dst_start + 1 && !isdirsep (dst[-1]))
	    dst--;
	  src += 2;
	  while (SLASH_P (*src))
	    src++;
	}
      /* Otherwise, add char to result.  */
      else
	{
	  if (*src == '\\')
	    *dst++ = '/';
	  else
	    *dst++ = *src;
	  ++src;
	}
    }
  *dst = 0;
  debug_printf ("%s = normalize_posix_path (%s)", dst_start, src_start);
  return 0;
}

/* Normalize a Win32 path.
   /'s are converted to \'s in the process.
   All duplicate \'s, except for 2 leading \'s, are deleted.

   The result is 0 for success, or an errno error value.
   FIXME: A lot of this should be mergeable with the POSIX critter.  */

static int
normalize_win32_path (const char *cwd, const char *src, char *dst)
{
  const char *src_start = src;
  char *dst_start = dst;

  if (! SLASH_P (src[0])
      && strchr (src, ':') == NULL)
    {
      if (strlen (cwd) + 1 + strlen (src) >= MAX_PATH)
	{
	  debug_printf ("ENAMETOOLONG = normalize_win32_path (%s)", src);
	  return ENAMETOOLONG;
	}
      strcpy (dst, cwd);
      dst += strlen (dst);
      *dst++ = '\\';
    }
  /* Two leading \'s?  If so, preserve them.  */
  else if (SLASH_P (src[0]) && SLASH_P (src[1]))
    {
      *dst++ = '\\';
      ++src;
    }

  while (*src)
    {
      /* Strip duplicate /'s.  */
      if (SLASH_P (src[0]) && SLASH_P (src[1]))
	src++;
      /* Ignore "./".  */
      else if (src[0] == '.' && SLASH_P (src[1])
	       && (src == src_start || SLASH_P (src[-1])))
	{
	  src += 2;
	}
#if 0
      /* Strip trailing "/.".  */
      else if (src[0] == '.' && src[1] == 0
	       /* dst must be greater than dst_start */
	       && dst[-1] == '\\')
	{
	  /* Only strip ".", not "\.", if path is only "\.".  */
	  if (dst - 1 > dst_start)
	    dst--;
	  src++;
	}
#endif
      /* Backup if "..".  */
      else if (src[0] == '.' && src[1] == '.'
	       /* dst must be greater than dst_start */
	       && dst[-1] == '\\'
	       && (SLASH_P (src[2]) || src[2] == 0))
	{
	  /* Back up over /, but not if it's the first one.  */
	  if (dst > dst_start + 1)
	    dst--;
	  /* Now back up to the next /.  */
	  while (dst > dst_start + 1 && dst[-1] != '\\' && dst[-2] != ':')
	    dst--;
	  src += 2;
	  if (SLASH_P (*src))
	    src++;
	}
      /* Otherwise, add char to result.  */
      else
	{
	  if (*src == '/')
	    *dst++ = '\\';
	  else
	    *dst++ = *src;
	  ++src;
	}
    }
  *dst = 0;
  debug_printf ("%s = normalize_win32_path (%s)", dst_start, src_start);
  return 0;
}


/* Various utilities.  */

/* slashify: Convert all back slashes in src path to forward slashes
   in dst path.  Add a trailing slash to dst when trailing_slash_p arg
   is set to 1. */

static void
slashify (const char *src, char *dst, int trailing_slash_p)
{
  const char *start = src;

  while (*src)
    {
      if (*src == '\\')
	*dst++ = '/';
      else
	*dst++ = *src;
      ++src;
    }
  if (trailing_slash_p
      && src > start
      && !isdirsep (src[-1]))
    *dst++ = '/';
  *dst++ = 0;
}

/* backslashify: Convert all forward slashes in src path to back slashes
   in dst path.  Add a trailing slash to dst when trailing_slash_p arg
   is set to 1. */

static void
backslashify (const char *src, char *dst, int trailing_slash_p)
{
  const char *start = src;

  while (*src)
    {
      if (*src == '/')
	*dst++ = '\\';
      else
	*dst++ = *src;
      ++src;
    }
  if (trailing_slash_p
      && src > start
      && !isdirsep (src[-1]))
    *dst++ = '\\';
  *dst++ = 0;
}

/* nofinalslash: Remove trailing / and \ from SRC (except for the
   first one).  It is ok for src == dst.  */

static void
nofinalslash (const char *src, char *dst)
{
  int len = strlen (src);
  if (src != dst)
    memcpy (dst, src, len + 1);
  while (len > 1 && SLASH_P (dst[--len]))
    dst[len] = 0;
}

/* slash_drive_prefix_p: Return non-zero if PATH begins with
   //<letter>.  */

static int
slash_drive_prefix_p (const char *path)
{
  return (path[0] == '/'
	  && path[1] == '/'
	  && isalpha (path[2])
	  && (path[3] == 0 || path[3] == '/'));
}

/* slash_unc_prefix_p: Return non-zero if PATH begins with //UNC/SHARE */

static int
slash_unc_prefix_p (const char *path)
{
  char *p = NULL;
  int ret = (isdirsep (path[0])
	     && isdirsep (path[1])
	     && isalpha (path[2])
	     && path[3] != 0
	     && !isdirsep (path[3])
	     && ((p = strchr(&path[3], '/')) != NULL));
  if (!ret || p == NULL)
    return ret;
  return ret && isalnum (p[1]);
}

/* conv_path_list: Convert a list of path names to/from Win32/POSIX.

   SRC is not a const char * because we temporarily modify it to ease
   the implementation.

   I believe Win32 always has '.' in $PATH.   POSIX obviously doesn't.
   We certainly don't want to handle that here, but it is something for
   the caller to think about.  */

static void
conv_path_list (const char *src, char *dst, int to_posix_p)
{
  char *s;
  char *d = dst;
  char src_delim = to_posix_p ? ';' : ':';
  char dst_delim = to_posix_p ? ':' : ';';
  int (*conv_fn) (const char *, char *) = (to_posix_p
					   ? cygwin_conv_to_posix_path
					   : cygwin_conv_to_win32_path);

  do
    {
      s = strchr (src, src_delim);
      if (s)
	{
	  *s = 0;
	  (*conv_fn) (src[0] != 0 ? src : ".", d);
	  d += strlen (d);
	  *d++ = dst_delim;
	  *s = src_delim;
	  src = s + 1;
	}
      else
	{
	  /* Last one.  */
	  (*conv_fn) (src[0] != 0 ? src : ".", d);
	}
    }
  while (s != NULL);
}

/************************* mount_info class ****************************/

/* init: Initialize the mount table.  */

void
mount_info::init ()
{
  int found_slash = 0;

  nmounts = 0;
  had_to_create_mount_areas = 0;

  /* Fetch the mount table and automount-related information from
     the registry.  */
  from_registry ();

  /* If slash isn't already mounted, mount system directory as slash. */
  if (nmounts != 0)
    for (int i = 0; i < nmounts; i++)
      {
	if (strcmp (mount[i].posix_path, "/") == 0)
	  {
	    found_slash = 1;
	    break;
	  }
      }
  else
    mount_slash ();

  if (!found_slash)
    mount_slash ();
}

/* mount_slash: mount the system partition as slash. */

void
mount_info::mount_slash ()
{
  char drivestring[MAX_PATH];
  GetSystemDirectory (drivestring, MAX_PATH);
  drivestring[2] = 0;   /* truncate path to "<drive>:" */

  if (add_reg_mount (drivestring, "/", MOUNT_AUTO) == 0)
    add_item (drivestring, "/", MOUNT_AUTO);
}

/* conv_to_win32_path: Ensure src_path is a pure Win32 path and store
   the result in win32_path.

   If win32_path != NULL, the relative path, if possible to keep, is
   stored in win32_path.  If the relative path isn't possible to keep,
   the full path is stored.

   If full_win32_path != NULL, the full path is stored there.

   The result is zero for success, or an errno value.

   {,full_}win32_path must have sufficient space (i.e. MAX_PATH bytes).  */

int
mount_info::conv_to_win32_path (const char *src_path, char *win32_path,
				char *full_win32_path, DWORD &devn, int &unit)
{
  int src_path_len = strlen (src_path);
  int trailing_slash_p = (src_path_len > 0
			  && SLASH_P (src_path[src_path_len - 1]));

  debug_printf ("conv_to_win32_path (%s)", src_path);

  if (src_path_len >= MAX_PATH)
    {
      debug_printf ("ENAMETOOLONG = conv_to_win32_path (%s)", src_path);
      return ENAMETOOLONG;
    }

  if (win32_device_name_p (src_path, full_win32_path ?: win32_path, devn, unit))
    return 0;

  /* The rule is :'s can't appear in [our] POSIX path names so this is a safe
     test; if ':' is present it already be in Win32 form.  */
  if (strchr (src_path, ':') != NULL)
    {
      debug_printf ("%s already win32", src_path);
      if (win32_path != NULL)
	backslashify (src_path, win32_path, trailing_slash_p);
      if (full_win32_path != NULL)
	backslashify (src_path, full_win32_path, trailing_slash_p);
      return 0;
    }

  /* Normalize the path, taking out ../../ stuff, we need to do this
     so that we can move from one mounted directory to another with relative
     stuff.

     eg mounting c:/foo /foo
     d:/bar /bar

     cd /bar
     ls ../foo

     should look in c:/foo, not d:/foo.

     We do this by first getting an absolute UNIX-style path and then
     converting it to a DOS-style path, looking up the appropriate drive
     in the mount table.  */

  char pathbuf[MAX_PATH];
  char cwd[MAX_PATH];

  /* No need to fetch cwd if path is absolute.  */
  if (! SLASH_P (*src_path))
    getcwd_inner (cwd, MAX_PATH, 1); /* FIXME: check rc */
  else
    strcpy (cwd, "/"); /* some innocuous value */
  int rc = normalize_posix_path (cwd, src_path, pathbuf);
  if (rc != 0)
    {
      debug_printf ("%d = conv_to_win32_path (%s)", rc, src_path);
      return rc;
    }
  nofinalslash (pathbuf, pathbuf);

  for (int i = 0; i < nmounts; ++i)
    {
      mount_item &mi = mount[posix_sorted[i]];
      if (! path_prefix_p (mi.posix_path, pathbuf, mi.posix_pathlen))
	continue;

      /* SRC_PATH is in the mount table.  */

      /* Compute relative path if asked to and able to.  */
      int got_rel_p = 0;
      if (win32_path != NULL
	  && ! SLASH_P (*src_path)
	  /* Ensure the path didn't ../ out of cwd.
	     There are times when we could keep ../foo as is, but
	     normalize_posix_path strips all ../'s and it's [currently] too
	     much work to deal with them.  */
	  && path_prefix_p (cwd, pathbuf, strlen (cwd)))
	{
	  if (path_prefix_p (cwd, mi.posix_path, strlen (cwd)))
	    {
	      if (strcmp (pathbuf, mi.posix_path) == 0)
		strcpy (win32_path, ".");
	      else
		{
		  int n = strlen (cwd);
		  if (n > 1)
		    n++;
		  strcpy (win32_path, pathbuf + n);
		}
	    }
	  else
	    {
	      /* must be path_prefix_p (mi.path, cwd,
						  mi.pathlen) */
	      if (strcmp (pathbuf, cwd) == 0)
		strcpy (win32_path, ".");
	      else
		strcpy (win32_path, pathbuf + strlen (cwd) + 1);
	    }
	  backslashify (win32_path, win32_path, trailing_slash_p);
	  got_rel_p = 1;
	}

      /* If we've matched / and the path begins with the slash drive prefix,
	 or the UNC path prefix, break out.  If the slash drive path was
	 mounted, it would have matched already.  */
      if (! got_rel_p
	  && strcmp (mi.posix_path, "/") == 0
	  && (slash_drive_prefix_p (pathbuf) || slash_unc_prefix_p(pathbuf)))
	break;

      /* Compute the full Win32 path.
	 We go to some lengths to avoid unnecessary copying.  */
      char *p = NULL;
      if (full_win32_path != NULL)
	p = full_win32_path;
      else if (win32_path != NULL && ! got_rel_p)
	p = win32_path;
      if (p != NULL)
	{
	  int j = mi.native_pathlen;
	  memcpy (p, mi.native_path, j);
	  /* Do not add trailing \ to UNC device names like \\.\a: */
	  if (pathbuf[mi.posix_pathlen] != '/' &&
	     !(strncmp (mi.native_path, "\\\\.\\", 4) == 0 &&
	       strncmp (mi.native_path + 4, "UNC\\", 4) != 0))
	    p[j++] = '\\';
	  strcpy (p + j, pathbuf + mi.posix_pathlen);
	  backslashify (p, p, trailing_slash_p);
	}
      if (win32_path != NULL && ! got_rel_p && win32_path != p)
	strcpy (win32_path, p);

      debug_printf ("%s = conv_to_win32_path (%s)",
		    win32_path != NULL ? win32_path : full_win32_path,
		    src_path);
      return 0;
    }

  char *p;
  if (full_win32_path != NULL)
    p = full_win32_path;
  else
    p = win32_path;

  /* Not in the database.  Is it a //<letter> path? */
  if (slash_drive_prefix_p (pathbuf))
    slash_drive_to_win32_path (pathbuf, p, trailing_slash_p);
  else
    {
      /* Can't translate it.  Just ensure no /'s are present.
	 Note that this should [theoretically] only happen if / isn't in
	 the data base, but that's ok [I think].
	 The use of posix_path and not pathbuf here is intentional.
	 We couldn't translate the path, so just ensure no \'s are present. */
      backslashify (src_path, p, trailing_slash_p);
    }

  if (win32_path != NULL && win32_path != p)
    strcpy (win32_path, p);

  debug_printf ("%s = conv_to_win32_path (%s)",
		win32_path != NULL ? win32_path : full_win32_path,
		src_path);
  return 0;
}

/* Convert PATH (for which slash_drive_prefix_p returns 1) to WIN32 form.  */

void
mount_info::slash_drive_to_win32_path (const char *path, char *buf,
				       int trailing_slash_p)
{
  buf[0] = path[2];
  buf[1] = ':';
  if (path[3] == '0')
    strcpy (buf + 2, "\\");
  else
    backslashify (path + 3, buf + 2, trailing_slash_p);
}

/* build_automount_mountpoint_path: Build POSIX path used as the
   mount point for automounts created when there is no other way to
   obtain a POSIX path from a Win32 one. */

void
mount_info::build_automount_mountpoint_path (const char *src, char *dst)
{
  int len = strlen (automount_prefix);

  memcpy (dst, automount_prefix, len + 1);

  /* Add trailing slash if automount_prefix isn't '/' and doesn't already
     end with a slash. */
  if ( (len > 1) && (! SLASH_P (dst[len - 1])))
    dst[len++] = '/';

  /* Now finish the path off with the drive letter to be automounted. */
  dst[len++] = tolower (src[0]);

  /* And complete the string. */
  dst[len++] = '\0';
}

/* conv_to_posix_path: Ensure src_path is a POSIX path.

   The result is zero for success, or an errno value.
   posix_path must have sufficient space (i.e. MAX_PATH bytes).
   If keep_rel_p is non-zero, relative paths stay that way.  */

int
mount_info::conv_to_posix_path (const char *src_path, char *posix_path,
				int keep_rel_p)
{
  int src_path_len = strlen (src_path);
  int trailing_slash_p = (src_path_len > 0
			  && SLASH_P (src_path[src_path_len - 1]));
  int relative_path_p = (! SLASH_P (*src_path)
			 && strchr (src_path, ':') == NULL);

  debug_printf ("conv_to_posix_path (%s, %s)", src_path,
		keep_rel_p ? "keep-rel" : "no-keep-rel");

  if (src_path_len >= MAX_PATH)
    {
      debug_printf ("ENAMETOOLONG");
      return ENAMETOOLONG;
    }

  /* FIXME: For now, if the path is relative and it's supposed to stay
     that way, skip mount table processing. */
  if (keep_rel_p && relative_path_p)
    {
      slashify (src_path, posix_path, 0);
      debug_printf ("%s = conv_to_posix_path (%s)", posix_path, src_path);
      return 0;
    }

  char pathbuf[MAX_PATH];
  char cwd[MAX_PATH];

  /* No need to fetch cwd if path is absolute. */
  if (relative_path_p)
    getcwd_inner (cwd, MAX_PATH, 0); /* FIXME: check rc */
  else
    strcpy (cwd, "/"); /* some innocuous value */
  int rc = normalize_win32_path (cwd, src_path, pathbuf);
  if (rc != 0)
    {
      debug_printf ("%d = conv_to_posix_path (%s)", rc, src_path);
      return rc;
    }
  nofinalslash (pathbuf, pathbuf);

  for (int i = 0; i < nmounts; ++i)
    {
      mount_item &mi = mount[native_sorted[i]];
      if (! path_prefix_p (mi.native_path, pathbuf, mi.native_pathlen))
	continue;

      /* SRC_PATH is in the mount table. */
      int nextchar;
      if (!pathbuf[mi.native_pathlen])
	nextchar = 0;
      else if (isdirsep (pathbuf[mi.native_pathlen]))
	nextchar = -1;
      else
	nextchar = 1;

      int addslash = nextchar > 0 ? 1 : 0;
      if ((mi.posix_pathlen + (strlen (pathbuf) - mi.native_pathlen) + addslash) >= MAX_PATH)
	return ENAMETOOLONG;
      strcpy (posix_path, mi.posix_path);
      if (addslash)
	strcat (posix_path, "/");
      if (nextchar)
	slashify (pathbuf + mi.native_pathlen,
		  posix_path + addslash + (mi.posix_pathlen == 1 ? 0 : mi.posix_pathlen),
		trailing_slash_p);
      debug_printf ("%s = conv_to_posix_path (%s)", posix_path, src_path);
      return 0;
    }

  /* Not in the database.  This should [theoretically] only happen if either
     the path begins with //, or / isn't mounted, or the path has a drive
     letter not covered by the mount table.  If it's a relative path then the
     caller must want an absolute path (otherwise we would have returned
     above).  So we always return an absolute path at this point. */
  if ((isalpha (pathbuf[0])) && (pathbuf[1] == ':'))
    {
      char pathtmp[MAX_PATH];
      char dev[3];

      dev[0] = pathbuf[0];
      dev[1] = pathbuf[1];
      dev[2] = '\0';

      slashify (pathbuf, pathtmp, 0);
      build_automount_mountpoint_path (pathtmp, posix_path);

      /* Note: Can't call mount system call because we use MOUNT_AUTO
	 to tell mount when to change the automount prefix. */
      if (add_reg_mount (dev, posix_path, automount_flags) == 0)
	add_item (dev, posix_path, automount_flags);

      /* Recursion should be safe because this point should
	 only be able to happen once. */
      conv_to_posix_path (src_path, posix_path, keep_rel_p);
    }
  else
    {
      /* The use of src_path and not pathbuf here is intentional.
	 We couldn't translate the path, so just ensure no \'s are present. */
      slashify (src_path, posix_path, trailing_slash_p);
    }

  debug_printf ("%s = conv_to_posix_path (%s)", posix_path, src_path);
  return 0;
}

/* binary_win32_path_p: Return non-zero if p, a pure Win32 path,
   points to a directory mounted with -b [binary mode]. */

int
mount_info::binary_win32_path_p (const char *p)
{
  for (int i = 0; i < nmounts; i++)
    {
      mount_item &mi = mount[native_sorted[i]];
      if (path_prefix_p (mi.native_path, p, mi.native_pathlen))
      return ((mi.flags & MOUNT_BINARY) != 0);
    }
  return 0;
}

/* read_mounts: Given a specific regkey, read mounts from under its
   key. */

void
mount_info::read_mounts (reg_key r)
{
  HKEY key = r.get_key ();
  LONG err;
  char native_path[MAX_PATH];
  int mount_flags;

  /* Loop through subkeys */
  /* FIXME: we would like to not check MAX_MOUNTS but the heap in the
     shared area is currently statically allocated so we can't have an
     arbitrarily large number of mounts. */
  for (DWORD i = 0; i < MAX_MOUNTS; i++)
    {
      char posix_path[MAX_PATH];
      DWORD posix_path_size = MAX_PATH;

      /* FIXME: if maximum posix_path_size is 256, we're going to
	 run into problems if we ever try to store a mount point that's
	 over 256 but is under MAX_PATH! */
      err = RegEnumKeyEx (key, i, posix_path, &posix_path_size, NULL,
			  NULL, NULL, NULL);

      if (err == ERROR_NO_MORE_ITEMS)
	break;
      else if (err != ERROR_SUCCESS)
	{
	  debug_printf ("RegEnumKeyEx failed, error %d!\n", err);
	  break;
	}

      /* Get a reg_key based on i. */
      reg_key subkey = reg_key (key, KEY_READ, posix_path, NULL);

      /* Fetch info from the subkey. */
      subkey.get_string ("native", native_path, sizeof (native_path), "");
      mount_flags = subkey.get_int ("flags", 0);

      /* Add mount_item corresponding to registry mount point. */
      cygwin_shared->mount.add_item (native_path, posix_path, mount_flags);
    }
}

/* from_registry: Build the entire mount table from the registry.  Also,
   read in automount-related information from its registry location. */

void
mount_info::from_registry ()
{
  /* Use current mount areas if either user or system mount areas
     already exist.  Otherwise, import old mounts. */

  reg_key r;

  nmounts = 0;

  /* First read mounts from user's table. */
  read_mounts (r);

  /* Then read mounts from system-wide mount table. */
  reg_key r1 (HKEY_LOCAL_MACHINE, KEY_READ, "SOFTWARE",
	      CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	      CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	      CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME,
	      NULL);
  read_mounts (r1);

  /* If we had to create both user and system mount areas, import
     old mounts. */
  if (had_to_create_mount_areas == 2)
    import_v1_mounts ();

  sort ();

  /* Retrieve automount-related information. */
  read_automount_info_from_registry ();
}

/* add_reg_mount: Add mount item to registry.  Return zero on success,
   non-zero on failure. */
/* FIXME: Need a mutex to avoid collisions with other tasks. */

int
mount_info::add_reg_mount (const char * native_path, const char * posix_path, const int mountflags)
{
  /* Add the mount to the right registry location, depending on
     whether MOUNT_SYSTEM is set in the mount flags. */
  if ((mountflags & MOUNT_SYSTEM) == 0) /* current_user mount */
    {
      /* reg_key for user mounts in HKEY_CURRENT_USER. */
      reg_key reg_user;
      /* (KEY_ALL_ACCESS, CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME, NULL); */

      /* Start by deleting existing mount if one exists. */
      reg_user.kill (posix_path);

      /* Create the new mount. */
      reg_key subkey = reg_key (reg_user.get_key (),
				KEY_ALL_ACCESS,
				posix_path, NULL);
      subkey.set_string ("native", native_path);
      subkey.set_int ("flags", mountflags);
    }
  else /* local_machine mount */
    {
      /* reg_key for system mounts in HKEY_LOCAL_MACHINE. */
      reg_key reg_sys (HKEY_LOCAL_MACHINE, KEY_ALL_ACCESS, "SOFTWARE",
		       CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
		       CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
		       CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME,
		       NULL);

      if (reg_sys.get_key () == INVALID_HANDLE_VALUE)
	{
	  set_errno (EACCES);
	  return -1;
	}

      /* Start by deleting existing mount if one exists. */
      reg_sys.kill (posix_path);

      /* Create the new mount. */
      reg_key subkey = reg_key (reg_sys.get_key (),
				KEY_ALL_ACCESS,
				posix_path, NULL);
      subkey.set_string ("native", native_path);
      subkey.set_int ("flags", mountflags);
    }

  return 0; /* Success! */
}

/* del_reg_mount: delete mount item from registry indicated in flags.
   Return zero on success, non-zero on failure.*/
/* FIXME: Need a mutex to avoid collisions with other tasks. */

int
mount_info::del_reg_mount (const char * posix_path, const int flags)
{
  int killres;

  if ((flags & MOUNT_SYSTEM) == 0)    /* Delete from user registry */
    {
      reg_key reg_user (KEY_ALL_ACCESS,
			CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME, NULL);
      killres = reg_user.kill (posix_path);
    }
  else                                /* Delete from system registry */
    {
      reg_key reg_sys (HKEY_LOCAL_MACHINE, KEY_ALL_ACCESS, "SOFTWARE",
		       CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
		       CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
		       CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME,
		       NULL);

      if (reg_sys.get_key () == INVALID_HANDLE_VALUE)
	{
	  set_errno (EACCES);
	  return -1;
	}

      killres = reg_sys.kill (posix_path);
    }

  if (killres != ERROR_SUCCESS)
    {
      __seterrno_from_win_error (killres);
      return -1;
    }

  return 0; /* Success! */
}

/* read_automount_info_from_registry: Read the default prefix and flags
   to use when creating automounts from the special user registry
   location used to store automount information. */

void
mount_info::read_automount_info_from_registry ()
{
  /* reg_key for user mounts in HKEY_CURRENT_USER. */
  reg_key r;

  if (r.get_string ("automount prefix", automount_prefix, sizeof (automount_prefix), "") != 0)
    {
      /* Didn't find it so write the default to the registry and use it. */
      strcpy (automount_prefix, "/cygdrive");
      write_automount_info_to_registry (automount_prefix, MOUNT_AUTO);
    }
  else
    {
      /* Fetch automount_flags from registry; returns MOUNT_AUTO on error. */
      automount_flags = r.get_int ("automount flags", MOUNT_AUTO);
    }
}

/* write_automount_info_to_registry: Write the default prefix and flags
   to use when creating automounts to the special user registry
   location used to store automount information. */

int
mount_info::write_automount_info_to_registry (const char *auto_prefix, const int auto_flags)
{
  /* reg_key for user mounts in HKEY_CURRENT_USER. */
  reg_key r;

  /* Verify automount prefix starts with a forward slash and if there's
     another character, it's not a slash. */
  if ((auto_prefix == NULL) || (*auto_prefix == 0) ||
      (auto_prefix[0] != '/') ||
      ((auto_prefix[1] != '\0') && (SLASH_P (auto_prefix[1]))))
      {
	set_errno (EINVAL);
	return -1;
      }

  r.set_string ("automount prefix", auto_prefix);
  r.set_int ("automount flags", auto_flags);

  return 0;
}

struct mntent *
mount_info::getmntent (int x)
{
  if (x < 0 || x >= nmounts)
    return NULL;

  return mount[native_sorted[x]].getmntent ();
}

static mount_item *mounts_for_sort;

/* sort_by_posix_name: qsort callback to sort the mount entries.  Sort
   user mounts ahead of system mounts to the same POSIX path. */
/* FIXME: should the user should be able to choose whether to
   prefer user or system mounts??? */
int
sort_by_posix_name (const void *a, const void *b)
{
  mount_item *ap = mounts_for_sort + (*((int*) a));
  mount_item *bp = mounts_for_sort + (*((int*) b));

  /* Base weighting on longest posix path first so that the most
     obvious path will be chosen. */
  size_t alen = strlen (ap->posix_path);
  size_t blen = strlen (bp->posix_path);

  if (alen == 0)
    return 1;

  int res = blen - alen;

  if (res)
    return res;		/* Path lengths differed */

  /* The two paths were the same length, so just determine normal
     lexical sorted order. */
  res = strcmp (ap->posix_path, bp->posix_path);

  if (res == 0)
   {
     /* need to select between user and system mount to same POSIX path */
     if ((bp->flags & MOUNT_SYSTEM) == 0) /* user mount */
      return 1;
     else
      return -1;
   }

  return res;
}

/* sort_by_native_name: qsort callback to sort the mount entries.  Sort
   user mounts ahead of system mounts to the same POSIX path. */
/* FIXME: should the user should be able to choose whether to
   prefer user or system mounts??? */
int
sort_by_native_name (const void *a, const void *b)
{
  mount_item *ap = mounts_for_sort + (*((int*) a));
  mount_item *bp = mounts_for_sort + (*((int*) b));

  /* Base weighting on longest win32 path first so that the most
     obvious path will be chosen. */
  size_t alen = strlen (ap->native_path);
  size_t blen = strlen (bp->native_path);

  if (alen == 0)
    return 1;

  int res = blen - alen;

  if (res)
    return res;		/* Path lengths differed */

  /* The two paths were the same length, so just determine normal
     lexical sorted order. */
  res = strcasecmp (ap->posix_path, bp->posix_path);

  if (res == 0)
   {
     /* need to select between user and system mount to same POSIX path */
     if ((bp->flags & MOUNT_SYSTEM) == 0) /* user mount */
      return 1;
     else
      return -1;
   }

  return res;
}

void
mount_info::sort ()
{
  for (int i = 0; i < nmounts; i++)
    native_sorted[i] = posix_sorted[i] = i;
  /* Sort them into reverse length order, otherwise we won't
     be able to look for /foo in /.  */
  mounts_for_sort = mount;	/* ouch. */
  qsort (posix_sorted, nmounts, sizeof (posix_sorted[0]), sort_by_posix_name);
  qsort (native_sorted, nmounts, sizeof (native_sorted[0]), sort_by_native_name);
}

/* Add an entry to the mount table.
   Returns 0 on success, -1 on failure and errno is set.

   This is where all argument validation is done.  It may not make sense to
   do this when called internally, but it's cleaner to keep it all here.  */

int
mount_info::add_item (const char *native, const char *posix, int mountflags)
{
  /* Can't add more than MAX_MOUNTS. */
  if (nmounts == MAX_MOUNTS)
    {
      set_errno (EMFILE);
      return -1;
    }

  /* Something's wrong if either path is NULL or empty, or if it's
     not a UNC or absolute path. */

  if ((native == NULL) || (*native == 0) ||
      (posix == NULL) || (*posix == 0) ||
      (!slash_unc_prefix_p (native) && !isabspath (native)))
    {
      set_errno (EINVAL);
      return -1;
    }

  /* Make sure both paths do not end in /. */
  char nativetmp[MAX_PATH];
  char posixtmp[MAX_PATH];

  if (slash_drive_prefix_p (native))
    slash_drive_to_win32_path (native, nativetmp, 0);
  else
    {
      backslashify (native, nativetmp, 0);
      nofinalslash (nativetmp, nativetmp);
    }

  slashify (posix, posixtmp, 0);
  nofinalslash (posixtmp, posixtmp);

  debug_printf ("%s[%s], %s[%s], %p",
		native, nativetmp, posix, posixtmp, mountflags);

  /* Duplicate /'s in path are an error. */
  for (char *p = posixtmp + 1; *p; ++p)
    {
      if (p[-1] == '/' && p[0] == '/')
	{
	  set_errno (EINVAL);
	  return -1;
	}
    }

  /* Write over an existing mount item with the same POSIX path if
     it exists and is from the same registry area. */
  for (int i = 0; i < nmounts; i++)
    {
      if ((strcmp (mount[i].posix_path, posixtmp) == 0) &&
	  ((mount[i].flags & MOUNT_SYSTEM) == (mountflags & MOUNT_SYSTEM)))
	{
	  /* replace existing mount item */
	  mount[i].init (nativetmp, posixtmp, mountflags);
	  goto sortit;
	}
    }

  mount[nmounts++].init (nativetmp, posixtmp, mountflags);

sortit:
  sort ();

  return 0;
}

/* Delete a mount table entry where path is either a Win32 or POSIX
   path. Since the mount table is really just a table of aliases,
   deleting / is ok (although running without a slash mount is
   strongly discouraged because some programs may run erratically
   without one).  If MOUNT_SYSTEM is set in flags, remove from system
   registry, otherwise remove the user registry mount.
*/

int
mount_info::del_item (const char *path, int flags)
{
  char pathtmp[MAX_PATH];

  /* Something's wrong if path is NULL or empty. */
  if ((path == NULL) || (*path == 0))
    {
      set_errno (EINVAL);
      return -1;
    }

  slashify (path, pathtmp, 0);
  nofinalslash (pathtmp, pathtmp);

  debug_printf ("%s[%s]", path, pathtmp);

  for (int i = 0; i < nmounts; i++)
    {
      /* Delete if paths and mount locations match. */
      if (((strcmp (mount[i].posix_path, pathtmp) == 0
	    || strcmp (mount[i].native_path, pathtmp) == 0)) &&
	  ((mount[i].flags & MOUNT_SYSTEM) == (flags & MOUNT_SYSTEM)))
	{
	  /* Delete by emptying mount point in question, then sorting
	     the mount table, which will put the empty one to the end.
	     Inefficient but simple.  */
	  mount[i].init ("", "", 0);
	  sort ();
	  nmounts--;
	  return 0;
	}
    }
  set_errno (EINVAL);
  return -1;
}

/* read_v1_mounts: Given a reg_key to an old mount table registry area,
   read in the mounts.  The "which" arg contains zero if we're reading
   the user area and MOUNT_SYSTEM if we're reading the system area.
   This way we can store the mounts read in the appropriate place when
   they are written back to the new registry layout. */

void
mount_info::read_v1_mounts (reg_key r, const int which)
{
  int mountflags = 0;

  /* MAX_MOUNTS was 30 when we stopped using the v1 layout */
  for (int i = 0; i < 30; i++)
    {
      char key_name[10];
      char win32path[MAX_PATH];
      char unixpath[MAX_PATH];

      __small_sprintf (key_name, "%02x", i);

      reg_key k (r.get_key (), KEY_ALL_ACCESS, key_name, NULL);

      /* The registry names are historical but useful so are left alone.  */
      k.get_string ("native", win32path, sizeof (win32path), "");
      k.get_string ("unix", unixpath, sizeof (unixpath), "");

      /* Does this entry contain something?  */
      if (*win32path != 0)
	{
	  mountflags = 0;

	  if (k.get_int ("fbinary", 0))
	    mountflags |= MOUNT_BINARY;

	  /* Or in zero or MOUNT_SYSTEM depending on which table
	     we're reading. */
	  mountflags |= which;

	  cygwin_shared->mount.add_item (win32path, unixpath, mountflags);
	}
    }
}

/* from_v1_registry: Build the entire mount table from the old v1 registry
   mount area.  */

void
mount_info::from_v1_registry ()
{
  reg_key r (HKEY_CURRENT_USER, KEY_ALL_ACCESS,
	     "SOFTWARE",
	     "Cygnus Solutions",
	     "CYGWIN.DLL setup",
	     "b15.0",
	     "mounts",
	     NULL);

  nmounts = 0;

  /* First read mounts from user's table. */
  read_v1_mounts (r, 0);

  /* Then read mounts from system-wide mount table. */
  reg_key r1 (HKEY_LOCAL_MACHINE, KEY_ALL_ACCESS,
	      "SOFTWARE",
	      "Cygnus Solutions",
	      "CYGWIN.DLL setup",
	      "b15.0",
	      "mounts",
	      NULL);
  read_v1_mounts (r1, MOUNT_SYSTEM);

  /* Note: we don't need to sort internal table here since it is
     done in main from_registry call after this function would be
     run. */
}

/* import_v1_mounts: If v1 mounts are present, load them and write
   the new entries to the new registry area. */

void mount_info::import_v1_mounts ()
{
  /* Read in old mounts into memory. */
  from_v1_registry ();

  /* Write all mounts to the new registry. */
  to_registry ();
}

/* to_registry: For every mount point in memory, add a corresponding
   registry mount point. */

void mount_info::to_registry ()
{
  for (int i = 0; i < MAX_MOUNTS; i++)
    {
      if (i < nmounts)
	{
	  mount_item *p = mount + i;

	  add_reg_mount (p->native_path, p->posix_path, p->flags);

	  debug_printf ("%02x: %s, %s, %d",
			i, p->native_path, p->posix_path, p->flags);
	}
    }
}

/************************* mount_item class ****************************/

struct mntent *
mount_item::getmntent ()
{
#ifdef _MT_SAFE
  struct mntent &ret=_reent_winsup()->_ret;
#else
  static NO_COPY struct mntent ret;
#endif

  /* Pass back pointers to mount_info strings reserved for use by
     getmntent rather than pointers to strings in the internal mount
     table because the mount table might change, causing weird effects
     from the getmntent user's point of view. */

  strcpy (cygwin_shared->mount.mnt_fsname, native_path);
  ret.mnt_fsname = cygwin_shared->mount.mnt_fsname;
  strcpy (cygwin_shared->mount.mnt_dir, posix_path);
  ret.mnt_dir = cygwin_shared->mount.mnt_dir;

  if ((flags & MOUNT_SYSTEM) == 0)       /* user mount */
    strcpy (cygwin_shared->mount.mnt_type, (char *) "user");
  else                                   /* system mount */
    strcpy (cygwin_shared->mount.mnt_type, (char *) "system");

  if ((flags & MOUNT_AUTO) != 0)         /* automount */
    strcat (cygwin_shared->mount.mnt_type, (char *) ",auto");

  ret.mnt_type = cygwin_shared->mount.mnt_type;

  /* mnt_opts is a string that details mount params such as
     binary or textmode, or exec.  We don't print
     `silent' here; it's a magic internal thing. */

  if (! (flags & MOUNT_BINARY))
    strcpy (cygwin_shared->mount.mnt_opts, (char *) "textmode");
  else
    strcpy (cygwin_shared->mount.mnt_opts, (char *) "binmode");

#if 0
  if (flags & MOUNT_EXEC)
    strcat (cygwin_shared->mount.mnt_opts, (char *) ",exec");
#endif

  ret.mnt_opts = cygwin_shared->mount.mnt_opts;

  ret.mnt_freq = 1;
  ret.mnt_passno = 1;
  return &ret;
}

/* Fill in the fields of a mount table entry.  */

void
mount_item::init (const char *native, const char *posix, int mountflags)
{
  strcpy ((char *) native_path, native);
  strcpy ((char *) posix_path, posix);

  native_pathlen = strlen (native_path);
  posix_pathlen = strlen (posix_path);

  flags = mountflags;
}

/********************** Mount System Calls **************************/

/* Mount table system calls.
   Note that these are exported to the application.  */

/* mount: Add a mount to the mount table in memory and to the registry
   that will cause paths under win32_path to be translated to paths
   under posix_path. */

extern "C"
int
mount (const char *win32_path, const char *posix_path, int flags)
{
  int res = -1;

  if ((flags & MOUNT_AUTO) == 0) /* normal mount */
    {
      res = cygwin_shared->mount.add_reg_mount (win32_path, posix_path,
						flags);

      if (res == 0)
	cygwin_shared->mount.add_item (win32_path, posix_path, flags);

      syscall_printf ("%d = mount (%s, %s, %p)", res,
		      win32_path, posix_path, flags);
    }
  else
    {    /* When flags include MOUNT_AUTO, take this to mean that
	    we actually want to change the automount prefix and flags
	    without actually mounting anything. */
      res = cygwin_shared->mount.write_automount_info_to_registry (posix_path, flags);

      if (res == 0)
	{
	  strcpy (cygwin_shared->mount.automount_prefix, posix_path);
	  cygwin_shared->mount.automount_flags = flags;
	}

      syscall_printf ("%d = mount (NULL, %s, %p)", res, posix_path, flags);
    }

  return res;
}

/* umount: The standard umount call only has a path parameter.  Since
   it is not possible for this call to specify whether to remove the
   mount from the user or global mount registry table, assume the user
   table. */

extern "C"
int
umount (const char *path)
{
  return cygwin_umount (path, 0);
}

/* cygwin_umount: This is like umount but takes an additional flags
   parameter that specifies whether to umount from the user or system-wide
   registry area. */

extern "C"
int
cygwin_umount (const char *path, const int flags)
{
  int res = cygwin_shared->mount.del_reg_mount (path, flags);

  if (res == 0)
    cygwin_shared->mount.del_item (path, flags);

  syscall_printf ("%d = cygwin_umount (%s, %d)", res,  path, flags);
  return res;
}

#ifdef _MT_SAFE
#define iteration _reent_winsup()->_iteration
#else
static int iteration;
#endif

extern "C"
FILE *
setmntent (const char *filep, const char *)
{
  iteration = 0;
  return (FILE *) filep;
}

extern "C"
struct mntent *
getmntent (FILE *)
{
  return cygwin_shared->mount.getmntent (iteration++);
}

extern "C"
int
endmntent (FILE *)
{
  return 1;
}

/********************** Symbolic Link Support **************************/

/* Create a symlink from FROMPATH to TOPATH. */

extern "C"
int
symlink (const char *topath, const char *frompath)
{
  int fd;
  int res = -1;

  syscall_printf ("symlink (%s, %s)", topath, frompath);

  if (topath[0] == 0)
    {
      set_errno (EINVAL);
      goto done;
    }
  if (strlen (topath) >= MAX_PATH)
    {
      set_errno (ENAMETOOLONG);
      goto done;
    }

  fd = _open (frompath, O_WRONLY | O_CREAT | O_BINARY, 0);
  if (fd >= 0)
    {
      char buf[sizeof (SYMLINK_COOKIE) + MAX_PATH + 10];

      __small_sprintf (buf, "%s%s", SYMLINK_COOKIE, topath);
      int len = strlen (buf) + 1;

      /* Note that the terminating nul is written.  */
      if (_write (fd, buf, len) != len)
	{
	  int saved_errno = get_errno ();
	  _close (fd);
	  _unlink (frompath);
	  set_errno (saved_errno);
	}
      else
	{
	  _close (fd);
	  chmod (frompath, S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO);
	  res = 0;
	}
    }

done:
  syscall_printf ("%d = symlink (%s, %s)", res, topath, frompath);
  return res;
}

static __inline char *
has_suffix (const char *path, const suffix_info *suffixes)
{
  char *ext = strrchr (path, '.');
  if (ext)
    for (const suffix_info *ex = suffixes; ex->name != NULL; ex++)
      if (strcasematch (ext, ex->name))
	return ext;
  return NULL;
}

static int __inline
next_suffix (char *ext_here, const suffix_info *&suffixes)
{
  if (!suffixes)
    return 1;

  while (suffixes && suffixes->name)
    if (!suffixes->addon)
      suffixes++;
    else
      {
	strcpy (ext_here, suffixes->name);
	suffixes++;
	return 1;
      }
  return 0;
}

/* Check if PATH is a symlink.  PATH must be a valid Win32 path name.

   If PATH is a symlink, put the value of the symlink--the file to
   which it points--into BUF.  The value stored in BUF is not
   necessarily null terminated.  BUFLEN is the length of BUF; only up
   to BUFLEN characters will be stored in BUF.  BUF may be NULL, in
   which case nothing will be stored.

   Set *SYML if PATH is a symlink.

   Set *EXEC if PATH appears to be executable.  This is an efficiency
   hack because we sometimes have to open the file anyhow.  *EXEC will
   not be set for every executable file.

   Return -1 on error, 0 if PATH is not a symlink, or the length
   stored into BUF if PATH is a symlink.  */

static int
symlink_check_one (const char *in_path, char *buf, int buflen, DWORD& fileattr,
		   int *exec, const suffix_info *suffixes, char *&known_suffix)
{
  HANDLE h;
  int res = 0;
  char extbuf[buflen + 5];
  char *ext_here;
  const char *path = in_path;

  if (!suffixes)
    ext_here = NULL;
  else if ((known_suffix = has_suffix (in_path, suffixes)) != NULL)
    {
      suffixes = NULL;
      ext_here = NULL;
    }
  else
    {
      path = strcpy (extbuf, in_path);
      ext_here = strchr (path, '\0');
    }

  *buf = '\0';
  do
    {
      if (!next_suffix (ext_here, suffixes))
	break;
      fileattr = GetFileAttributesA (path);
      if (fileattr == (DWORD) -1)
	{
	  /* The GetFileAttributesA call can fail for reasons that don't
	     matter, so we just return 0.  For example, getting the
	     attributes of \\HOST will typically fail.  */
	  debug_printf ("GetFileAttributesA (%s) failed", path);
	  __seterrno ();
	  continue;
	}

      /* Windows allows path\. even when `path' isn't a directory.
	 Detect this scenario and disallow it, since it is non-UNIX like. */
      char *p = strchr (path, '\0');
      if (p > path + 1 && p[-1] == '.' && SLASH_P (p[-2]) &&
	  !(fileattr & FILE_ATTRIBUTE_DIRECTORY))
	{
	  debug_printf ("\\. specified on non-directory");
	  set_errno (ENOTDIR);
	  return 0;
	}

      /* A symlink will have the `system' file attribute. */
      /* Only files can be symlinks (which can be symlinks to directories). */
      if (!SYMLINKATTR (fileattr))
	goto file_not_symlink;

      /* Check the file's extended attributes, if it has any.  */
      int unixattr;

      if (get_file_attribute (path, &unixattr) > 0)
	{
	  if ((unixattr & STD_XBITS) != 0)
	    *exec = 1;
	  if (! S_ISLNK (unixattr))
	    ;
	}

      /* Open the file.  */

      h = CreateFileA (path, GENERIC_READ, FILE_SHARE_READ, &sec_none_nih, OPEN_EXISTING,
		       FILE_ATTRIBUTE_NORMAL, 0);

      res = -1;
      if (h == INVALID_HANDLE_VALUE)
	__seterrno ();
      else
	{
	  char cookie_buf[sizeof (SYMLINK_COOKIE) - 1];
	  DWORD got;

	  if (! ReadFile (h, cookie_buf, sizeof (cookie_buf), &got, 0))
	    set_errno (EIO);
	  else if (got == sizeof (cookie_buf)
		   && memcmp (cookie_buf, SYMLINK_COOKIE,
			      sizeof (cookie_buf)) == 0)
	    {
	      /* It's a symlink.  */
	      *exec = 0;

	      res = ReadFile (h, buf, buflen, &got, 0);
	      if (!res)
		set_errno (EIO);
	      else
		{
		  /* Versions prior to b16 stored several trailing
		     NULs with the path (to fill the path out to 1024
		     chars).  Current versions only store one trailing
		     NUL.  The length returned is the path without
		     *any* trailing NULs.  We also have to handle (or
		     at least not die from) corrupted paths.  */
		  if (memchr (buf, 0, got) != NULL)
		    res = strlen (buf);
		  else
		    res = got;
		}
	    }
	  else
	    {
	      /* Not a symlink, see if executable.  */
	      *exec = (got >= 2
		       && cookie_buf[0] == '#'
		       && cookie_buf[1] == '!');
	      CloseHandle (h);
	      goto file_not_symlink;
	    }
	}
      CloseHandle (h);
      break;
    }
  while (suffixes);
  goto out;

file_not_symlink:
  set_errno (EINVAL);
  if (ext_here)
    strcpy (buf, ext_here);
  res = 0;

out:
  if (res >= 0)
    syscall_printf ("%d = symlink_check_one (%s, %p, %d) (%s)",
		    res, path, buf, buflen,
		    res > 0 ? "symlink"
		    : *exec ? "executable"
		    : "normal file");
  else
    syscall_printf ("%d = symlink_check_one (%s, %p, %d)",
		    res, path, buf, buflen);

  return res;
}

/* readlink system call */

extern "C"
int
readlink (const char *path, char *buf, int buflen)
{
  path_conv pathbuf (path, SYMLINK_CONTENTS);
  if (pathbuf.error)
    {
      set_errno (pathbuf.error);
      syscall_printf ("-1 = readlink (%s, %p, %d)", path, buf, buflen);
      return -1;
    }

  if (!pathbuf.symlink_p)
    {
      set_errno (EINVAL);
      return -1;
    }

  int len = strlen (pathbuf.get_win32 ());
  if (len > (buflen - 1))
    {
      set_errno (ENAMETOOLONG);
      return -1;
    }
  memcpy (buf, pathbuf.get_win32 (), len);
  buf[len] = '\0';

  /* errno set by symlink_check_one if error */
  return len;
}

/******************** Directory-related Support **************************/

/* Cache getcwd value.  FIXME: We need a lock for these in order to
   support multiple threads.  */

#ifdef _MT_SAFE
#define current_directory_name  _reent_winsup()->_current_directory_name
#define current_directory_posix_name _reent_winsup()->_current_directory_posix_name
#define current_directory_hash _reent_winsup()->_current_directory_hash
#else
static char *current_directory_name;
static char *current_directory_posix_name;
static unsigned long current_directory_hash;
#endif

/* Some programs rely on st_dev/st_ino being unique for each file.
   Hash the path name and hope for the best.  The hash arg is not
   always initialized to zero since readdir needs to compute the
   dirent ino_t based on a combination of the hash of the directory
   done during the opendir call and the hash or the filename within
   the directory.  FIXME: Not bullet-proof. */
/* Cygwin internal */

unsigned long
hash_path_name (unsigned long hash, const char *name)
{
  if (!*name)
    return hash;

  /* Perform some initial permutations on the pathname if this is
     not "seeded" */
  if (!hash)
    {
      /* Simplistic handling of drives.  If there is a drive specified,
	 make sure that the initial letter is upper case.  If there is
	 no \ after the ':' assume access through the root directory
	 of that drive.
	 FIXME:  Should really honor MS-Windows convention of using
	 the environment to track current directory on various drives. */
      if (name[1] == ':')
	{
	  char *nn, *newname = (char *) alloca (strlen (name) + 2);
	  nn = strncpy (newname, name, 2);
	  if (islower (*nn))
	    *newname = toupper (*nn);
	  *(nn += 2) = '\0';
	  name += 2;
	  if (*name != '\\')
	    {
	      *nn = '\\';
	      *++nn = '\0';
	    }
	  strcpy (nn, name);
	  name = newname;
	  goto hashit;
	}

      /* Fill out the hashed path name with the current working directory if
	 this is not an absolute path and there is no pre-specified hash value.
	 Otherwise the inodes same will differ depending on whether a file is
	 referenced with an absolute value or relatively. */

      if (*name != '\\' && (current_directory_name == NULL ||
			    get_current_directory_name ()))
	{
	  hash = current_directory_hash;
	  if (name[0] == '.' && name[1] == '\0')
	    return hash;
	  hash = hash_path_name (hash, "\\");
	}
    }

hashit:
  /* Build up hash.  Ignore single trailing slash or \a\b\ != \a\b or
     \a\b\.  but allow a single \ if that's all there is. */
  do
    {
      hash += *name + (*name << 17);
      hash ^= hash >> 2;
    }
  while (*++name != '\0' &&
	 !(*name == '\\' && (!name[1] || (name[1] == '.' && !name[2]))));
  return hash;
}

static int
get_current_directory_name ()
{
  DWORD dlen, len;

  for (dlen = 256; ; dlen *= 2)
    {
      current_directory_name = (char *) realloc (current_directory_name, dlen);
      if ((len = GetCurrentDirectoryA (dlen, current_directory_name)) < dlen)
	break;
    }

  if (len == 0)
    __seterrno ();
  else
    current_directory_hash = hash_path_name (0, current_directory_name);

  return len;
}

/* getcwd */

static char *
getcwd_inner (char *buf, size_t ulen, int posix_p)
{
  char *resbuf = NULL;
  size_t len = ulen;

  if (current_directory_name == NULL && !get_current_directory_name ())
    return NULL;

  if (!posix_p)
    {
      if (strlen (current_directory_name) >= len)
	set_errno (ERANGE);
      else
	{
	  strcpy (buf, current_directory_name);
	  resbuf = buf;
	}

      syscall_printf ("%p (%s) = getcwd_inner (%p, %d, win32) (cached)",
		      resbuf, resbuf ? resbuf : "", buf, len);
      return resbuf;
    }
  else if (current_directory_posix_name != NULL)
    {
      if (strlen (current_directory_posix_name) >= len)
	set_errno (ERANGE);
      else
	{
	  strcpy (buf, current_directory_posix_name);
	  resbuf = buf;
	}

      syscall_printf ("%p (%s) = getcwd_inner (%p, %d, posix) (cached)",
		      resbuf, resbuf ? resbuf : "", buf, len);
      return resbuf;
    }

  /* posix_p required and current_directory_posix_name == NULL */

  char temp[MAX_PATH];

  /* Turn from Win32 style to our style.  */
  cygwin_shared->mount.conv_to_posix_path (current_directory_name, temp, 0);

  size_t tlen = strlen (temp);

  current_directory_posix_name = (char *) realloc (
				  current_directory_posix_name, tlen + 1);
  if (current_directory_posix_name != NULL)
    strcpy (current_directory_posix_name, temp);

  if (tlen >= ulen)
    {
      /* len was too small */
      set_errno (ERANGE);
    }
  else
    {
      strcpy (buf, temp);
      resbuf = buf;
    }

  syscall_printf ("%p (%s) = getcwd_inner (%p, %d, %s)",
		  resbuf, resbuf ? resbuf : "",
		  buf, len, posix_p ? "posix" : "win32");
  return resbuf;
}

char *
getcwd (char *buf, size_t ulen)
{
  char *res;

  if (buf == NULL || ulen == 0)
    {
      buf = (char *) alloca (MAX_PATH);
      res = getcwd_inner (buf, MAX_PATH, 1);
      res = strdup (buf);
    }
  else
    {
      res = getcwd_inner (buf, ulen, 1);
    }

  return res;
}

/* getwd: standards? */
extern "C"
char *
getwd (char *buf)
{
  return getcwd (buf, MAX_PATH);
}

/* chdir: POSIX 5.2.1.1 */
extern "C"
int
chdir (const char *dir)
{
  path_conv path (dir);

  if (path.error)
    {
      set_errno (path.error);
      syscall_printf ("-1 = chdir (%s)", dir);
      return -1;
    }

  char *native_dir = path.get_win32 ();

  /* Check to see if path translates to something like C:.
     If it does, append a \ to the native directory specification to
     defeat the Windows 95 (i.e. MS-DOS) tendency of returning to
     the last directory visited on the given drive. */
  if (isalpha (native_dir[0]) && native_dir[1] == ':' && !native_dir[2])
    {
      native_dir[2] = '\\';
      native_dir[3] = '\0';
    }
  int res = SetCurrentDirectoryA (native_dir);
  if (!res)
    __seterrno ();

  sig_protect (here, 1);

  /* Clear the cache until we need to retrieve the directory again.  */
  if (current_directory_name != NULL)
    {
      free (current_directory_name);
      current_directory_name = NULL;
    }
  if (current_directory_posix_name != NULL)
    {
      free (current_directory_posix_name);
      current_directory_posix_name = NULL;
    }

  syscall_printf ("%d = chdir (%s) (dos %s)", res ? 0 : -1, dir, native_dir);
  return res ? 0 : -1;
}

/******************** Exported Path Routines *********************/

/* Cover functions to the path conversion routines.
   These are exported to the world as cygwin_foo by cygwin.din.  */

extern "C"
int
cygwin_conv_to_win32_path (const char *path, char *win32_path)
{
  path_conv p (path, SYMLINK_FOLLOW, 0);
  if (p.error)
    {
      set_errno (p.error);
      return -1;
    }

  strcpy (win32_path, p.get_win32 ());
  return 0;
}

extern "C"
int
cygwin_conv_to_full_win32_path (const char *path, char *win32_path)
{
  path_conv p (path, SYMLINK_FOLLOW, 1);
  if (p.error)
    {
      set_errno (p.error);
      return -1;
    }

  strcpy (win32_path, p.get_win32 ());
  return 0;
}

/* This is exported to the world as cygwin_foo by cygwin.din.  */

extern "C"
int
cygwin_conv_to_posix_path (const char *path, char *posix_path)
{
  if (check_null_empty_path_errno (path))
    return -1;
  cygwin_shared->mount.conv_to_posix_path (path, posix_path, 1);
  return 0;
}

extern "C"
int
cygwin_conv_to_full_posix_path (const char *path, char *posix_path)
{
  if (check_null_empty_path_errno (path))
    return -1;
  cygwin_shared->mount.conv_to_posix_path (path, posix_path, 0);
  return 0;
}

/* The realpath function is supported on some UNIX systems.  */

extern "C"
char *
realpath (const char *path, char *resolved)
{
  int err;

  path_conv real_path (path, SYMLINK_FOLLOW, 1);

  if (real_path.error)
    err = real_path.error;
  else
    {
      err = cygwin_shared->mount.conv_to_posix_path (real_path.get_win32 (), resolved, 0);
      if (err == 0)
	return resolved;
    }

  /* FIXME: on error, we are supposed to put the name of the path
     component which could not be resolved into RESOLVED.  */
  resolved[0] = '\0';

  set_errno (err);
  return NULL;
}

/* Return non-zero if path is a POSIX path list.
   This is exported to the world as cygwin_foo by cygwin.din.

DOCTOOL-START
<sect1 id="add-func-cygwin-posix-path-list-p">
  <para>Rather than use a mode to say what the "proper" path list
  format is, we allow any, and give apps the tools they need to
  convert between the two.  If a ';' is present in the path list it's
  a Win32 path list.  Otherwise, if the first path begins with
  [letter]: (in which case it can be the only element since if it
  wasn't a ';' would be present) it's a Win32 path list.  Otherwise,
  it's a POSIX path list.</para>
</sect1>
DOCTOOL-END
  */

extern "C"
int
cygwin_posix_path_list_p (const char *path)
{
  int posix_p = ! (strchr (path, ';')
		   || (isalpha (path[0]) && path[1] == ':'));
  return posix_p;
}

/* These are used for apps that need to convert env vars like PATH back and
   forth.  The conversion is a two step process.  First, an upper bound on the
   size of the buffer needed is computed.  Then the conversion is done.  This
   allows the caller to use alloca if it wants.  */

static int
conv_path_list_buf_size (const char *path_list, int to_posix_p)
{
  int i, num_elms, max_mount_path_len, size;
  const char *p;

  /* The theory is that an upper bound is
     current_size + (num_elms * max_mount_path_len)  */

  char delim = to_posix_p ? ';' : ':';
  p = path_list;
  for (num_elms = 1; (p = strchr (p, delim)) != NULL; ++num_elms)
    ++p;

  /* 7: strlen ("//c") + slop, a conservative initial value */
  for (max_mount_path_len = 7, i = 0; i < cygwin_shared->mount.nmounts; ++i)
    {
      int mount_len = (to_posix_p
		       ? cygwin_shared->mount.mount[i].posix_pathlen
		       : cygwin_shared->mount.mount[i].native_pathlen);
      if (max_mount_path_len < mount_len)
	max_mount_path_len = mount_len;
    }

  /* 100: slop */
  size = strlen (path_list) + (num_elms * max_mount_path_len) + 100;
  return size;
}

extern "C"
int
cygwin_win32_to_posix_path_list_buf_size (const char *path_list)
{
  return conv_path_list_buf_size (path_list, 1);
}

extern "C"
int
cygwin_posix_to_win32_path_list_buf_size (const char *path_list)
{
  return conv_path_list_buf_size (path_list, 0);
}

extern "C"
int
cygwin_win32_to_posix_path_list (const char *win32, char *posix)
{
  conv_path_list (win32, posix, 1);
  return 0;
}

extern "C"
int
cygwin_posix_to_win32_path_list (const char *posix, char *win32)
{
  conv_path_list (posix, win32, 0);
  return 0;
}

/* cygwin_split_path: Split a path into directory and file name parts.
   Buffers DIR and FILE are assumed to be big enough.

   Examples (path -> `dir' / `file'):
   / -> `/' / `'
   "" -> `.' / `'
   . -> `.' / `.' (FIXME: should this be `.' / `'?)
   .. -> `.' / `..' (FIXME: should this be `..' / `'?)
   foo -> `.' / `foo'
   foo/bar -> `foo' / `bar'
   foo/bar/ -> `foo' / `bar'
   /foo -> `/' / `foo'
   /foo/bar -> `/foo' / `bar'
   c: -> `c:/' / `'
   c:/ -> `c:/' / `'
   c:foo -> `c:/' / `foo'
   c:/foo -> `c:/' / `foo'
 */

extern "C"
void
cygwin_split_path (const char *path, char *dir, char *file)
{
  int dir_started_p = 0;

  /* Deal with drives.
     Remember that c:foo <==> c:/foo.  */
  if (isalpha (path[0]) && path[1] == ':')
    {
      *dir++ = *path++;
      *dir++ = *path++;
      *dir++ = '/';
      if (! *path)
	{
	  *dir = 0;
	  *file = 0;
	  return;
	}
      if (SLASH_P (*path))
	++path;
      dir_started_p = 1;
    }

  /* Determine if there are trailing slashes and "delete" them if present.
     We pretend as if they don't exist.  */
  const char *end = path + strlen (path);
  /* path + 1: keep leading slash.  */
  while (end > path + 1 && SLASH_P (end[-1]))
    --end;

  /* At this point, END points to one beyond the last character
     (with trailing slashes "deleted").  */

  /* Point LAST_SLASH at the last slash (duh...).  */
  const char *last_slash;
  for (last_slash = end - 1; last_slash >= path; --last_slash)
    if (SLASH_P (*last_slash))
      break;

  if (last_slash == path)
    {
      *dir++ = '/';
      *dir = 0;
    }
  else if (last_slash > path)
    {
      memcpy (dir, path, last_slash - path);
      dir[last_slash - path] = 0;
    }
  else
    {
      if (dir_started_p)
	; /* nothing to do */
      else
	*dir++ = '.';
      *dir = 0;
    }

  memcpy (file, last_slash + 1, end - last_slash - 1);
  file[end - last_slash - 1] = 0;
}

/********************** String Helper Functions ************************/

#define CHXOR ('a' ^ 'A')
#define ch_case_eq(ch1, ch2) \
    ({ \
      unsigned char x; \
      !((x = ((unsigned char)ch1 ^ (unsigned char)ch2)) && \
       (x != CHXOR || !isalpha (ch1))); \
    })

int
strncasematch (const char *s1, const char *s2, size_t n)
{
  if (s1 == s2)
    return 1;

  n++;
  while (--n && *s1)
    {
      if (!ch_case_eq (*s1, *s2))
	return 0;
      s1++; s2++;
    }
  return !n || *s2 == '\0';
}

int
strcasematch (const char *s1, const char *s2)
{
  if (s1 == s2)
    return 1;

  while (*s1)
    {
      if (!ch_case_eq (*s1, *s2))
	return 0;
      s1++; s2++;
    }
  return *s2 == '\0';
}

char *
strcasestr (const char *searchee, const char *lookfor)
{
  if (*searchee == 0)
    {
      if (*lookfor)
	return NULL;
      return (char *) searchee;
    }

  while (*searchee)
    {
      int i = 0;
      while (1)
	{
	  if (lookfor[i] == 0)
	    return (char *) searchee;

	  if (!ch_case_eq (lookfor[i], searchee[i]))
	    break;
	  lookfor++;
	}
      searchee++;
    }

  return NULL;
}
