/* General utility routines for the remote server for GDB.
   Copyright (C) 1986, 1989, 1993 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "server.h"

extern int target_byte_order;

/* Generally useful subroutines used throughout the program.  */

/* Print the system error message for errno, and also mention STRING
   as the file name for which the error was encountered.
   Then return to command level.  */

void
perror_with_name (string)
     char *string;
{
  extern int errno;
  char *err;
  char *combined;

  err = strerror(errno);
  
  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  output_error ("%s.\n", combined);
}


/*
 * convert_bytes_to_ascii
 *
 * This converts an array of N bytes in FROM to a null-terminated ascii hex
 * representation in TO.  If SWAP is 1 the bytes are swapped.
 */

void
convert_bytes_to_ascii (char *from, char * to, int n, int swap)
{
  int nib;
  char ch;
  while (n--)
    {
      if (swap)
	{
	  ch = *from++;
	  nib = ch & 0x0f;
	  *to++ = tohex (nib);
	  nib = ((ch & 0xf0) >> 4) & 0x0f;
	  *to++ = tohex (nib);
	}
      else
	{ 
	  ch = *from++;
	  nib = ((ch & 0xf0) >> 4) & 0x0f;
	  *to++ = tohex (nib);
	  nib = ch & 0x0f;
	  *to++ = tohex (nib);
	}
    }
  *to++ = 0;
}

/*
 * convert_ascii_to_bytes
 *
 * This converts an array of 2 * N ascii characters in FROM to N bytes
 * representation in TO.  If SWAP is 1 the nibbles in each byte are
 * swapped.
 */

void
convert_ascii_to_bytes (char *from, char *to, int n, int swap)
{
  int nib1, nib2;
  
  if (!swap)
    {
      while (n--)
	{
	  nib1 = fromhex (*from++);
	  nib2 = fromhex (*from++);
	  *to++ = (((nib1 & 0x0f) << 4) & 0xf0) | (nib2 & 0x0f);
	}
    }
  else
    {
      while (n--)
	{
	  nib1 = fromhex (*from++);
	  nib2 = fromhex (*from++);
	  *to++ = (((nib2 & 0x0f) << 4) & 0xf0) | (nib1 & 0x0f);
	}
    }
}

/*
 * FIXME: These are routines hacked out of findvar.c.  I should really
 * use that file, but I don't have the time to get it to build right now.
 */

void
store_unsigned_integer (PTR addr, int len, ULONGEST val)
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
  if (target_byte_order == BIG_ENDIAN)
    {
      for (p = endaddr - 1; p >= startaddr; --p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
    }
  else
    {
      for (p = startaddr; p < endaddr; ++p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
    }
}

ULONGEST
extract_unsigned_integer (PTR addr, int len)
{
  ULONGEST retval;
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
  if (target_byte_order == BIG_ENDIAN)
    {
      for (p = startaddr; p < endaddr; ++p)
	retval = (retval << 8) | *p;
    }
  else
    {
      for (p = endaddr - 1; p >= startaddr; --p)
	retval = (retval << 8) | *p;
    }
  return retval;
}

LONGEST
extract_signed_integer (PTR addr, int len)
{
  LONGEST retval;
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  /* Need to do better than this, but I don't want to longjump.  Really
   * should be an return status parameter.
   */
  
  if (len > (int) sizeof (LONGEST))
    {
      output_error ("\
That operation is not available on integers of more than %d bytes.",
		    sizeof (LONGEST));
      return 0;
    }

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  if (target_byte_order == BIG_ENDIAN)
    {
      p = startaddr;
      /* Do the sign extension once at the start.  */
      retval = ((LONGEST)*p ^ 0x80) - 0x80;
      for (++p; p < endaddr; ++p)
	retval = (retval << 8) | *p;
    }
  else
    {
      p = endaddr - 1;
      /* Do the sign extension once at the start.  */
      retval = ((LONGEST)*p ^ 0x80) - 0x80;
      for (--p; p >= startaddr; --p)
	retval = (retval << 8) | *p;
    }
  return retval;
}

/* Convert number NIB to a hex digit.  */

int
tohex (int nib)
{
  if (nib < 10)
    return '0' + nib;
  else
    return 'a' + nib - 10;
}

/* Convert hex digit A to a number.  Return -1 on error.*/

int
fromhex (int a)
{
  if (a >= '0' && a <= '9')
    {
      return a - '0';
    }
  else if (a >= 'a' && a <= 'f')
    {
      return a - 'a' + 10;
    }
  else
    {
      output_error ("Reply contains invalid hex digit");
      return -1;
    }
}
