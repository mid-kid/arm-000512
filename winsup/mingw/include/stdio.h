/*
 * stdio.h
 *
 * Definitions of types and prototypes of functions for standard input and
 * output.
 *
 * NOTE: The file manipulation functions provided by Microsoft seem to
 * work with either slash (/) or backslash (\) as the path separator.
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

#ifndef _STDIO_H_
#define	_STDIO_H_

#define __need_size_t
#define __need_NULL
#define __need_wchar_t
#define	__need_wint_t
#ifndef RC_INVOKED
#include <stddef.h>
#endif	/* Not RC_INVOKED */


/* Some flags for the iobuf structure provided by <paag@tid.es> */
#define	_IOREAD	1
#define	_IOWRT	2
#define	_IORW	4

/*
 * The three standard file pointers provided by the run time library.
 * NOTE: These will go to the bit-bucket silently in GUI applications!
 */
#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

/* Returned by various functions on end of file condition or error. */
#define	EOF	(-1)

/*
 * The maximum length of a file name. You should use GetVolumeInformation
 * instead of this constant. But hey, this works.
 *
 * NOTE: This is used in the structure _finddata_t (see dir.h) so changing it
 *       is probably not a good idea.
 */
#define	FILENAME_MAX	(260)

/*
 * The maximum number of files that may be open at once. I have set this to
 * a conservative number. The actual value may be higher.
 */
#define FOPEN_MAX	(20)

/*
 * The maximum size of name (including NUL) that will be put in the user
 * supplied buffer caName for tmpnam.
 * NOTE: This has not been determined by experiment, but based on the
 * maximum file name length above it is probably reasonable. I could be
 * wrong...
 */
#define	L_tmpnam	(260)

/*
 * The three possible buffering mode (nMode) values for setvbuf.
 * NOTE: _IOFBF works, but _IOLBF seems to work like unbuffered...
 * maybe I'm testing it wrong?
 */
#define	_IOFBF	0	/* fully buffered */
#define	_IOLBF	1	/* line buffered */
#define	_IONBF	2	/* unbuffered */

/*
 * The buffer size as used by setbuf such that it is equivalent to
 * (void) setvbuf(fileSetBuffer, caBuffer, _IOFBF, BUFSIZ).
 */
#define	BUFSIZ	512

/* Constants for nOrigin indicating the position relative to which fseek
 * sets the file position. Enclosed in ifdefs because io.h could also
 * define them. (Though not anymore since io.h includes this file now.) */
#ifndef	SEEK_SET
#define SEEK_SET	(0)
#endif

#ifndef	SEEK_CUR
#define	SEEK_CUR	(1)
#endif

#ifndef	SEEK_END
#define SEEK_END	(2)
#endif


#ifndef	RC_INVOKED

/*
 * I used to include stdarg.h at this point, in order to allow for the
 * functions later on in the file which use va_list. That conflicts with
 * using stdio.h and varargs.h in the same file, so I do the typedef myself.
 */
#ifndef _VA_LIST
#define _VA_LIST
typedef	char* va_list;
#endif

/*
 * The structure underlying the FILE type.
 *
 * I still believe that nobody in their right mind should make use of the
 * internals of this structure. Provided by Pedro A. Aranda Gutiirrez
 * <paag@tid.es>.
 */
#ifndef _FILE_DEFINED
#define	_FILE_DEFINED
typedef struct _iobuf
{
	char*	_ptr;
	int	_cnt;
	char*	_base;
	int	_flag;
	int	_file;
	int	_charbuf;
	int	_bufsiz;
	char*	_tmpfname;
} FILE;
#endif	/* Not _FILE_DEFINED */


/*
 * The standard file handles
 */
extern FILE (*__imp__iob)[];	/* A pointer to an array of FILE */

#define _iob	(*__imp__iob)	/* An array of FILE */

#define stdin	(&_iob[STDIN_FILENO])
#define stdout	(&_iob[STDOUT_FILENO])
#define stderr	(&_iob[STDERR_FILENO])


#ifdef __cplusplus
extern "C" {
#endif

/*
 * File Operations
 */

FILE*	fopen (const char* szFileName, const char* szMode);
FILE*	freopen (const char* szNewFileName, const char* szNewMode,
		 FILE* fileChangeAssociation);
int	fflush (FILE* fileFlush);
int	fclose (FILE* fileClose);
int	remove (const char* szFileName);
int	rename (const char* szOldFileName, const char* szNewFileName);
FILE*	tmpfile ();
char*	tmpnam (char caName[]);
char*	_tempnam (char* szPath, const char* szPrefix);

#ifndef	NO_OLDNAMES
char*	tempnam (char* szPath, const char* szPrefix);
#endif

int	setvbuf (FILE* fileSetBuffer, char* caBuffer, int nMode,
		 size_t sizeBuffer);

void	setbuf (FILE* fileSetBuffer, char* caBuffer);

/*
 * Formatted Output
 */

int	fprintf (FILE* filePrintTo, const char* szFormat, ...);
int	printf (const char* szFormat, ...);
int	sprintf (char* caBuffer, const char* szFormat, ...);
int	vfprintf (FILE* filePrintTo, const char* szFormat, va_list varg);
int	vprintf (const char* szFormat, va_list varg);
int	vsprintf (char* caBuffer, const char* szFormat, va_list varg);

/* Wide character versions */
int	fwprintf (FILE* filePrintTo, const wchar_t* wsFormat, ...);
int	wprintf (const wchar_t* wsFormat, ...);
int	swprintf (wchar_t* wcaBuffer, const wchar_t* wsFormat, ...);
int	vfwprintf (FILE* filePrintTo, const wchar_t* wsFormat, va_list varg);
int	vwprintf (const wchar_t* wsFormat, va_list varg);
int	vswprintf (wchar_t* wcaBuffer, const wchar_t* wsFormat, va_list varg);

/*
 * Formatted Input
 */

int	fscanf (FILE* fileReadFrom, const char* szFormat, ...);
int	scanf (const char* szFormat, ...);
int	sscanf (const char* szReadFrom, const char* szFormat, ...);

/* Wide character versions */
int	fwscanf (FILE* fileReadFrom, const wchar_t* wsFormat, ...);
int	wscanf (const wchar_t* wsFormat, ...);
int	swscanf (wchar_t* wsReadFrom, const wchar_t* wsFormat, ...);

/*
 * Character Input and Output Functions
 */

int	fgetc (FILE* fileRead);
char*	fgets (char* caBuffer, int nBufferSize, FILE* fileRead);
int	fputc (int c, FILE* fileWrite);
int	fputs (const char* szOutput, FILE* fileWrite);
int	getc (FILE* fileRead);
int	getchar ();
char*	gets (char* caBuffer);	/* Unsafe: how does gets know how long the
				 * buffer is? */
int	putc (int c, FILE* fileWrite);
int	putchar (int c);
int	puts (const char* szOutput);
int	ungetc (int c, FILE* fileWasRead);

/* Wide character versions */
wint_t	fgetwc (FILE* fileRead);
wint_t	fputwc (wchar_t wc, FILE* fileWrite);
wint_t	ungetwc (wchar_t wc, FILE* fileWasRead);


/*
 * Not exported by CRTDLL.DLL included for reference purposes.
 */
#if 0
wchar_t*	fgetws (wchar_t* wcaBuffer, int nBufferSize, FILE* fileRead);
int		fputws (const wchar_t* wsOutput, FILE* fileWrite);
int		getwc (FILE* fileRead);
int		getwchar ();
wchar_t*	getws (wchar_t* wcaBuffer);
int		putwc (wchar_t wc, FILE* fileWrite);
int		putws (const wchar_t* wsOutput);
#endif	/* 0 */

/* NOTE: putchar has no wide char equivalent even in tchar.h */


/*
 * Direct Input and Output Functions
 */

size_t	fread (void* pBuffer, size_t sizeObject, size_t sizeObjCount,
		FILE* fileRead);
size_t	fwrite (const void* pObjArray, size_t sizeObject, size_t sizeObjCount,
		FILE* fileWrite);


/*
 * File Positioning Functions
 */

int	fseek	(FILE* fileSetPosition, long lnOffset, int nOrigin);
long	ftell	(FILE* fileGetPosition);
void	rewind	(FILE* fileRewind);

/*
 * An opaque data type used for storing file positions... The contents of
 * this type are unknown, but we (the compiler) need to know the size
 * because the programmer using fgetpos and fsetpos will be setting aside
 * storage for fpos_t structres. Actually I tested using a byte array and
 * it is fairly evident that the fpos_t type is a long (in CRTDLL.DLL).
 * Perhaps an unsigned long? TODO?
 */
typedef long	fpos_t;

int	fgetpos	(FILE* fileGetPosition, fpos_t* pfpos);
int	fsetpos (FILE* fileSetPosition, fpos_t* pfpos);

/*
 * Error Functions
 */

void	clearerr (FILE* fileClearErrors);
int	feof (FILE* fileIsAtEnd);
int	ferror (FILE* fileIsError);
void	perror (const char* szErrorMessage);


#ifndef __STRICT_ANSI__

/*
 * Pipes
 */
FILE*	_popen (const char* szPipeName, const char* szMode);
int	_pclose (FILE* pipeClose);

#ifndef NO_OLDNAMES
FILE*	popen (const char* szPipeName, const char* szMode);
int	pclose (FILE* pipeClose);
#endif

/* The wide character version, only available in MSVCRT DLL versions, not
 * CRTDLL. */
#ifdef __MSVCRT__
FILE*	_wpopen (const wchar_t* szPipeName, const wchar_t* szMode);

#ifndef NO_OLDNAMES
#if 0
FILE*	wpopen (const wchar_t* szPipeName, const wchar_t* szMode);
#else /* Always true */
/*
 * The above prototypeing is not possible unless the wpopen export is added
 * to moldnames, which can't be done unless we make separate moldnames.def
 * files for every supported runtime. For the time being we use a define
 * instead. Pedro's modified dlltool should take care of this I think.
 */
#define wpopen _wpopen
#endif	/* Always true */

#endif /* not NO_OLDNAMES */
#endif /* MSVCRT runtime */

/*
 * Other Non ANSI functions
 */
int	_fgetchar ();
int	_fputchar (int c);
FILE*	_fdopen (int nHandle, const char* szMode);
wint_t	_fgetwchar(void);
wint_t	_fputwchar(wint_t c);
int	_fileno (FILE* fileGetHandle);
int	_getw (FILE*);
int	_putw (int, FILE*);

#ifndef _NO_OLDNAMES
int	fgetchar ();
int	fputchar (int c);
FILE*	fdopen (int nHandle, const char* szMode);
wint_t	fgetwchar(void);
wint_t	fputwchar(wint_t c);
int	fileno (FILE* fileGetHandle);
int	getw (FILE*);
int	putw (int, FILE*);
#endif	/* Not _NO_OLDNAMES */

#endif	/* Not __STRICT_ANSI__ */

#ifdef __cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif /* _STDIO_H_ */
