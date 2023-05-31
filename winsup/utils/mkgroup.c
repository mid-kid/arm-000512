/* mkgroup.c:

   Copyright 1997, 1998 Cygnus Solutions.

   This file is part of Cygwin.

   This software is a copyrighted work licensed under the terms of the
   Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
   details. */

#include <ctype.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#ifndef __GNUC__
#include <lmaccess.h>
#include <lmapibuf.h>
#endif

#include <stdio.h>

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

void 
psx_dir (char *in, char *out)
{
  if (isalpha (in[0]) && in[1] == ':')
    {
      sprintf (out, "//%c", in[0]);
      in += 2;
      out += 3;
    }

  while (*in)
    {
      if (*in == '\\')
	*out = '/';
      else
	*out = *in;
      in++;
      out++;
    }

  *out = '\0';
}

void 
uni2ansi (LPWSTR wcs, char *mbs)
{
  if (wcs)
    wcstombs (mbs, wcs, (wcslen (wcs) + 1) * sizeof (WCHAR));

  else
    *mbs = '\0';
}

void 
enum_groups (LPWSTR servername)
{
  GROUP_INFO_2 *buffer;
  DWORD entriesread = 0;
  DWORD totalentries = 0;
  DWORD resume_handle = 0;

  do
    {
      DWORD i;
      DWORD rc = NetGroupEnum (servername, 2, (LPBYTE *) & buffer, 1024,
			       &entriesread, &totalentries, &resume_handle);

      switch (rc)
	{
	case ERROR_ACCESS_DENIED:
	  fprintf (stderr, "Access denied\n");
	  exit (1);

	case ERROR_MORE_DATA:
	case ERROR_SUCCESS:
	  break;

	default:
	  fprintf (stderr, "NetUserEnum() failed with %d\n", rc);
	  exit (1);
	}

      for (i = 0; i < entriesread; i++)
	{
	  char groupname[100];
	  int gid = buffer[i].grpi2_group_id;
	  uni2ansi (buffer[i].grpi2_name, groupname);
	  printf ("%s::%d:\n", groupname, gid);
	}

      NetApiBufferFree (buffer);

    }
  while (entriesread < totalentries);

  if (servername)
    NetApiBufferFree (servername);
}

void 
usage ()
{
  fprintf (stderr, "\n");
  fprintf (stderr, "usage: mkgroup <options> [domain]\n\n");
  fprintf (stderr, "This program prints group information to stdout\n\n");
  fprintf (stderr, "Options:\n");
  fprintf (stderr, "    -l,--local           print pseudo group information if there is\n");
  fprintf (stderr, "                         no domain\n");
  fprintf (stderr, "    -d,--domain          print global group information from the domain\n");
  fprintf (stderr, "                         specified (or from the current domain if there is\n");
  fprintf (stderr, "                         no domain specified)\n");
  fprintf (stderr, "    -?,--help            print this message\n\n");
  exit (1);
}

int 
main (int argc, char **argv)
{
  LPWSTR servername;
  DWORD rc = ERROR_SUCCESS;
  WCHAR domain_name[100];
  int print_local = 0;
  int print_domain = 0;
  int domain_specified = 0;
  int i;

  if (argc == 1)
    usage ();

  else
    {
      for (i = 1; i < argc; i++)
	{
	  if (!strcmp (argv[i], "-l") || !strcmp (argv[i], "--local"))
	    print_local = 1;

	  else if (!strcmp (argv[i], "-d") || !strcmp (argv[i], "--domain"))
	    print_domain = 1;

	  else if (!strcmp (argv[i], "-?") || !strcmp (argv[i], "--help"))
	    usage ();

	  else
	    {
	      mbstowcs (domain_name, argv[1], (strlen (argv[1]) + 1) * sizeof (WCHAR));
	      domain_specified = 1;
	    }
	}
    }
  if (print_domain)
    {
      if (domain_specified)
	rc = NetGetDCName (NULL, domain_name, (LPBYTE *) & servername);

      else
	rc = NetGetDCName (NULL, NULL, (LPBYTE *) & servername);

      if (rc != ERROR_SUCCESS)
	{
	  fprintf (stderr, "Cannot get PDC, code = %d\n", rc);
	  exit (1);
	}

      enum_groups (servername);
    }

  if (print_local)
    enum_groups (NULL);

  printf ("Everyone::0:\n");

  return 0;
}
