/* 
 * tclEnv.c --
 *
 *	Tcl support for environment variables, including a setenv
 *	procedure.  This file contains the generic portion of the
 *	environment module.  It is primarily responsible for keeping
 *	the "env" arrays in sync with the system environment variables.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclEnv.c,v 1.19 1999/01/26 03:53:11 jingham Exp $
 */

#include "tclInt.h"
#include "tclPort.h"

/* CYGNUS LOCAL */
#ifdef __CYGWIN32__

/* On cygwin32, the environment is imported from the cygwin32 DLL.  */

extern char ***__imp___cygwin_environ;

#define environ (*__imp___cygwin_environ)

/* We need to use a special putenv function to handle PATH.  */
#ifndef USE_PUTENV
#define USE_PUTENV
#endif
#define putenv TclCygwin32Putenv

#endif
/* END CYGNUS LOCAL */

/*
 * The structure below is used to keep track of all of the interpereters
 * for which we're managing the "env" array.  It's needed so that they
 * can all be updated whenever an environment variable is changed
 * anywhere.
 */

typedef struct EnvInterp {
    Tcl_Interp *interp;		/* Interpreter for which we're managing
				 * the env array. */
    struct EnvInterp *nextPtr;	/* Next in list of all such interpreters,
				 * or zero. */
} EnvInterp;

static EnvInterp *firstInterpPtr = NULL;
				/* First in list of all managed interpreters,
				 * or NULL if none. */

static int cacheSize = 0;	/* Number of env strings in environCache. */
static char **environCache = NULL;
				/* Array containing all of the environment
				 * strings that Tcl has allocated. */

#ifndef USE_PUTENV
static int environSize = 0;	/* Non-zero means that the environ array was
				 * malloced and has this many total entries
				 * allocated to it (not all may be in use at
				 * once).  Zero means that the environment
				 * array is in its original static state. */
#endif

/*
 * Declarations for local procedures defined in this file:
 */

static char *		EnvTraceProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		FindVariable _ANSI_ARGS_((CONST char *name,
			    int *lengthPtr));
static void		ReplaceString _ANSI_ARGS_((CONST char *oldStr,
			    char *newStr));
void			TclSetEnv _ANSI_ARGS_((CONST char *name,
			    CONST char *value));
void			TclUnsetEnv _ANSI_ARGS_((CONST char *name));

/* CYGNUS LOCAL */
#ifdef __CYGWIN32__
static void		TclCygwin32Putenv _ANSI_ARGS_((CONST char *string));
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclSetupEnv --
 *
 *	This procedure is invoked for an interpreter to make environment
 *	variables accessible from that interpreter via the "env"
 *	associative array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The interpreter is added to a list of interpreters managed
 *	by us, so that its view of envariables can be kept consistent
 *	with the view in other interpreters.  If this is the first
 *	call to Tcl_SetupEnv, then additional initialization happens,
 *	such as copying the environment to dynamically-allocated space
 *	for ease of management.
 *
 *----------------------------------------------------------------------
 */

void
TclSetupEnv(interp)
    Tcl_Interp *interp;		/* Interpreter whose "env" array is to be
				 * managed. */
{
    EnvInterp *eiPtr;
    char *p, *p2;
    Tcl_DString ds;
    int i, sz;

#ifdef MAC_TCL
    if (environ == NULL) {
	environSize = TclMacCreateEnv();
    }
#endif

    /*
     * Next, initialize the DString we are going to use for copying
     * the names of the environment variables.
     */

    Tcl_DStringInit(&ds);
    
    /*
     * Next, add the interpreter to the list of those that we manage.
     */

    eiPtr = (EnvInterp *) ckalloc(sizeof(EnvInterp));
    eiPtr->interp = interp;
    eiPtr->nextPtr = firstInterpPtr;
    firstInterpPtr = eiPtr;

    /*
     * Store the environment variable values into the interpreter's
     * "env" array, and arrange for us to be notified on future
     * writes and unsets to that array.
     */

    (void) Tcl_UnsetVar2(interp, "env", (char *) NULL, TCL_GLOBAL_ONLY);
    for (i = 0; ; i++) {
	p = environ[i];
	if (p == NULL) {
	    break;
	}
	for (p2 = p; *p2 != '='; p2++) {
	    if (*p2 == 0) {
		/*
		 * This condition doesn't seem like it should ever happen,
		 * but it does seem to happen occasionally under some
		 * versions of Solaris; ignore the entry.
		 */

		goto nextEntry;
	    }
	}
        sz = p2 - p;
        Tcl_DStringSetLength(&ds, 0);
        Tcl_DStringAppend(&ds, p, sz);
	(void) Tcl_SetVar2(interp, "env", Tcl_DStringValue(&ds),
                p2+1, TCL_GLOBAL_ONLY);
	nextEntry:
	continue;
    }
    Tcl_TraceVar2(interp, "env", (char *) NULL,
	    TCL_GLOBAL_ONLY | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	    EnvTraceProc, (ClientData) NULL);

    /*
     * Finally clean up the DString.
     */

    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * TclSetEnv --
 *
 *	Set an environment variable, replacing an existing value
 *	or creating a new variable if there doesn't exist a variable
 *	by the given name.  This procedure is intended to be a
 *	stand-in for the  UNIX "setenv" procedure so that applications
 *	using that procedure will interface properly to Tcl.  To make
 *	it a stand-in, the Makefile must define "TclSetEnv" to "setenv".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The environ array gets updated, as do all of the interpreters
 *	that we manage.
 *
 *----------------------------------------------------------------------
 */

void
TclSetEnv(name, value)
    CONST char *name;		/* Name of variable whose value is to be
				 * set. */
    CONST char *value;		/* New value for variable. */
{
    int index, length, nameLength;
    char *p, *oldValue;
    EnvInterp *eiPtr;

#ifdef MAC_TCL
    if (environ == NULL) {
	environSize = TclMacCreateEnv();
    }
#endif

    /*
     * Figure out where the entry is going to go.  If the name doesn't
     * already exist, enlarge the array if necessary to make room.  If
     * the name exists, free its old entry.
     */

    index = FindVariable(name, &length);
    if (index == -1) {
#ifndef USE_PUTENV
	if ((length+2) > environSize) {
	    char **newEnviron;

	    newEnviron = (char **) ckalloc((unsigned)
		    ((length+5) * sizeof(char *)));

	    /* CYGNUS LOCAL: Added to avoid an error from Purify,
               although I don't personally see where the error would
               occur--ian.  */
	    memset((VOID *) newEnviron, 0, (length+5) * sizeof(char *));

	    memcpy((VOID *) newEnviron, (VOID *) environ,
		    length*sizeof(char *));
	    if (environSize != 0) {
		ckfree((char *) environ);
	    }
	    environ = newEnviron;
	    environSize = length+5;
	}
	index = length;
	environ[index+1] = NULL;
#endif
	oldValue = NULL;
	nameLength = strlen(name);
    } else {
	/*
	 * Compare the new value to the existing value.  If they're
	 * the same then quit immediately (e.g. don't rewrite the
	 * value or propagate it to other interpreters).  Otherwise,
	 * when there are N interpreters there will be N! propagations
	 * of the same value among the interpreters.
	 */

	if (strcmp(value, environ[index]+length+1) == 0) {
	    return;
	}
	oldValue = environ[index];
	nameLength = length;
    }
	

    /*
     * Create a new entry.
     */

    p = (char *) ckalloc((unsigned) (nameLength + strlen(value) + 2));
    strcpy(p, name);
    p[nameLength] = '=';
    strcpy(p+nameLength+1, value);

    /*
     * Update the system environment.
     */

#ifdef USE_PUTENV
    putenv(p);
#else
    environ[index] = p;
#endif

    /*
     * Replace the old value with the new value in the cache.
     */

    ReplaceString(oldValue, p);

    /*
     * Update all of the interpreters.
     */

    /* CYGNUS LOCAL: The original code was bogus.  If we are being
       called because of a trace on the env array, then the call to
       Tcl_SetVar2 would free value.  We avoid that by checking
       whether the value is the same before calling Tcl_SetVar2.

       NOTE: This is not necessary in tcl8.1a2 which handles this in a
       completely different, and better, way.  */

    for (eiPtr= firstInterpPtr; eiPtr != NULL; eiPtr = eiPtr->nextPtr) {
	CONST char *v;

	v = Tcl_GetVar2(eiPtr->interp, "env", (char *) name, TCL_GLOBAL_ONLY);
	if (v == NULL || (v != value && strcmp (v, value) != 0)) {
	    (void) Tcl_SetVar2(eiPtr->interp, "env", (char *) name,
		    (char *) value, TCL_GLOBAL_ONLY);
	}
    }

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PutEnv --
 *
 *	Set an environment variable.  Similar to setenv except that
 *	the information is passed in a single string of the form
 *	NAME=value, rather than as separate name strings.  This procedure
 *	is intended to be a stand-in for the  UNIX "putenv" procedure
 *	so that applications using that procedure will interface
 *	properly to Tcl.  To make it a stand-in, the Makefile will
 *	define "Tcl_PutEnv" to "putenv".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The environ array gets updated, as do all of the interpreters
 *	that we manage.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_PutEnv(string)
    CONST char *string;		/* Info about environment variable in the
				 * form NAME=value. */
{
    int nameLength;
    char *name, *value;

    if (string == NULL) {
	return 0;
    }

    /*
     * Separate the string into name and value parts, then call
     * TclSetEnv to do all of the real work.
     */

    value = strchr(string, '=');
    if (value == NULL) {
	return 0;
    }
    nameLength = value - string;
    if (nameLength == 0) {
	return 0;
    }
    name = (char *) ckalloc((unsigned) nameLength+1);
    memcpy((VOID *) name, (VOID *) string, (size_t) nameLength);
    name[nameLength] = 0;
    TclSetEnv(name, value+1);
    ckfree(name);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUnsetEnv --
 *
 *	Remove an environment variable, updating the "env" arrays
 *	in all interpreters managed by us.  This function is intended
 *	to replace the UNIX "unsetenv" function (but to do this the
 *	Makefile must be modified to redefine "TclUnsetEnv" to
 *	"unsetenv".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interpreters are updated, as is environ.
 *
 *----------------------------------------------------------------------
 */

void
TclUnsetEnv(name)
    CONST char *name;			/* Name of variable to remove. */
{
    EnvInterp *eiPtr;
    char *oldValue;
    int length, index;
#ifdef USE_PUTENV
    char *string;
#else
    char **envPtr;
#endif

#ifdef MAC_TCL
    if (environ == NULL) {
	environSize = TclMacCreateEnv();
    }
#endif

    index = FindVariable(name, &length);

    /*
     * First make sure that the environment variable exists to avoid
     * doing needless work and to avoid recursion on the unset.
     */
    
    if (index == -1) {
	return;
    }
    /*
     * Remember the old value so we can free it if Tcl created the string.
     */

    oldValue = environ[index];

    /*
     * Update the system environment.  This must be done before we 
     * update the interpreters or we will recurse.
     */

#ifdef USE_PUTENV
    string = ckalloc(length+2);
    memcpy((VOID *) string, (VOID *) name, (size_t) length);
    string[length] = '=';
    string[length+1] = '\0';
    putenv(string);
    ckfree(string);
#else
    for (envPtr = environ+index+1; ; envPtr++) {
	envPtr[-1] = *envPtr;
	if (*envPtr == NULL) {
	    break;
	}
    }
#endif

    /*
     * Replace the old value in the cache.
     */

    ReplaceString(oldValue, NULL);

    /*
     * Update all of the interpreters.
     */

    for (eiPtr = firstInterpPtr; eiPtr != NULL; eiPtr = eiPtr->nextPtr) {
	(void) Tcl_UnsetVar2(eiPtr->interp, "env", (char *) name,
		TCL_GLOBAL_ONLY);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetEnv --
 *
 *	Retrieve the value of an environment variable.
 *
 * Results:
 *	Returns a pointer to a static string in the environment,
 *	or NULL if the value was not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetEnv(name)
    CONST char *name;		/* Name of variable to find. */
{
    int length, index;

#ifdef MAC_TCL
    if (environ == NULL) {
	environSize = TclMacCreateEnv();
    }
#endif

    index = FindVariable(name, &length);
    if ((index != -1) &&  (*(environ[index]+length) == '=')) {
	return environ[index]+length+1;
    } else {
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EnvTraceProc --
 *
 *	This procedure is invoked whenever an environment variable
 *	is modified or deleted.  It propagates the change to the
 *	"environ" array and to any other interpreters for whom
 *	we're managing an "env" array.
 *
 * Results:
 *	Always returns NULL to indicate success.
 *
 * Side effects:
 *	Environment variable changes get propagated.  If the whole
 *	"env" array is deleted, then we stop managing things for
 *	this interpreter (usually this happens because the whole
 *	interpreter is being deleted).
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
EnvTraceProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter whose "env" variable is
				 * being modified. */
    char *name1;		/* Better be "env". */
    char *name2;		/* Name of variable being modified, or
				 * NULL if whole array is being deleted. */
    int flags;			/* Indicates what's happening. */
{
    /*
     * First see if the whole "env" variable is being deleted.  If
     * so, just forget about this interpreter.
     */

    if (name2 == NULL) {
	register EnvInterp *eiPtr, *prevPtr;

	if ((flags & (TCL_TRACE_UNSETS|TCL_TRACE_DESTROYED))
		!= (TCL_TRACE_UNSETS|TCL_TRACE_DESTROYED)) {
	    panic("EnvTraceProc called with confusing arguments");
	}
	eiPtr = firstInterpPtr;
	if (eiPtr->interp == interp) {
	    firstInterpPtr = eiPtr->nextPtr;
	} else {
	    for (prevPtr = eiPtr, eiPtr = eiPtr->nextPtr; ;
		    prevPtr = eiPtr, eiPtr = eiPtr->nextPtr) {
		if (eiPtr == NULL) {
		    panic("EnvTraceProc couldn't find interpreter");
		}
		if (eiPtr->interp == interp) {
		    prevPtr->nextPtr = eiPtr->nextPtr;
		    break;
		}
	    }
	}
	ckfree((char *) eiPtr);
	return NULL;
    }

    /*
     * If a value is being set, call TclSetEnv to do all of the work.
     */

    if (flags & TCL_TRACE_WRITES) {
	TclSetEnv(name2, Tcl_GetVar2(interp, "env", name2, TCL_GLOBAL_ONLY));
    }

    if (flags & TCL_TRACE_UNSETS) {
	TclUnsetEnv(name2);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ReplaceString --
 *
 *	Replace one string with another in the environment variable
 *	cache.  The cache keeps track of all of the environment
 *	variables that Tcl has modified so they can be freed later.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May free the old string.
 *
 *----------------------------------------------------------------------
 */

static void
ReplaceString(oldStr, newStr)
    CONST char *oldStr;		/* Old environment string. */
    char *newStr;		/* New environment string. */
{
    int i;
    char **newCache;

    /*
     * Check to see if the old value was allocated by Tcl.  If so,
     * it needs to be deallocated to avoid memory leaks.  Note that this
     * algorithm is O(n), not O(1).  This will result in n-squared behavior
     * if lots of environment changes are being made.
     */

    for (i = 0; i < cacheSize; i++) {
	if ((environCache[i] == oldStr) || (environCache[i] == NULL)) {
	    break;
	}
    }
    if (i < cacheSize) {
	/*
	 * Replace or delete the old value.
	 */

	if (environCache[i]) {
	    ckfree(environCache[i]);
	}
	    
	if (newStr) {
	    environCache[i] = newStr;
	} else {
	    for (; i < cacheSize-1; i++) {
		environCache[i] = environCache[i+1];
	    }
	    environCache[cacheSize-1] = NULL;
	}
    } else {	
        int allocatedSize = (cacheSize + 5) * sizeof(char *);

	/*
	 * We need to grow the cache in order to hold the new string.
	 */

	newCache = (char **) ckalloc((size_t) allocatedSize);
        (VOID *) memset(newCache, (int) 0, (size_t) allocatedSize);
        
	if (environCache) {
	    memcpy((VOID *) newCache, (VOID *) environCache,
		    (size_t) (cacheSize * sizeof(char*)));
	    ckfree((char *) environCache);
	}
	environCache = newCache;
	environCache[cacheSize] = (char *) newStr;
	environCache[cacheSize+1] = NULL;
	cacheSize += 5;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FindVariable --
 *
 *	Locate the entry in environ for a given name.
 *
 * Results:
 *	The return value is the index in environ of an entry with the
 *	name "name", or -1 if there is no such entry.   The integer at
 *	*lengthPtr is filled in with the length of name (if a matching
 *	entry is found) or the length of the environ array (if no matching
 *	entry is found).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
FindVariable(name, lengthPtr)
    CONST char *name;		/* Name of desired environment variable. */
    int *lengthPtr;		/* Used to return length of name (for
				 * successful searches) or number of non-NULL
				 * entries in environ (for unsuccessful
				 * searches). */
{
    int i;
    register CONST char *p1, *p2;

    for (i = 0, p1 = environ[i]; p1 != NULL; i++, p1 = environ[i]) {
	for (p2 = name; *p2 == *p1; p1++, p2++) {
	    /* NULL loop body. */
	}
	if ((*p1 == '=') && (*p2 == '\0')) {
	    *lengthPtr = p2-name;
	    return i;
	}
    }
    *lengthPtr = i;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeEnvironment --
 *
 *	This function releases any storage allocated by this module
 *	that isn't still in use by the global environment.  Any
 *	strings that are still in the environment will be leaked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May deallocate storage.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeEnvironment()
{
    /*
     * For now we just deallocate the cache array and none of the environment
     * strings.  This may leak more memory that strictly necessary, since some
     * of the strings may no longer be in the environment.  However,
     * determining which ones are ok to delete is n-squared, and is pretty
     * unlikely, so we don't bother.
     */

    if (environCache) {
	ckfree((char *) environCache);
	environCache = NULL;
	cacheSize    = 0;
#ifndef USE_PUTENV
	environSize  = 0;
#endif
    }
}

/* CYGNUS LOCAL */
#ifdef __CYGWIN32__

/* When using cygwin32, when an environment variable changes, we need
   to synch with both the cygwin32 environment (in case the
   application C code calls fork) and the Windows environment (in case
   the application TCL code calls exec, which calls the Windows
   CreateProcess function).  */

static void
TclCygwin32Putenv(str)
     const char *str;
{
  char *name, *value;

  /* Get the name and value, so that we can change the environment
     variable for Windows.  */
  name = (char *) alloca (strlen (str) + 1);
  strcpy (name, str);
  for (value = name; *value != '=' && *value != '\0'; ++value)
    ;
  if (*value == '\0')
    {
      /* Can't happen.  */
      return;
    }
  *value = '\0';
  ++value;
  if (*value == '\0')
    value = NULL;

  /* Set the cygwin32 environment variable.  */
#undef putenv
  if (value == NULL)
    unsetenv (name);
  else
    putenv(str);

  /* Before changing the environment variable in Windows, if this is
     PATH, we need to convert the value back to a Windows style path.

     FIXME: The calling program may now it is running under windows,
     and may have set the path to a Windows path, or, worse, appended
     or prepended a Windows path to PATH.  */
  if (strcmp (name, "PATH") != 0)
    {
      /* If this is Path, eliminate any PATH variable, to prevent any
         confusion.  */
      if (strcmp (name, "Path") == 0)
	{
	  SetEnvironmentVariable ("PATH", (char *) NULL);
	  unsetenv ("PATH");
	}

      SetEnvironmentVariable (name, value);
    }
  else
    {
      char *buf;

      /* Eliminate any Path variable, to prevent any confusion.  */
      SetEnvironmentVariable ("Path", (char *) NULL);
      unsetenv ("Path");

      if (value == NULL)
	buf = NULL;
      else
	{
	  int size;

	  size = cygwin32_posix_to_win32_path_list_buf_size (value);
	  buf = (char *) alloca (size + 1);
	  cygwin32_posix_to_win32_path_list (value, buf);
	}

      SetEnvironmentVariable (name, buf);
    }
}

#endif /* __CYGWIN32__ */
/* END CYGNUS LOCAL */
