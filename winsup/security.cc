/* security.cc: NT security functions

   Copyright 1997, 1998 Cygnus Solutions.

   Written by Gunther Ebert, gunther.ebert@ixos-leipzig.de

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include "winsup.h"

#if 0
PSID
get_world_sid ()
{
  PSID world_sid;
  SID_IDENTIFIER_AUTHORITY world_sid_auth = SECURITY_WORLD_SID_AUTHORITY;

  world_sid = (PSID) LocalAlloc (LPTR,GetSidLengthRequired (1));

  InitializeSid (world_sid, &world_sid_auth, 1);
  *(GetSidSubAuthority (world_sid, 0)) = SECURITY_WORLD_RID;

  return world_sid;
}

int
world_full_access (HANDLE h, int type)
{
  PSID world_sid = NULL;
  PSECURITY_DESCRIPTOR psd = NULL;
  PACL pacl = NULL;
  DWORD acl_size = 1024;

  world_sid = get_world_sid();

  if (!world_sid)
    goto clean_on_error;

  psd = (PSECURITY_DESCRIPTOR) LocalAlloc (LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

  if (!psd)
    goto clean_on_error;

  if (!InitializeSecurityDescriptor (psd, SECURITY_DESCRIPTOR_REVISION))
    goto clean_on_error;

  pacl = (PACL) LocalAlloc (LPTR, acl_size);

  if (!pacl)
    goto clean_on_error;

  /* FIXME: the 3rd parameter is actually ACL_REVISION which is
     not defined in Scott Christley's header files. */
  if (!InitializeAcl (pacl, acl_size, 2))
    goto clean_on_error;

  if (!AddAccessAllowedAce (pacl, 2, type, world_sid))
    goto clean_on_error;

  if (!SetSecurityDescriptorDacl (psd, TRUE, pacl, FALSE))
    goto clean_on_error;

  if(!SetKernelObjectSecurity (h, DACL_SECURITY_INFORMATION, psd))
    goto clean_on_error;

  LocalFree ((HANDLE) world_sid);
  LocalFree ((HANDLE) psd);
  LocalFree ((HANDLE) pacl);
  return TRUE;

clean_on_error:
    if (world_sid)
	LocalFree ((HANDLE) world_sid);

    if (psd)
	LocalFree ((HANDLE) psd);

    if (pacl)
	LocalFree ((HANDLE) pacl);

    return FALSE;
}
#endif

int
get_id_from_sid (PSID psid)
{
  if (!IsValidSid (psid))
    {
      __seterrno ();
      small_printf ("IsValidSid failed with %E");
      return -1;
    }

  /*
   * FIXME: I don't know if it really works in any case, but calling
   * LookupAccountSid() and NetxxxGetInfo() has very poor performance
   * especially in domain environments
   */

  /* It seems like the uid (or gid) is the last sub authority of the sid */
  int id = *GetSidSubAuthority(psid, *GetSidSubAuthorityCount(psid) - 1);

  if ((WORD) id == (WORD) -1)
    {
      /*
       * the last subauthority is sometimes -1, we have to go the
       * 'clean way' in this case
       */

      char account[100];
      char domain[100];
      DWORD account_length = 100;
      DWORD domain_length = 100;
      SID_NAME_USE account_type;

      if (!LookupAccountSid (NULL, psid, account, &account_length,
				domain, &domain_length, &account_type))
	{
	  __seterrno ();
	  return -1;
	}

      switch (account_type)
	{
	  case SidTypeUser:
	    {
	      struct passwd *pw = getpwnam (account);
	      return pw ? pw->pw_uid : getuid ();
	    }
	  case SidTypeGroup:
	    {
	      struct group *gr = getgrnam (account);
	      if (gr == NULL)
		 {
		   /* local groups are more like users than like groups */
		   struct passwd *pw = getpwnam (account);
		   return pw ? pw->pw_uid : getuid ();
		 }
	      return gr->gr_gid;
	    }
	  default:
	    return getuid ();
	}
    }

  return id;
}

/*
 * File attribute stuff. FIXME: add NTFS security.
 */

BOOL
get_file_attribute (const char *file, int *attribute)
{
  int res = NTReadEA (file, ".UNIXATTR", (char *) attribute,
		      sizeof (*attribute));
  return res > 0;
}

BOOL
set_file_attribute (const char *file, int attribute)
{
  return NTWriteEA (file, ".UNIXATTR", (char *) &attribute,
		    sizeof (attribute));
}
