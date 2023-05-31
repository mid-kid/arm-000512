/* exec.cc: exec system call support.

   Copyright 1996, 1997, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <process.h>
#include "winsup.h"

/* This is called _execve and not execve because the real execve is defined
   in libc/posix/execve.c.  It calls us.  */

extern "C"
pid_t
_execve (const char *path, const char *const argv[], const char *const envp[])
{
  return sexecve (NULL, path, argv, envp);
}

extern "C"
int
execl (const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char *argv[1024];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;
  do
      argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);
  va_end (args);

  return _execve (path, (char * const  *) argv, *user_data->envptr);
}

extern "C"
int
execv (const char *path, char * const *argv)
{
  return _execve (path, (char * const *) argv, *user_data->envptr);
}

/* the same as a standard exec() calls family, but with NT security support */

extern "C"
pid_t
sexecve (HANDLE hToken, const char *path, const char *const argv[], const char *const envp[])
{
  if (path == 0)
    {
      syscall_printf ("path 0");
      set_errno(EINVAL);
      return (-1);
    }

  if (argv[0] == 0)
    {
      syscall_printf ("argv[0] is 0");
      char **tmp = (char **) &argv[0];
      *tmp = (char *) path;
    }

  if (envp == 0)
    {
      syscall_printf ("envp is 0");
      set_errno(EINVAL);
      return (-1);
    }

  syscall_printf ("_execve (%s, %s, %x)", path, argv[0], envp);

  /* We do not pass _P_SEARCH_PATH here.  execve doesn't search PATH.  */
  spawn_guts (hToken, path, argv, envp, myself, _P_OVERLAY);

  /* Errno should be set by spawn_guts.  */
  return -1;
}

extern "C"
int
sexecl (HANDLE hToken, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char *argv[1024];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
      argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  va_end (args);

  return sexecve (hToken, path, (char * const *) argv, *user_data->envptr);
}

extern "C"
int
sexecle (HANDLE hToken, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char * const *envp;
  const char *argv[1024];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  envp = va_arg (args, const char * const *);
  va_end (args);

  return sexecve(hToken, path, (char * const *) argv, (char * const *) envp);
}

extern "C"
int
sexeclp (HANDLE hToken, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char *argv[1024];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
      argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  va_end (args);

  return sexecvpe (hToken, path, (const char * const *) argv,
					      *user_data->envptr);
}

extern "C"
int
sexeclpe (HANDLE hToken, const char *path, const char *arg0, ...)
{
  int i;
  va_list args;
  const char * const *envp;
  const char *argv[1024];

  va_start (args, arg0);
  argv[0] = arg0;
  i = 1;

  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);

  envp = va_arg (args, const char * const *);
  va_end (args);

  return sexecvpe (hToken, path, argv, envp);
}

extern "C"
int
sexecv (HANDLE hToken, const char *path, const char * const *argv)
{
  return sexecve (hToken, path, argv, *user_data->envptr);
}

extern "C"
int
sexecp (HANDLE hToken, const char *path, const char * const *argv)
{
  return sexecvpe (hToken, path, argv, *user_data->envptr);
}

/*
 * Copy string, until c or <nul> is encountered.
 * NUL-terminate the destination string (s1).
 * Return pointer to terminating byte in dst string.
 */

char *
strccpy (char *s1, const char **s2, char c)
{
  while (**s2 && **s2 != c)
    *s1++ = *((*s2)++);
  *s1 = 0;

  return s1;
}

extern "C"
int
sexecvpe (HANDLE hToken, const char *file, const char * const *argv,
	  const char *const *envp)
{
  char buf[MAXNAMLEN];
  return sexecve (hToken, find_exec (file, buf), argv, envp);
}
