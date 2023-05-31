/* environ.cc: Cygwin-adopted functions from newlib to manipulate
   process's environment.

   Copyright 1997, 1998, 1999 Cygnus Solutions.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include "winsup.h"
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <fcntl.h>

#define environ (*user_data->envptr)

extern BOOL allow_glob;
extern BOOL allow_ntea;
extern BOOL strip_title_path;
extern DWORD chunksize;
BOOL threadsafe;
BOOL reset_com = TRUE;
static BOOL envcache = TRUE;

/* List of names which are converted from dos to unix
 * on the way in and back again on the way out.
 *
 * PATH needs to be here because CreateProcess uses it and gdb uses
 * CreateProcess.  HOME is here because most shells use it and would be
 * confused by Windows style path names.
 */
static int return_MAX_PATH (const char *) {return MAX_PATH;}
static win_env conv_envvars[] =
  {
    {"PATH=", 5, NULL, NULL, cygwin_win32_to_posix_path_list,
     cygwin_posix_to_win32_path_list,
     cygwin_win32_to_posix_path_list_buf_size,
     cygwin_posix_to_win32_path_list_buf_size},
    {"HOME=", 5, NULL, NULL, cygwin_conv_to_full_posix_path, cygwin_conv_to_full_win32_path,
     return_MAX_PATH, return_MAX_PATH},
    {"LD_LIBRARY_PATH=", 16, NULL, NULL, cygwin_conv_to_full_posix_path,
     cygwin_conv_to_full_win32_path, return_MAX_PATH, return_MAX_PATH},
    {NULL}
  };

void
win_env::add_cache (const char *in_posix, const char *in_native)
{
  posix = (char *) realloc (posix, strlen (in_posix) + 1);
  strcpy (posix, in_posix);
  if (in_native)
    {
      native = (char *) realloc (native, namelen + 1 + strlen (in_native));
      (void) strcpy (native, name);
      (void) strcpy (native + namelen, in_native);
    }
  else
    {
      native = (char *) realloc (native, namelen + 1 + win32_len (in_posix));
      (void) strcpy (native, name);
      towin32 (in_posix, native + namelen);
    }
  debug_printf ("posix %s", posix);
  debug_printf ("native %s", native);
}


/* Check for a "special" environment variable name.  *env is the pointer
 * to the beginning of the environment variable name.  n is the length
 * of the name including a mandatory '='.  Returns a pointer to the
 * appropriate conversion structure.
 */
win_env *
getwinenv (const char *env, const char *in_posix)
{
  for (int i = 0; conv_envvars[i].name != NULL; i++)
    if (strncasematch (env, conv_envvars[i].name, conv_envvars[i].namelen))
      {
	win_env *we = conv_envvars + i;
	const char *val;
	if (!environ || !(val = in_posix ?: getenv(we->name)))
	  debug_printf ("can't set native for %s since no environ yet",
			we->name);
	else if (!envcache || !we->posix || strcmp (val, we->posix))
	      we->add_cache (val);
	return we;
      }
  return NULL;
}

/* Convert windows path specs to POSIX, if appropriate.
 */
static void
posify (char **here, const char *value)
{
  char *src = *here;
  win_env *conv;
  int len = strcspn (src, "=") + 1;

  if (!(conv = getwinenv (src)))
    return;

  /* Turn all the items from c:<foo>;<bar> into their
     mounted equivalents - if there is one.  */

  char *outenv = (char *) malloc (1 + len + conv->posix_len (value));
  memcpy (outenv, src, len);
  conv->toposix (value, outenv + len);
  conv->add_cache (outenv + len, value);

  debug_printf ("env var converted to %s", outenv);
  *here = outenv;
  free (src);
}

/*
 * _findenv --
 *	Returns pointer to value associated with name, if any, else NULL.
 *	Sets offset to be the offset of the name/value combination in the
 *	environment array, for use by setenv(3) and unsetenv(3).
 *	Explicitly removes '=' in argument name.
 */

static char *
_findenv (const char *name, int *offset)
{
  register int len;
  register char **p;
  const char *c;

  c = name;
  len = 0;
  while (*c && *c != '=')
    {
      c++;
      len++;
    }

  for (p = environ; *p; ++p)
    if (!strncmp (*p, name, len))
      if (*(c = *p + len) == '=')
	{
	  *offset = p - environ;
	  return (char *) (++c);
	}
  return NULL;
}

/*
 * getenv --
 *	Returns ptr to value associated with name, if any, else NULL.
 */

extern "C"
char *
getenv (const char *name)
{
  int offset;

  return _findenv (name, &offset);
}

/* putenv --
 *	Sets an environment variable
 */

extern "C"
int
putenv (const char *str)
{
  register char *p, *equal;
  int rval;

  if (!(p = strdup (str)))
    return 1;
  if (!(equal = index (p, '=')))
    {
      (void) free (p);
      return 1;
    }
  *equal = '\0';
  rval = setenv (p, equal + 1, 1);
  (void) free (p);
  return rval;
}

/*
 * setenv --
 *	Set the value of the environment variable "name" to be
 *	"value".  If rewrite is set, replace any current value.
 */

extern "C"
int
setenv (const char *name, const char *value, int rewrite)
{
  register char *C;
  unsigned int l_value;
  int offset;

  if (*value == '=')		/* no `=' in value */
    ++value;
  l_value = strlen (value);
  if ((C = _findenv (name, &offset)))
    {				/* find if already exists */
      if (!rewrite)
	return 0;
      if (strlen (C) >= l_value)
	{			/* old larger; copy over */
	  while ((*C++ = *value++));
	  return 0;
	}
    }
  else
    {				/* create new slot */
      register int cnt;
      register char **P;

      for (P = environ, cnt = 0; *P; ++P, ++cnt)
	;
      __cygwin_environ = environ = (char **) realloc ((char *) environ,
		      (size_t) (sizeof (char *) * (cnt + 2)));
      if (!environ)
	return -1;
      environ[cnt + 1] = NULL;
      offset = cnt;
    }

  for (C = (char *) name; *C && *C != '='; ++C);	/* no `=' in name */

  if (!(environ[offset] =	/* name + `=' + value */
	(char *) malloc ((size_t) ((int) (C - name) + l_value + 2))))
    return -1;
  for (C = environ[offset]; (*C = *name++) && *C != '='; ++C);
  *C++ = '=';
  strcpy (C, value);

  win_env *spenv;
  if ((spenv = getwinenv (environ[offset])))
    spenv->add_cache (value);

  return 0;
}

/*
 * unsetenv(name) --
 *	Delete environment variable "name".
 */

extern "C"
void
unsetenv (const char *name)
{
  register char **P;
  int offset;

  while (_findenv (name, &offset))	/* if set multiple times */
    for (P = &environ[offset];; ++P)
      if (!(*P = *(P + 1)))
	break;
}

/* Turn environment variable part of a=b string into uppercase. */

static void __inline
ucenv (char *p, char *eq)
{
  /* Amazingly, NT has a case sensitive environment name list,
     but only sometimes.
     It's normal to have NT set your "Path" to something.
     Later, you set "PATH" to something else.  This alters "Path".
     But if you try and do a naive getenv on "PATH" you'll get nothing.

     So we upper case the labels here to prevent confusion later but
     we only do it for the first process in a session group. */
  for (; p < eq; p++)
    if (islower (*p))
      *p = toupper (*p);
}

/* Parse CYGWIN options */

static NO_COPY BOOL export_settings = FALSE;

enum settings
  {
    justset,
    isfunc,
    setbit
  };

/* The structure below is used to set up an array which is used to
 * parse the CYGWIN environment variable or, if enabled, options from
 * the registry.
 */
struct parse_thing
  {
    parse_thing () {}
    const char *name;
    union parse_setting
      {
	BOOL *b;
	DWORD *x;
	int *i;
	void (*func)(const char *);
	parse_setting() {}
	parse_setting(DWORD *x_in) {x = x_in;}
	parse_setting(int *i_in) {i = i_in;}
	parse_setting(void (*func_in) (const char *)) {func = func_in;}
      } setting;

    enum settings disposition;
    char *remember;
    union parse_values
      {
	DWORD i;
	const char *s;
	parse_values() {}
	parse_values(int in_i) {i = in_i;}
	parse_values(const char *in_s) {s = in_s;}
      } values[2];
  };

/* Create a table of "known" parse settings.  I wish there was some way
 * to avoid the arbitrary '10' below.
 */
class parse_things
{
  parse_thing things[20];
public:
  unsigned i;
  parse_things () {};
  parse_thing &operator [] (size_t x)
  {
#ifdef DEBUGGING
    if (x >= (sizeof(things) / sizeof(things[0])))
      api_fatal ("environ:  too many parse elements");
#endif
    return things[x];
  }
};

static parse_things known;

/* This template is used to initialize the parse table with appropriate
 * values needed by parse_options.
 */
template<class S, class T> static void __inline
add (const char *name, S setting, enum settings disposition, T value0, T value1)
{
  unsigned i = known.i++;
  known[i].name = name;
  known[i].disposition = disposition;
  known[i].remember = NULL;
  known[i].setting = setting;
  known[i].values[0] = value0;
  known[i].values[1] = value1;
}

/* Parse a string of the form "something=stuff somethingelse=more-stuff",
 * silently ignoring unknown "somethings".
 */
static void
parse_options (char *buf)
{
  int i, istrue;
  char *p;

  if (known.i == 0)
    {
      add ("export", &export_settings, justset, FALSE, TRUE);
      add ("title", &display_title, justset, FALSE, TRUE);
      add ("strip_title", &strip_title_path, justset, FALSE, TRUE);
      add ("glob", &allow_glob, justset, FALSE, TRUE);
      add ("tty", &myself->process_state, setbit, 0, (int)PID_USETTY);
      add ("binmode", &__fmode, justset, O_TEXT, O_BINARY);
      add ("strace", strace_init, isfunc, (char *) "0", (char *) "1");
      add ("ntea", &allow_ntea, justset, FALSE, TRUE);
      add ("reset_com", &reset_com, justset, FALSE, TRUE);
      add ("envcache", &envcache, justset, TRUE, FALSE);
      add ("threadsafe", &threadsafe, justset, TRUE, FALSE);
      add ("forkchunk", &chunksize, justset, 8192, 0);
    }

  if (buf == NULL)
    {
      char newbuf[MAX_PATH + 7] = "CYGWIN";
      for (i = 0; known[i].name != NULL; i++)
	if (known[i].remember)
	  {
	    strcat (strcat (newbuf, " "), known[i].remember);
	    free (known[i].remember);
	    known[i].remember = NULL;
	  }
      if (!export_settings)
	return;
      newbuf[sizeof ("CYGWIN") - 1] = '=';
      debug_printf ("%s", newbuf);
      putenv (newbuf);
    }

  buf = strcpy ((char *) alloca (strlen (buf) + 1), buf);
  for (p = strtok (buf, " \t"); p != NULL; p = strtok (NULL, " \t"))
    {
      if (!(istrue = !strncasematch (p, "no", 2)))
	p += 2;
      else if (!(istrue = *p != '-'))
	p++;

      char ch, *eq;
      if ((eq = strchr (p, '=')) != NULL || (eq = strchr (p, ':')) != NULL)
	ch = *eq, *eq++ = '\0';
      else
	ch = 0;
      for (i = 0; known[i].name != NULL; i++)
	if (strcasematch (p, known[i].name))
	  {
	    if (known[i].disposition == isfunc)
	      known[i].setting.func ((!eq || !istrue) ?
		known[i].values[istrue].s : eq);
	    else if (known[i].disposition == justset)
	      {
		if (!istrue || !eq)
		  *known[i].setting.x = known[i].values[istrue].i;
		else
		  *known[i].setting.x = strtol (eq, NULL, 0);
	      }
	    else // if (known[i].disposition == setbit)
	      {
		*known[i].setting.x &= ~known[i].values[istrue].i;
		if (istrue || (eq && strtol (eq, NULL, 0)))
		  *known[i].setting.x |= known[i].values[istrue].i;
	      }

	    if (!known[i].disposition == isfunc)
	      debug_printf ("%s %d", known[i].name, *known[i].setting.x);

	    if (eq)
	      *--eq = ch;

	    int n = eq - p;
	    p = strdup (p);
	    if (n > 0)
	      p[n] = ':';
	    known[i].remember = p;
	    break;
	  }
      }
  debug_printf ("returning");
  return;
}

#ifdef USE_REGISTRY
/* Set options from the registry. */

static void
regopt (const char *name)
{
  /* FIXME: should not be under mount */
  reg_key r (KEY_READ, CYGWIN_INFO_PROGRAM_OPTIONS_NAME, NULL);
  char buf[MAX_PATH];
  char lname[strlen(name) + 1];
  strlwr (strcpy (lname, name));
  if (r.get_string (lname, buf, sizeof (buf) - 1, "") == ERROR_SUCCESS)
    parse_options (buf);
}
#endif

/* Initialize the environ array.  Look for CYGWIN and STRACE environment
 * environment variables and set appropriate options from them.
 */
void
environ_init (void)
{
  const char * const rawenv = GetEnvironmentStrings ();
  int envsize, i;
  char *newp, **envp;
  const char *p;
  int sawTERM = 0;

  /* Allocate space for environment + trailing NULL + CYGWIN env. */
  envp = (char **) malloc ((2 + (envsize = 100)) * sizeof (char *));

#ifdef USE_REGISTRY
  regopt ("default");
  if (myself->progname[0])
    regopt (myself->progname);
#endif

  /* Current directory information is recorded as variables of the
     form "=X:=X:\foo\bar; these must be changed into something legal
     (we could just ignore them but maybe an application will
     eventually want to use them).  */
  for (i = 0, p = rawenv; *p != '\0'; p = strchr (p, '\0') + 1, i++)
    {
      newp = strdup (p);
      if (i >= envsize)
	envp = (char **) realloc (envp, (2 + (envsize += 100)) *
					    sizeof (char *));
      envp[i] = newp;
      if (*newp == '=')
	*newp = '!';
      char *eq;
      if ((eq = strchr (newp, '=')) == NULL)
	eq = strchr (newp, '\0');
      if (!myself->parent_alive)
	ucenv (newp, eq);
      if (strncmp (newp, "TERM=", 5) == 0)
	sawTERM = 1;
      if (strncmp (newp, "CYGWIN=", sizeof("CYGWIN=") - 1) == 0)
	parse_options (newp + sizeof("CYGWIN=") - 1);
      if (strncmp (newp, "STRACE=", 7) == 0)
	parse_options (newp);
      if (*eq)
	posify (envp + i, *++eq ? eq : --eq);
      debug_printf ("%s", envp[i]);
    }

  if (!sawTERM)
    envp[i++] = strdup ("TERM=cygwin");
  envp[i] = NULL;
  __cygwin_environ = *user_data->envptr = envp;
  FreeEnvironmentStringsA ((char *) rawenv);
  parse_options (NULL);
}

/* Function called by qsort to sort environment strings.
 */
static int
env_sort (const void *a, const void *b)
{
  const char **p = (const char **) a;
  const char **q = (const char **) b;

  return strcmp (*p, *q);
}

/* Create a Windows-style environment block, i.e. a typical character buffer
 * filled with null terminated strings, terminated by double null characters.
 * Converts environment variables noted in conv_envvars into win32 form
 * prior to placing them in the string.
 */
char *
winenv (const char * const *envp)
{
  int len, n, tl;
  const char * const *srcp;
  const char * *dstp;

  for (n = 0; envp[n]; n++)
    continue;

  const char *newenvp[n + 1];

  for (tl = 0, srcp = envp, dstp = newenvp; *srcp; srcp++, dstp++)
    {
      len = strcspn (*srcp, "=") + 1;
      win_env *conv;

      if ((conv = getwinenv (*srcp, *srcp + len)))
	*dstp = conv->native;
      else
	*dstp = *srcp;
      tl += strlen (*dstp) + 1;
      if ((*dstp)[0] == '!' && isalpha((*dstp)[1]) && (*dstp)[2] == ':' &&
	  (*dstp)[3] == '=')
	{
	  char *p = (char *) alloca (strlen (*dstp) + 1);
	  strcpy (p, *dstp);
	  *p = '=';
	  *dstp = p;
	}
    }

  *dstp = NULL;		/* Terminate */

  int envlen = dstp - newenvp;
  debug_printf ("env count %d, bytes %d", envlen, tl);

  /* Windows programs expect the environment block to be sorted.  */
  qsort (newenvp, envlen, sizeof (char *), env_sort);

  /* Create an environment block suitable for passing to CreateProcess.  */
  char *ptr, *envblock;
  envblock = (char *) malloc (tl + 2);
  for (srcp = newenvp, ptr = envblock; *srcp; srcp++)
    {
      len = strlen (*srcp);
      memcpy (ptr, *srcp, len + 1);
      ptr += len + 1;
    }
  *ptr = '\0';		/* Two null bytes at the end */

  return envblock;
}
