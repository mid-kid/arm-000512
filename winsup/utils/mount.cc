/* mount.cc

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <mntent.h>
#include <windows.h>
#include <sys/cygwin.h>
#include "winsup.h"
#include "external.h"

#ifdef errno
#undef errno
#endif
#include <errno.h>

static void show_mounts (void);
static void change_automount_prefix (const char *new_prefix, int flags);
static int mount_already_exists (const char *posix_path, int flags);

// static short create_missing_dirs = FALSE;
static short force = FALSE;

static void
do_mount (const char *dev, const char *where, int flags)
{
  struct stat statbuf;
  char win32_path[MAX_PATH];
  int statres;

  cygwin_conv_to_win32_path (where, win32_path);

  statres = stat (win32_path, &statbuf);

#if 0
  if (statres == -1)
    {
      /* FIXME: this'll fail if mount dir is missing any parent dirs */
      if (create_missing_dirs == TRUE)
	{
	  if (mkdir (where, 0755) == -1)
	    fprintf (stderr, "Warning: unable to create %s!\n", where);
	  else
	    statres = 0; /* Pretend stat succeeded if we could mkdir. */
	}
    }
#endif

  if (mount (dev, where, flags))
    {
      perror ("mount failed");
      exit (1);
    }

  if (statres == -1)
    {
      if (force == FALSE)
	fprintf (stderr, "Warning: %s does not exist!\n", where);
    }
  else if (!(statbuf.st_mode & S_IFDIR))
    {
      if (force == FALSE)
	fprintf (stderr, "Warning: %s is not a directory!\n", where);
    }    

  exit (0);
}

static const char *progname;

static void
usage (void)
{
  fprintf (stderr, "Usage %s [-bfs] <win32path> <posixpath>\n", progname);
  fprintf (stderr, "-b = text files are equivalent to binary files (newline = \\n)\n");
  //  fprintf (stderr, "-c = create missing mount point directories\n");
  fprintf (stderr, "-f = force mount, don't warn about missing mount point directories\n");
  fprintf (stderr, "-s = add mount point to system-wide registry location\n");
  //  fprintf (stderr, "-e = treat all files under mount point as executables\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "[-bs] --change-automount-prefix <posixpath> = change the automount path prefix to <posixpath>\n");
  fprintf (stderr, "--import-old-mounts = copy old registry mount table mounts into the current mount areas.\n");
  exit (1);
}

int
main (int argc, const char **argv)
{
  int i;
  int flags = 0;

  progname = argv[0];

  if (argc == 1)
    {
      show_mounts ();
      exit (0);
    }

  for (i = 1; i < argc; ++i)
    {
     if (argv[i][0] != '-')
	break;

     if (strcmp (argv[i], "--change-automount-prefix") == 0)
       {
	 if ((i + 2) != argc)
	   usage ();

	 change_automount_prefix (argv[i+1], flags);
       }
     else if (strcmp (argv[i], "--import-old-mounts") == 0)
       {
	 if ((i + 1) != argc)
	   usage ();

	 cygwin_internal (CW_READ_V1_MOUNT_TABLES);
	 exit (0);
       }
     else if (strcmp (argv[i], "-b") == 0)
       flags |= MOUNT_BINARY;
#if 0
     else if (strcmp (argv[i], "-c") == 0)
       create_missing_dirs = TRUE;
#endif
     else if (strcmp (argv[i], "-s") == 0)
       flags |= MOUNT_SYSTEM;
#if 0
     else if (strcmp (argv[i], "-e") == 0)
       flags |= MOUNT_EXEC;
#endif
     else if (strcmp (argv[i], "-f") == 0)
       force = TRUE;
     else
       usage ();
    }
  
  if ((i + 2) != argc)
    usage ();

  if ((force == FALSE) && (mount_already_exists (argv[i + 1], flags)))
    {
      errno = EBUSY;
      perror ("mount failed");
      exit (1);
    }
  else
    do_mount (argv[i], argv[i + 1], flags);

  /* NOTREACHED */
  return 0;
}

static void
show_mounts (void)
{
  FILE *m = setmntent ("/-not-used-", "r");
  struct mntent *p;
  const char *format = "%-18s  %-18s  %-11s  %s\n";

  printf (format, "Device", "Directory", "Type", "Flags");
  while ((p = getmntent (m)) != NULL)
    {
      printf (format,
	      p->mnt_fsname,
	      p->mnt_dir,
	      p->mnt_type,
	      p->mnt_opts);
    }
  endmntent (m);
}

/* Return 1 if mountpoint from the same registry area is already in
   mount table.  Otherwise return 0. */
static int
mount_already_exists (const char *posix_path, int flags)
{
  int found_matching = 0;

  FILE *m = setmntent ("/-not-used-", "r");
  struct mntent *p;

  while ((p = getmntent (m)) != NULL)
    {
      /* if the paths match, and they're both the same type of mount. */
      if (strcmp (p->mnt_dir, posix_path) == 0)
	{
	  if ((p->mnt_type[0] == 'u') &&
	      ((flags & MOUNT_SYSTEM) == 0)) /* both current_user */
	    {
	      found_matching = 1;
	      break;
	    }
	  else if ((p->mnt_type[0] == 's') &&
		   ((flags & MOUNT_SYSTEM) != 0)) /* both system */
	    {
	      found_matching = 1;
	      break;
	    }
	  else
	    {
	      fprintf (stderr, "mount: warning -- couldn't determine mount type!\n");
	      break;
	    }
	}
    }
  endmntent (m);

  return found_matching;
}

/* change_automount_prefix: Change the automount prefix */
static void
change_automount_prefix (const char *new_prefix, int flags)
{
  flags |= MOUNT_AUTO;

  if (mount (NULL, new_prefix, flags))
    {
      perror ("mount failed");
      exit (1);
    }
  
  exit (0);
}
