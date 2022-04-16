/* scandir.cc

   Copyright 1998 Cygnus Solutions.

   Written by Corinna Vinschen <corinna.vinschen@cityweb.de>

   This file is part of Cygwin.

   This software is a copyrighted work licensed under the terms of the
   Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
   details. */

#include <dirent.h>
#include <stdlib.h>

extern "C"
int
scandir (const char *dir,
	 struct dirent ***namelist,
	 int (*select) (const struct dirent *),
	 int (*compar) (const struct dirent **, const struct dirent **))
{
  DIR *dirp;
  struct dirent *ent, *etmp, **nl = NULL, **ntmp;
  int count = 0;

  if (!(dirp = opendir (dir)))
    return -1;
  while ((ent = readdir (dirp)))
    {
      if (!select || select (ent))
	{
	  if (!(etmp = (struct dirent *) malloc (sizeof *ent)))
	    goto error;
	  *etmp = *ent;
	  ntmp = (struct dirent **) realloc (nl, (count + 1) * sizeof *nl);
	  if (!ntmp)
	    {
	      free (etmp);
	      goto error;
	    }
	  nl = ntmp;
	  nl[count++] = etmp;
	}
    }
  closedir (dirp);
  qsort (nl, count, sizeof *nl, (int (*)(const void *, const void *)) compar);
  if (namelist)
    *namelist = nl;
  return count;

error:
  if (nl)
    {
      while (count > 0)
	free (nl[--count]);
      free (nl);
    }
  return -1;
}

extern "C"
int
alphasort (const struct dirent **a, const struct dirent **b)
{
  return strcmp ((*a)->d_name, (*b)->d_name);
}
