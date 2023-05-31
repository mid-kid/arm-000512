/* debug.h

   Copyright 1998, 1999 Cygnus Solutions.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#if !defined(_DEBUG_H_)
#define _DEBUG_H_

HANDLE makethread (LPTHREAD_START_ROUTINE, LPVOID, DWORD, const char *);
const char *threadname (DWORD);
void regthread (const char *, DWORD);

#ifndef DEBUGGING
# define ForceCloseHandle CloseHandle
# define ProtectHandle(h) do {} while (0)

#else

# ifdef NO_DEBUG_DEFINES
#   undef NO_DEBUG_DEFINES
# else
#   define CloseHandle(h) \
	close_handle (__PRETTY_FUNCTION__, __LINE__, (h), #h, FALSE)
#   define ForceCloseHandle(h) \
	close_handle (__PRETTY_FUNCTION__, __LINE__, (h), #h, TRUE)
#   define ProtectHandle(h) add_handle (__PRETTY_FUNCTION__, __LINE__, (h), #h)
#   define lock_pinfo_for_update(n) lpfu(__PRETTY_FUNCTION__, __LINE__, n)
# endif

void add_handle (const char *, int, HANDLE, const char *);
BOOL close_handle (const char *, int, HANDLE, const char *, BOOL);
int lpfu (const char *, int, DWORD timeout);
DWORD WFSO (HANDLE, DWORD);
DWORD WFMO (DWORD, CONST HANDLE *, BOOL, DWORD);

#define WaitForSingleObject WFSO
#define WaitForMultipleObject WFMO

#endif /*DEBUGGING*/
#endif /*_DEBUG_H_*/
