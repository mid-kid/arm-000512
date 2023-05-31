/* smallprint.c: small print routines for WIN32

   Copyright 1996, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>

int __small_sprintf (char *dst, const char *fmt,...);
int __small_vsprintf (char *dst, const char *fmt, va_list ap);

static char *
rn (char *dst, int base, int dosign, int val, int len, int pad)
{
  /* longest number is 4294967295, 10 digits */
  unsigned uval;
  char res[10];
  static const char str[16] = "0123456789ABCDEF";
  int l = 0;

  if (dosign && val < 0)
    {
      *dst++ = '-';
      uval = -val;
    }
  else
    {
      uval = val;
    }
  do
    {
      res[l++] = str[uval % base];
      uval /= base;
    }
  while (uval);

  while (len -- > l)
    *dst++ = pad;

  while (l > 0)
    {
      *dst++ = res[--l];
    }

  return dst;
}

int
__small_vsprintf (char *dst, const char *fmt, va_list ap)
{
  char tmp[MAX_PATH + 1];
  char *orig = dst;
  const char *s;

  while (*fmt)
    {
      int i, n = 0x7fff;
      if (*fmt != '%')
	*dst++ = *fmt++;
      else
	{
	  int len = 0;
	  char pad = ' ';

	  fmt++;

	  if (*fmt == '0') 
	    {
	      pad = '0';
	      fmt++;
	    }
	  
	  if (*fmt >= '1' && *fmt <= '9')
	    len = *fmt++ - '0';

	  switch (*fmt++)
	    {
	    case 'c':
	      {
		int c = va_arg (ap,int);
		if (c > ' ' && c <= 127)
		  *dst++ = c;
		else
		  {
		    *dst++ = '0';
		    *dst++ = 'x';
		    dst = rn (dst, 16, 0, c, len, pad);
		  }
	      }
	      break;
	    case 'E':
	      strcpy (dst, "error ");
	      dst = rn (dst + 6, 10, 1, GetLastError (), len, pad);
	      break;
	    case 'd':
	      dst = rn (dst, 10, 1, va_arg (ap, int), len, pad);
	      break;
	    case 'u':
	      dst = rn (dst, 10, 0, va_arg (ap, int), len, pad);
	      break;
	    case 'p':
	      *dst++ = '0';
	      *dst++ = 'x';
	      /* fall through */
	    case 'x':
	      dst = rn (dst, 16, 0, va_arg (ap, int), len, pad);
	      break;
	    case 'P':
	      if (!GetModuleFileName (NULL, tmp, MAX_PATH))
		s = "cygwin program";
	      else
		s = tmp;
	      goto fillin;
	    case '.':
	      n = strtol (fmt, (char **)&fmt, 10);
	      if (*fmt++ != 's')
		continue;
	    case 's':
	      s = va_arg (ap, char *);
	      if (s == NULL)
		s = "(null)";
	    fillin:
	      for (i = 0; *s && i < n; i++)
		*dst++ = *s++;
	      break;
	    case 'F':
	    {
	      const char *p, *pe;
	      s = va_arg (ap, char *);
	      for (p = s; (pe = strchr (p, '(')); p = pe + 1)
		if (isalnum ((int)pe[-1]) || pe[-1] == '_')
		  break;
	      if (!pe)
		pe = strchr (s, '\0');
	      for (p = pe; p > s; p--)
		if (*p == ' ')
		  {
		    p++;
		    break;
		  }
	      while (p < pe)
		*dst++ = *p++;
	      break;
	    }
	    default:
	      *dst++ = '?';
	      *dst++ = fmt[-1];
	    }
	}
    }
  *dst = 0;
  return dst - orig;
}

int
__small_sprintf (char *dst, const char *fmt,...)
{
  int r;
  va_list ap;
  va_start (ap, fmt);
  r = __small_vsprintf (dst, fmt, ap);
  va_end (ap);
  return r;
}
