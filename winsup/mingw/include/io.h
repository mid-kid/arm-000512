/* 
 * io.h
 *
 * System level I/O functions and types.
 *
 * This file is part of the Mingw32 package.
 *
 * Contributors:
 *  Created by Colin Peters <colin@bird.fu.is.saga-u.ac.jp>
 *
 *  THIS SOFTWARE IS NOT COPYRIGHTED
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Revision: 1.1 $
 * $Author: noer $
 * $Date: 1998/10/10 00:51:16 $
 *
 */

#ifndef	__STRICT_ANSI__

#ifndef	_IO_H_
#define	_IO_H_

/* We need the definition of FILE anyway... */
#include <stdio.h>

/* MSVC's io.h contains the stuff from dir.h, so I will too.
 * NOTE: This also defines off_t, the file offset type, through
 *       an inclusion of sys/types.h */
#include <dir.h>

/* TODO: Maximum number of open handles has not been tested, I just set
 * it the same as FOPEN_MAX. */
#define	HANDLE_MAX	FOPEN_MAX


/* Some defines for _access nAccessMode (MS doesn't define them, but
 * it doesn't seem to hurt to add them). */
#define	F_OK	0	/* Check for file existence */
#define	W_OK	2	/* Check for write permission */
#define	R_OK	4	/* Check for read permission */
/* TODO: Is this safe? X_OK not supported directly... */
#define X_OK	R_OK	/* Check for execute permission */

#ifndef RC_INVOKED

#ifdef	__cplusplus
extern "C" {
#endif

int		_access (const char* szFileName, int nAccessMode);
int		_chsize (int nHandle, long lnNewSize);
int		_close (int nHandle);

/* NOTE: The only significant bit in unPermissions appears to be bit 7 (0x80),
 *       the "owner write permission" bit (on FAT). */
int		_creat (const char* szFileName, unsigned unPermissions);

int		_dup (int nHandle);
int		_dup2 (int nOldHandle, int nNewHandle);
long		_filelength (int nHandle);
int		_fileno (FILE* fileGetHandle);
long		_get_osfhandle (int nHandle);
int		_isatty (int nHandle);

/* In a very odd turn of events this function is excluded from those
 * files which define _STREAM_COMPAT. This is required in order to
 * build GNU libio because of a conflict with _eof in streambuf.h
 * line 107. Actually I might just be able to change the name of
 * the enum member in streambuf.h... we'll see. TODO */
#ifndef	_STREAM_COMPAT
int		_eof (int nHandle);
#endif

/* LK_... locking commands defined in sys/locking.h. */
int		_locking (int nHandle, int nCmd, long lnLockRegionLength);

long		_lseek (int nHandle, long lnOffset, int nOrigin);

/* Optional third argument is unsigned unPermissions. */
int		_open (const char* szFileName, int nFlags, ...);

int		_open_osfhandle (long lnOSHandle, int nFlags);
int		_pipe (int *naHandles, unsigned int unSize, int nMode);
int		_read (int nHandle, void* caBuffer, unsigned int nToRead);

/* SH_... flags for nShFlags defined in share.h
 * Optional fourth argument is unsigned unPermissions */
int		_sopen (char* szFileName, int nFlags, int nShFlags, ...);

long		_tell (int nHandle);
/* Should umask be in sys/stat.h and/or sys/types.h instead? */
int		_umask (int nMode);
int		_unlink (const char* szFileName);
int		_write (int nHandle, const void* caBuffer,
		        unsigned int unToWrite);


#ifndef	_NO_OLDNAMES
/*
 * Non-underscored versions of non-ANSI functions to improve portability.
 * These functions live in libmoldname.a.
 */

int		access (const char* szFileName, int nAccessMode);
int		chsize (int nHandle, long lnNewSize);
int		close (int nHandle);
int		creat (const char* szFileName, int nAccessMode);
int		dup (int nHandle);
int		dup2 (int nOldHandle, int nNewHandle);
int		eof (int nHandle);
long		filelength (int nHandle);
int		fileno (FILE* fileGetHandle);
int		isatty (int nHandle);
long		lseek (int nHandle, long lnOffset, int nOrigin);
int		open (const char* szFileName, int nFlags, ...);
int		read (int nHandle, void* caBuffer, unsigned int nToRead);
int		sopen (char* szFileName, int nAccess, int nFlag, ...);
long		tell (int nHandle);
int		umask (int nMode);
int		unlink (const char* szFileName);
int		write (int nHandle, const void* caBuffer,
		       unsigned int nToWrite);

#endif	/* Not _NO_OLDNAMES */

#ifdef	__cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif	/* _IO_H_ not defined */

#endif	/* Not strict ANSI */

