/* Remote utility routines for the remote server for GDB.
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

#if defined(__CYGWIN32__) && !defined(__CYGWIN__)
#define __CYGWIN__
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#ifndef __CYGWIN__ /* Cygwin does not have netint/tcp.h */
#include <netinet/tcp.h>
#endif
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include "../defs.h"
#include "server.h"
#include "remote-utils.h"

int remote_debug = 0;

static int remote_desc = -1;
static int listener_desc = -1;

/* Functions used only in this file */

void enable_async_io();
void disable_async_io();
void close_and_exit();

/* Open a listening socket for connections from a remote debugger.
   PORT_STR is the port number to use.  */

int
open_listener (port_str)
     char *port_str;
{
  int port;
  char *ptr;
  struct sockaddr_in sockaddr;
  int tmp;
    
  port = (int) strtol (port_str, &ptr, 10);
  if (ptr == port_str)
    {
      output ("Invalid port number: %s\n", port_str);
      return 0;
    }
  
  listener_desc = socket (PF_INET, SOCK_STREAM, 0);
  if (listener_desc < 0)
    {
      perror_with_name ("Can't open socket");
      return 0;
    }
  
  /* Allow rapid reuse of this port. */
  tmp = 1;
  setsockopt (listener_desc, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp,
	      sizeof(tmp));
  
  sockaddr.sin_family = PF_INET;
  sockaddr.sin_port = htons(port);
  sockaddr.sin_addr.s_addr = INADDR_ANY;
  
  if (bind (listener_desc, (struct sockaddr *)&sockaddr, sizeof (sockaddr))
      || listen (listener_desc, 1))
    {
      perror_with_name ("Can't bind address");
      return 0;
    }
  
  return 1;
}

int
wait_for_connection()
{
  struct sockaddr_in sockaddr;
  struct hostent *hostEntPtr;
  unsigned int save_fcntl_flags;
  int enable_async = 1;
  int tmp;
  struct protoent *protoent;
	
  tmp = sizeof (sockaddr);
  remote_desc = accept (listener_desc, (struct sockaddr *)&sockaddr, &tmp);

  if (remote_desc == -1)
    {
      perror_with_name ("Accept failed");
      return 0;
    }
  
  protoent = getprotobyname ("tcp");
  if (!protoent)
    {
      perror_with_name ("getprotobyname");
      return 0;
    }
  
  /* Enable TCP keep alive process. */
  tmp = 1;
  setsockopt (remote_desc, SOL_SOCKET, SO_KEEPALIVE,
	      (char *)&tmp, sizeof(tmp));
  
  /* Tell TCP not to delay small packets.  This greatly speeds up
     interactive response. */
  tmp = 1;
  setsockopt (remote_desc, protoent->p_proto, TCP_NODELAY,
	      (char *)&tmp, sizeof(tmp));
    
  signal (SIGPIPE, SIG_IGN); /* If we don't do this, then gdbserver simply
				exits when the remote side dies.  */
  
#if defined(F_SETFL) && defined (FASYNC)
  save_fcntl_flags = fcntl (remote_desc, F_GETFL, 0);
  save_fcntl_flags |= FASYNC;
#if defined(O_ASYNC)
  save_fcntl_flags |= O_ASYNC;
#endif
  fcntl (remote_desc, F_SETFL, save_fcntl_flags);
  ioctl (remote_desc, FIOASYNC, &enable_async);
  disable_async_io ();
#endif /* FASYNC */
  tmp = sizeof (sockaddr);
  
  if (getpeername(remote_desc, (struct sockaddr *) &sockaddr, &tmp) >= 0)
    {
      hostEntPtr = gethostbyaddr((char *) &(sockaddr.sin_addr),
		    sizeof(sockaddr.sin_addr), AF_INET);
      if (hostEntPtr != (struct hostent *) NULL)
	{
	  output ("Got a connection from %s\n", hostEntPtr->h_name);
	}
      else
	{
	  output ("Got a connection from %s\n", inet_ntoa(sockaddr.sin_addr));
	}
    }
  else
    {
      output ("Got a connection\n");
    }

  return 1;
}

void
close_connection()
{
  if (remote_desc == -1)
    return;
  
  output ("Closing connection...\n");
  close (remote_desc);
  remote_desc = -1;
}

/* Send a packet to the remote machine, with error checking.
   The data of the packet is in BUF.  Returns >= 0 on success, -1 otherwise. */

int
putpkt (buf)
     char *buf;
{
  int i;
  unsigned char csum = 0;
  char buf2[2000];
  char buf3[1];
  int cnt = strlen (buf);
  char *p;

  /* Copy the packet into buffer BUF2, encapsulating it
     and giving it a checksum.  */

  p = buf2;
  *p++ = '$';

  for (i = 0; i < cnt; i++)
    {
      csum += buf[i];
      *p++ = buf[i];
    }
  *p++ = '#';
  *p++ = tohex ((csum >> 4) & 0xf);
  *p++ = tohex (csum & 0xf);

  *p = '\0';

  if (debug_on)
    output ("Sending: %s\n", (char *) buf2);

  /* Send it over and over until we get a positive ack.  */

  do
    {
      int cc;

      if (write (remote_desc, buf2, p - buf2) != p - buf2)
	{
	  perror ("putpkt(write)");
	  return -1;
	}

      if (remote_debug)
	printf ("putpkt (\"%s\"); [looking for ack]\n", buf2);
      do {
      cc = read (remote_desc, buf3, 1);
      if (cc <= 0)
	{
	  if (cc == 0)
	    fprintf (stderr, "putpkt(read): Got EOF\n");
	  else
	    perror ("putpkt(read)");

	  return -1;
	}
      if (remote_debug)
	printf ("[received '%c' (0x%x)]\n", buf3[0], buf3[0]);
      } while (buf3[0] == 0x03);  /* Ignore ^C here */
    }
  while (buf3[0] != '+');

  return 1;			/* Success! */
}

/* Come here when we get an input interrupt from the remote side.  This
   interrupt should only be active while we are waiting for the child to do
   something.  About the only thing that should come through is a ^C, which
   will cause us to send a SIGINT to the child.  */

static void
input_interrupt()
{
  int cc;
  char c;

#if 0
  cc = read (remote_desc, &c, 1);

  if (cc != 1 || c != '\003')
    {
      fprintf(stderr, "input_interrupt, cc = %d c = %d\n", cc, c);
      return;
    }
#endif
  /* kill (inferior_pid, SIGINT); */
  low_stop();
}


void
enable_async_io()
{
  signal (SIGIO, input_interrupt);
}

void
disable_async_io()
{
  signal (SIGIO, SIG_IGN);
}

/* Since the SIGIO on sockets doesn't seem to work, try and emulate it */

void
check_for_SIGIO(void)
{
    fd_set sock_fds;
    struct timeval wait_time;

    FD_ZERO(&sock_fds);
    FD_SET(remote_desc, &sock_fds);
    wait_time.tv_sec = 0;
    wait_time.tv_usec = 0;
    if (select(remote_desc+1, &sock_fds, NULL, NULL, &wait_time) > 0) {
        /* There is data to be read on 'remote_desc' */
        kill(getpid(), SIGIO);
    }
}

/* Returns next char from remote GDB.  -1 if error.  */

static int
readchar ()
{
  static char buf[BUFSIZ];
  static int bufcnt = 0;
  static char *bufp;

  if (bufcnt-- > 0)
    return *bufp++ & 0x7f;

  bufcnt = read (remote_desc, buf, sizeof (buf));

  if (bufcnt <= 0)
    {
      if (bufcnt == 0)
	fprintf (stderr, "readchar: Got EOF\n");
      else
	perror ("readchar");

      return -1;
    }

  bufp = buf;
  bufcnt--;
  return *bufp++ & 0x7f;
}

/* Read a packet from the remote machine, with error checking,
   and store it in BUF.  Returns length of packet, or negative if error.
   buf_len is on input the buffer length.  Need to do something about
   buffer overflows, which the previous server ignored.  Not done yet.
*/

int
getpkt (buf, buf_len)
     char *buf;
     int *buf_len;
{
  char *bp;
  unsigned char csum, c1, c2;
  int c;

  while (1)
    {
      csum = 0;

      while (1)
	{
	  c = readchar ();
	  if (c == '$')
	    break;
	  if (remote_debug)
	    output ("[getpkt: discarding char '%c']\n", c);
	  if (c < 0)
	    return -1;
	}

      bp = buf;
      while (1)
	{
	  c = readchar ();
	  if (c < 0)
	    return -1;
	  if (c == '#')
	    break;
	  *bp++ = c;
	  csum += c;
	}
      *bp = 0;

      c1 = fromhex (readchar ());
      c2 = fromhex (readchar ());
      
      if (csum == (c1 << 4) + c2)
	break;

      fprintf (stderr, "Bad checksum, sentsum=0x%x, csum=0x%x, buf=%s\n",
	       (c1 << 4) + c2, csum, buf);
      write (remote_desc, "-", 1);
    }

  if (remote_debug)
    printf ("getpkt (\"%s\");  [sending ack] \n", buf);

  write (remote_desc, "+", 1);

  if (remote_debug)
    printf ("[sent ack]\n");
  return bp - buf;
}

static char *
outreg(int regno, char *buf)
{
  extern char registers[];
  int regsize = REGISTER_RAW_SIZE (regno);

  *buf++ = tohex (regno >> 4);
  *buf++ = tohex (regno & 0xf);
  *buf++ = ':';
  convert_bytes_to_ascii (&registers[REGISTER_BYTE (regno)], buf, regsize, 0);
  buf += 2 * regsize;
  *buf++ = ';';

  return buf;
}


void
prepare_resume_reply (char *buf, char status, unsigned char signo)
{
  int nib;

  *buf++ = status;

  /* FIXME!  Should be converting this signal number (numbered
     according to the signal numbering of the system we are running on)
     to the signal numbers used by the gdb protocol (see enum target_signal
     in gdb/target.h).  */
  
  nib = ((signo & 0xf0) >> 4);
  *buf++ = tohex (nib);
  nib = signo & 0x0f;
  *buf++ = tohex (nib);

  if (status == 'T')
    {
      buf = outreg (PC_REGNUM, buf);
      buf = outreg (FP_REGNUM, buf);
      buf = outreg (SP_REGNUM, buf);
#ifdef NPC_REGNUM
      buf = outreg (NPC_REGNUM, buf);
#endif
#ifdef O7_REGNUM
      buf = outreg (O7_REGNUM, buf);
#endif

#if 0 /* FIXME - what is this code for?  Look in original gdbserver */
      
      /* If the debugger hasn't used any thread features, don't burden it with
	 threads.  If we didn't check this, GDB 4.13 and older would choke.  */
      if (cont_thread != 0)
	{
	  if (old_thread_from_wait != thread_from_wait)
	    {
	      sprintf (buf, "thread:%x;", thread_from_wait);
	      buf += strlen (buf);
	      old_thread_from_wait = thread_from_wait;
	    }
	}
#endif
    }
  
  /* For W and X, we're done.  */
  *buf++ = 0;
}

void decode_m_packet (char *from,
		      CORE_ADDR *mem_addr_ptr,
		      unsigned int *len_ptr)
{
  int i = 0, j = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  for (j = 0; j < 4; j++)
    {
      if ((ch = from[i++]) == 0)
	break;
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }
}

void
decode_M_packet (char *from, char *to,
		 CORE_ADDR *mem_addr_ptr,
		 unsigned int *len_ptr)
{
  int i = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  while ((ch = from[i++]) != ':')
    {
      *len_ptr = *len_ptr << 4;
      /* FIXME: Not checking for error from fromhex here... */
      *len_ptr |= fromhex (ch) & 0x0f;
    }

  convert_ascii_to_bytes (&from[i++], to, *len_ptr, 0);
}

void
close_listener (void)
{
  if (listener_desc == -1)
    return;

  close (listener_desc);
  listener_desc = -1;
}
