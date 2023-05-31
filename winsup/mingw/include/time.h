/* 
 * time.h
 *
 * Date and time functions and types.
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
 * $Revision: 1.3 $
 * $Author: noer $
 * $Date: 1998/10/20 01:24:49 $
 *
 */

#ifndef	_TIME_H_
#define	_TIME_H_

#define __need_wchar_t
#define __need_size_t
#ifndef RC_INVOKED
#include <stddef.h>
#endif	/* Not RC_INVOKED */

/*
 * Need a definition of time_t.
 */
#include <sys/types.h>

/*
 * Number of clock ticks per second. A clock tick is the unit by which
 * processor time is measured and is returned by 'clock'.
 */
#define	CLOCKS_PER_SEC	1000.0
#define	CLK_TCK		CLOCKS_PER_SEC


#ifndef RC_INVOKED

/*
 * A type for storing the current time and date. This is the number of
 * seconds since midnight Jan 1, 1970.
 * NOTE: Normally this is defined by the above include of sys/types.h
 */
#ifndef _TIME_T_
#define _TIME_T_
typedef	long	time_t;
#endif

/*
 * A type for measuring processor time (in clock ticks).
 */
#ifndef _CLOCK_T_
#define _CLOCK_T_
typedef	long	clock_t;
#endif


/*
 * A structure for storing all kinds of useful information about the
 * current (or another) time.
 */
struct tm
{
	int	tm_sec;		/* Seconds: 0-59 (K&R says 0-61?) */
	int	tm_min;		/* Minutes: 0-59 */
	int	tm_hour;	/* Hours since midnight: 0-23 */
	int	tm_mday;	/* Day of the month: 1-31 */
	int	tm_mon;		/* Months *since* january: 0-11 */
	int	tm_year;	/* Years since 1900 */
	int	tm_wday;	/* Days since Sunday (0-6) */
	int	tm_yday;	/* Days since Jan. 1: 0-365 */
	int	tm_isdst;	/* +1 Daylight Savings Time, 0 No DST,
				 * -1 don't know */
};

#ifdef	__cplusplus
extern "C" {
#endif

clock_t	clock ();
time_t	time (time_t* tp);
double	difftime (time_t t2, time_t t1);
time_t	mktime (struct tm* tmsp);

/*
 * These functions write to and return pointers to static buffers that may
 * be overwritten by other function calls. Yikes!
 *
 * NOTE: localtime, and perhaps the others of the four functions grouped
 * below may return NULL if their argument is not 'acceptable'. Also note
 * that calling asctime with a NULL pointer will produce an Invalid Page
 * Fault and crap out your program. Guess how I know. Hint: stat called on
 * a directory gives 'invalid' times in st_atime etc...
 */
char*		asctime (const struct tm* tmsp);
char*		ctime (const time_t* tp);
struct tm*	gmtime (const time_t* tm);
struct tm*	localtime (const time_t* tm);


size_t	strftime (char* caBuffer, size_t sizeMax, const char* szFormat,
		  const struct tm* tpPrint);

size_t	wcsftime (wchar_t* wcaBuffer, size_t sizeMax,
		  const wchar_t* wsFormat, const struct tm* tpPrint);

#ifndef __STRICT_ANSI__
extern void	_tzset (void);

#ifndef _NO_OLDNAMES
extern void	tzset (void);
#endif

#endif	/* Not __STRICT_ANSI__ */

/*
 * _daylight: non zero if daylight savings time is used.
 * _timezone: difference in seconds between GMT and local time.
 * _tzname: standard/daylight savings time zone names (an array with two
 *          elements).
 */
#ifdef __MSVCRT__

extern int*	__p__daylight (void);
extern long*	__p__timezone (void);
extern char**	__p__tzname (void);

#define _daylight	(*__p__daylight())
#define _timezone	(*__p__timezone())
#define _tzname		(__p__tzname())

#else /* not __MSVCRT (ie. crtdll) */

extern int*	__imp__daylight_dll;
extern long*	__imp__timezone_dll;
extern char**	__imp__tzname;

#define _daylight	(*__imp__daylight_dll)
#define _timezone	(*__imp__timezone_dll)
#define _tzname		(__imp__tzname)

#endif /* not __MSVCRT__ */

#ifndef _NO_OLDNAMES
#define	daylight	_daylight
/* NOTE: timezone not defined because it would conflict with sys/timeb.h */
#define	tzname		_tzname
#endif	/* Not _NO_OLDNAMES */

#ifdef	__cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif	/* Not _TIME_H_ */

