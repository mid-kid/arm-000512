/*
 * stat.h
 *
 * Symbolic constants for opening and creating files, also stat, fstat and
 * chmod functions.
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
 * $Revision: 1.2 $
 * $Author: noer $
 * $Date: 1998/12/30 08:09:29 $
 *
 */

#ifndef __STRICT_ANSI__

#ifndef _STAT_H_
#define _STAT_H_

#include <sys/types.h>

/*
 * Constants for the stat st_mode member.
 */
#define	_S_IFIFO	0x1000	/* FIFO */
#define	_S_IFCHR	0x2000	/* Character */
#define	_S_IFBLK	0x3000	/* Block */
#define	_S_IFDIR	0x4000	/* Directory */
#define	_S_IFREG	0x8000	/* Regular */

#define	_S_IFMT		0xF000	/* File type mask */

#define	_S_IEXEC	0x0040
#define	_S_IWRITE	0x0080
#define	_S_IREAD	0x0100

#define	_S_IRWXU	(_S_IREAD | _S_IWRITE | _S_IEXEC)
#define	_S_IXUSR	_S_IEXEC
#define	_S_IWUSR	_S_IWRITE
#define	_S_IRUSR	_S_IREAD

#define	_S_ISDIR(m)	((m) & _S_IFDIR)
#define	_S_ISFIFO(m)	((m) & _S_IFIFO)
#define	_S_ISCHR(m)	((m) & _S_IFCHR)
#define	_S_ISBLK(m)	((m) & _S_IFBLK)
#define	_S_ISREG(m)	((m) & _S_IFREG)

#ifndef _NO_OLDNAMES

#define	S_IFIFO		_S_IFIFO
#define	S_IFCHR		_S_IFCHR
#define	S_IFBLK		_S_IFBLK
#define	S_IFDIR		_S_IFDIR
#define	S_IFREG		_S_IFREG
#define	S_IFMT		_S_IFMT
#define	S_IEXEC		_S_IEXEC
#define	S_IWRITE	_S_IWRITE
#define	S_IREAD		_S_IREAD
#define	S_IRWXU		_S_IRWXU
#define	S_IXUSR		_S_IXUSR
#define	S_IWUSR		_S_IWUSR
#define	S_IRUSR		_S_IRUSR

#define	S_ISDIR(m)	((m) & S_IFDIR)
#define	S_ISFIFO(m)	((m) & S_IFIFO)
#define	S_ISCHR(m)	((m) & S_IFCHR)
#define	S_ISBLK(m)	((m) & S_IFBLK)
#define	S_ISREG(m)	((m) & S_IFREG)

#endif	/* Not _NO_OLDNAMES */

#ifndef RC_INVOKED

/*
 * The structure manipulated and returned by stat and fstat.
 *
 * NOTE: If called on a directory the values in the time fields are not only
 * invalid, they will cause localtime et. al. to return NULL. And calling
 * asctime with a NULL pointer causes an Invalid Page Fault. So watch it!
 */
struct stat
{
	_dev_t	st_dev;		/* Equivalent to drive number 0=A 1=B ... */
	_ino_t	st_ino;		/* Always zero ? */
	_mode_t	st_mode;	/* See above constants */
	short	st_nlink;	/* Number of links. */
	short	st_uid;		/* User: Maybe significant on NT ? */
	short	st_gid;		/* Group: Ditto */
	_dev_t	st_rdev;	/* Seems useless (not even filled in) */
	_off_t	st_size;	/* File size in bytes */
	time_t	st_atime;	/* Accessed date (always 00:00 hrs local
				 * on FAT) */
	time_t	st_mtime;	/* Modified time */
	time_t	st_ctime;	/* Creation time */
};


#ifdef	__cplusplus
extern "C" {
#endif

int	_fstat (int nHandle, struct stat* pstat);
int	_chmod (const char* szPath, int nMode);
int	_stat (const char* szPath, struct stat* pstat);


#ifndef	_NO_OLDNAMES

/* These functions live in liboldnames.a. */
int	fstat (int nHandle, struct stat* pstat);
int	chmod (const char* szPath, int nMode);
int	stat (const char* szPath, struct stat* pstat);

#endif	/* Not _NO_OLDNAMES */


#ifdef	__cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif	/* Not _STAT_H_ */

#endif	/* Not __STRICT_ANSI__ */

