/* 
 * tkWin32Dll.c --
 *
 *	This file contains a stub dll entry point.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkWin32Dll.c,v 1.9 1999/01/26 04:11:44 jingham Exp $
 */

#include "tkPort.h"
#include "tkWinInt.h"

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain _ANSI_ARGS_((HINSTANCE hInst,
			    DWORD reason, LPVOID reserved));
/* CYGNUS LOCAL */
#ifdef __CYGWIN32__
/* cygwin32 requires an impure pointer variable, which must be
   explicitly initialized when the DLL starts up.  */
struct _reent *_impure_ptr;
extern struct _reent *__imp_reent_data;
#endif
/* END CYGNUS LOCAL */

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the
 *	initialization code for Tk.  It simply calls the DllMain
 *	routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	DLL entry point.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllMain(hInstance, reason, reserved)
    HINSTANCE hInstance;
    DWORD reason;
    LPVOID reserved;
{
    /* CYGNUS LOCAL */
#ifdef __CYGWIN32__
    /* cygwin32 requires the impure data pointer to be initialized
       when the DLL starts up.  */
    _impure_ptr = __imp_reent_data;
#endif
    /* END CYGNUS LOCAL */

    /*
     * If we are attaching to the DLL from a new process, tell Tk about
     * the hInstance to use. If we are detaching then clean up any
     * data structures related to this DLL.
     */
    
    if (reason == DLL_PROCESS_ATTACH) {
        TkWinXInit(hInstance);
    } else if (reason == DLL_PROCESS_DETACH) {
        TkWinXCleanup(hInstance);
    }
    return(TRUE);
}
