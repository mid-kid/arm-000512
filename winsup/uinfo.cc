/* uinfo.cc: user info (uid, gid, etc...)

   Copyright 1996, 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <pwd.h>
#include <unistd.h>
#include "winsup.h"
#include <utmp.h>

/* FIXME: shouldn't violate internal object space -- these two
   should be static inside grp.cc */
void read_etc_group ();
extern int group_in_memory_p;


#define DEFAULT_GID 100
#define DEFAULT_UID 500

void
uinfo_init ()
{
  struct passwd *p;

  if (myself->uid != USHRT_MAX)
    return;

  if ((p = getpwnam (getlogin ())) != NULL)
    {
      /* calling getpwnam assures us that /etc/password has been
	 read in, but we can't be sure about /etc/group */

      if (!group_in_memory_p)
	read_etc_group ();

      myself->uid = p->pw_uid;
      myself->gid = p->pw_gid;
    }
  else
    {
      myself->uid = DEFAULT_UID;
      myself->gid = DEFAULT_GID;
    }
}

extern "C" char *
getlogin (void)
{
#ifdef _MT_SAFE
  char *this_username=_reent_winsup()->_username;
#else
  static NO_COPY char this_username[MAX_USER_NAME];
#endif
  DWORD username_len = MAX_USER_NAME;

  if (GetUserName (this_username, &username_len))
    return this_username;
  else
    return strcpy (this_username, "unknown");
}

uid_t
getuid (void)
{
  return myself->uid;
}

gid_t
getgid (void)
{
  return myself->gid;
}

uid_t
geteuid (void)
{
  return getuid ();
}

gid_t
getegid (void)
{
  return getgid ();
}

/* Not quite right - cuserid can change, getlogin can't */
char *
cuserid (char *src)
{
  if (src)
    {
      strcpy (src, getlogin ());
      return src;
    }
  else
    {
      return getlogin ();
    }
}
