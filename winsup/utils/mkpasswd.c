/* mkpasswd.c:

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

int 
enum_users (LPWSTR servername)
{
  USER_INFO_3 *buffer;
  DWORD entriesread = 0;
  DWORD totalentries = 0;
  DWORD resume_handle = 0;

  do
    {
      DWORD i;
      DWORD rc = NetUserEnum (servername, 3, FILTER_NORMAL_ACCOUNT,
			      (LPBYTE *) & buffer, 1024,
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
	  char username[100];
	  char fullname[100];
	  char homedir_psx[MAX_PATH];
	  char homedir_w32[MAX_PATH];
	  int uid = buffer[i].usri3_user_id;
	  int gid = buffer[i].usri3_primary_group_id;
	  uni2ansi (buffer[i].usri3_name, username);
	  uni2ansi (buffer[i].usri3_full_name, fullname);
	  uni2ansi (buffer[i].usri3_home_dir, homedir_w32);
	  psx_dir (homedir_w32, homedir_psx);
	  printf ("%s::%d:%d:%s:%s:/bin/sh\n", username, uid, gid,
		  fullname, homedir_psx);
	}

      NetApiBufferFree (buffer);

    }
  while (entriesread < totalentries);

  if (servername)
    NetApiBufferFree (servername);

  return 0;
}

int 
enum_local_groups ()
{
  LOCALGROUP_INFO_0 *buffer;
  DWORD entriesread = 0;
  DWORD totalentries = 0;
  DWORD resume_handle = 0;

  do
    {
      DWORD i;
      DWORD rc = NetLocalGroupEnum (NULL, 0, (LPBYTE *) & buffer, 1024,
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
	  char localgroup_name[100];
	  char domain_name[100];
	  DWORD domname_len = 100;
	  char psid_buffer[1024];
	  PSID psid = (PSID) psid_buffer;
	  DWORD sid_length = 1024;
	  DWORD gid;
	  SID_NAME_USE acc_type;
	  uni2ansi (buffer[i].lgrpi0_name, localgroup_name);

	  if (!LookupAccountName (NULL, localgroup_name, psid,
				  &sid_length, domain_name, &domname_len,
				  &acc_type))
	    {
	      int code = GetLastError ();
	      fprintf (stderr, "LookupAccountName(%s) failed with %d\n",
		       localgroup_name, code);
	      return 0;
	    }

	  gid = *GetSidSubAuthority (psid, 1);

	  printf ("%s::%d:0:::\n", localgroup_name, gid);
	}

      NetApiBufferFree (buffer);

    }
  while (entriesread < totalentries);

  return 0;
}

void 
usage ()
{
  fprintf (stderr, "\n");
  fprintf (stderr, "usage: mkpasswd [options] [domain]\n\n");
  fprintf (stderr, "This program prints a /etc/passwd file to stdout\n\n");
  fprintf (stderr, "Options are\n");
  fprintf (stderr, "   -l,--local              print local accounts\n");
  fprintf (stderr, "   -d,--domain             print domain accounts (from current domain\n");
  fprintf (stderr, "                           if no domain specified\n");
  fprintf (stderr, "   -g,--local-groups       print local group information too\n");
  fprintf (stderr, "   -?,--help               displays this message\n\n");
  fprintf (stderr, "This program does only work on Windows NT\n\n");
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
  int print_local_groups = 0;
  int domain_name_specified = 0;
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

	  else if (!strcmp (argv[i], "-g") || !strcmp (argv[i], "--local-groups"))
	    print_local_groups = 1;

	  else if (!strcmp (argv[i], "-?") || !strcmp (argv[i], "--help"))
	    usage ();

	  else
	    {
	      mbstowcs (domain_name, argv[1],
			((strlen (argv[1]) + 1) * sizeof (WCHAR)));
	      domain_name_specified = 1;
	    }
	}
    }

  /* FIXME: this needs to take Windows 98 into account. */
  if (GetVersion () >= 0x80000000)
    {
      fprintf (stderr, "The required functionality is not supported by Windows 95. Sorry.\n");
      exit (1);
    }

  if (print_domain)
    {
      if (domain_name_specified)
	rc = NetGetDCName (NULL, domain_name, (LPBYTE *) & servername);

      else
	rc = NetGetDCName (NULL, NULL, (LPBYTE *) & servername);

      if (rc != ERROR_SUCCESS)
	{
	  fprintf (stderr, "Cannot get PDC, code = %d\n", rc);
	  exit (1);
	}

      enum_users (servername);
    }

  if (print_local)
    enum_users (NULL);

  if (print_local_groups)
    enum_local_groups ();

  return 0;
}
