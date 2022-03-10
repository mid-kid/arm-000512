/*
 * stdlib.h
 *
 * Definitions for common types, variables, and functions.
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

#ifndef _STDLIB_H_
#define _STDLIB_H_

/*
 * RAND_MAX is the maximum value that may be returned by rand.
 * The minimum is zero.
 */
#define	RAND_MAX	0x7FFF

/*
 * These values may be used as exit status codes.
 */
#define	EXIT_SUCCESS	0
#define	EXIT_FAILURE	-1

/*
 * Definitions for path name functions.
 * NOTE: All of these values have simply been chosen to be conservatively high.
 *       Remember that with long file names we can no longer depend on
 *       extensions being short.
 */
#ifndef __STRICT_ANSI__

#ifndef MAX_PATH
#define	MAX_PATH	(260)
#endif

#define	_MAX_PATH	MAX_PATH
#define	_MAX_DRIVE	(3)
#define	_MAX_DIR	256
#define	_MAX_FNAME	256
#define	_MAX_EXT	256

#endif	/* Not __STRICT_ANSI__ */


#ifndef RC_INVOKED

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This seems like a convenient place to declare these variables, which
 * give programs using WinMain (or main for that matter) access to main-ish
 * argc and argv. environ is a pointer to a table of environment variables.
 * NOTE: Strings in _argv and environ are ANSI strings.
 */
extern int	_argc;
extern char**	_argv;

/*
 * Use environ from the DLL, not as a global. 
 */

#ifdef __MSVCRT__
   extern char *** __p__environ();
#  define _environ (*__p__environ())
#else /* ! __MSVCRT__ */
   extern char *** __imp__environ_dll;
#  define _environ (*__imp__environ_dll)
#endif /* ! __MSVCRT__ */

#define environ _environ

#ifdef	__MSVCRT__
/* One of the MSVCRTxx libraries */
extern int*	__imp__sys_nerr;
#define	sys_nerr	(*__imp__sys_nerr)
#else
/* CRTDLL run time library */
extern int*	__imp__sys_nerr_dll;
#define sys_nerr	(*__imp__sys_nerr_dll)
#endif

extern char**	__imp__sys_errlist;
#define	sys_errlist	(__imp__sys_errlist)

/*
 * OS version and such constants.
 */
#ifndef __STRICT_ANSI__

#ifdef	__MSVCRT__
/* msvcrtxx.dll */

extern unsigned int*	__p__osver();
extern unsigned int*	__p__winver();
extern unsigned int*	__p__winmajor();
extern unsigned int*	__p__winminor();

#define _osver		(*__p__osver())
#define _winver		(*__p__winver())
#define _winmajor	(*__p__winmajor())
#define _winminor	(*__p__winminor())

#else
/* Not msvcrtxx.dll, thus crtdll.dll */

extern unsigned int*	_imp___osver_dll;
extern unsigned int*	_imp___winver_dll;
extern unsigned int*	_imp___winmajor_dll;
extern unsigned int*	_imp___winminor_dll;

#define _osver		(*_imp___osver_dll)
#define _winver		(*_imp___winver_dll)
#define _winmajor	(*_imp___winmajor_dll)
#define _winminor	(*_imp___winminor_dll)

#endif

#endif /* Not __STRICT_ANSI__ */


#define __need_size_t
#define __need_wchar_t
#define __need_NULL
#include <stddef.h>

#ifdef	__GNUC__
#define	_ATTRIB_NORETURN	__attribute__ ((noreturn))
#else	/* Not __GNUC__ */
#define	_ATTRIB_NORETURN
#endif	/* __GNUC__ */

double	atof	(const char* szNumber);
int	atoi	(const char* szNumber);
long	atol	(const char* szNumber);


double	strtod	(const char* szNumber, char** pszAfterNumber);
double	wcstod	(const wchar_t* wsNumber, wchar_t** pwsAfterNumber);
long	strtol	(const char* szNumber, char** pszAfterNumber, int nBase);
long	wcstol	(const wchar_t* wsNumber, wchar_t** pwsAfterNumber, int nBase);

unsigned long	strtoul	(const char* szNumber, char** pszAfterNumber,
			int nBase);
unsigned long	wcstoul (const wchar_t* wsNumber, wchar_t** pwsAfterNumber,
			int nBase);

size_t	wcstombs	(char* mbsDest, const wchar_t* wsConvert, size_t size);
int	wctomb		(char* mbDest, wchar_t wc);

int	mblen		(const char* mbs, size_t sizeString);
size_t	mbstowcs	(wchar_t* wcaDest, const char* mbsConvert,
			 size_t size);
int	mbtowc		(wchar_t* wcDest, const char* mbConvert, size_t size);

int	rand	();
void	srand	(unsigned int nSeed);

void*	calloc	(size_t sizeObjCnt, size_t sizeObject);
void*	malloc	(size_t	sizeObject);
void*	realloc	(void* pObject, size_t sizeNew);
void	free	(void* pObject);

void	abort	() _ATTRIB_NORETURN;
void	exit	(int nStatus) _ATTRIB_NORETURN;
int	atexit	(void (*pfuncExitProcessing)());

int	system	(const char* szCommand);
char*	getenv	(const char* szVarName);

typedef	int (*_pfunccmp_t)(const void*, const void*);

void*	bsearch	(const void* pKey, const void* pBase, size_t cntObjects,
		size_t sizeObject, _pfunccmp_t pfuncCmp);
void	qsort	(const void* pBase, size_t cntObjects, size_t sizeObject,
		_pfunccmp_t pfuncCmp);

int	abs	(int n);
long	labs	(long n);

/*
 * div_t and ldiv_t are structures used to return the results of div and
 * ldiv.
 *
 * NOTE: div and ldiv appear not to work correctly unless
 *       -fno-pcc-struct-return is specified. This is included in the
 *       mingw32 specs file.
 */
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;

div_t	div	(int nNumerator, int nDenominator);
ldiv_t	ldiv	(long lNumerator, long lDenominator);


#ifndef	__STRICT_ANSI__

/*
 * NOTE: Officially the three following functions are obsolete. The Win32 API
 *       functions SetErrorMode, Beep and Sleep are their replacements.
 */
void	_beep (unsigned int, unsigned int);
void	_seterrormode (int nMode);
void	_sleep (unsigned long ulTime);

void	_exit	(int nStatus) _ATTRIB_NORETURN;

int	_putenv	(const char* szNameEqValue);
void	_searchenv (const char* szFileName, const char* szVar,
		char* szFullPathBuf);

char*	_itoa (int nValue, char* sz, int nRadix);
char*	_ltoa (long lnValue, char* sz, int nRadix);

char*	_ecvt (double dValue, int nDig, int* pnDec, int* pnSign);
char*	_fcvt (double dValue, int nDig, int* pnDec, int* pnSign);
char*	_gcvt (double dValue, int nDec, char* caBuf);

void	_makepath (char* caPath, const char* szDrive, const char* szDir,
		const char* szName, const char* szExtension);
void	_splitpath (const char* szPath, char* caDrive, char* caDir,
		char* caName, char* caExtension);
char*	_fullpath (char* caBuf, const char* szPath, size_t sizeMax);

#ifndef	_NO_OLDNAMES
void	beep (unsigned int, unsigned int);
void	seterrormode (int nMode);
void	sleep (unsigned long ulTime);

int	putenv (const char* szNameEqValue);
void	searchenv (const char* szFileName, const char* szVar,
		char* szFullPathBuf);

char*	itoa (int nValue, char* sz, int nRadix);
char*	ltoa (long lnValue, char* sz, int nRadix);

char*	ecvt (double dValue, int nDig, int* pnDec, int* pnSign);
char*	fcvt (double dValue, int nDig, int* pnDec, int* pnSign);
char*	gcvt (double dValue, int nDec, char* caBuf);
#endif	/* Not _NO_OLDNAMES */

#endif	/* Not __STRICT_ANSI__ */

/*
 * Undefine the no return attribute used in some function definitions
 */
#undef	_ATTRIB_NORETURN

#ifdef __cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif	/* Not _STDLIB_H_ */

