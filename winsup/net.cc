/* net.cc: network-related routines.

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

/* #define DEBUG_NEST_ON 1 */

#define  __INSIDE_CYGWIN_NET__

#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#define Win32_Winsock
#include "winsup.h"
#include <unistd.h>

/* We only want to initialize WinSock in a child process if socket
   handles are inheritted. This global allows us to know whether this
   should be done or not */
int number_of_sockets = 0;

extern "C"
{
int h_errno;
}

/* Handle of wsock32.dll */
HANDLE wsock32;

/* Pointers to wsock32.dll exports */
int (*i_WSAAsyncSelect) (SOCKET s, HWND hWnd, u_int wMsg, long lEvent) NO_COPY PASCAL;
int (*i_WSACleanup) (void) NO_COPY PASCAL;
int (*i_WSAGetLastError) (void) NO_COPY PASCAL;
int (*i_WSAStartup) (int wVersionRequired, LPWSADATA lpWSAData) NO_COPY PASCAL;
int (*i___WSAFDIsSet) (SOCKET, fd_set *) NO_COPY PASCAL;
SOCKET (*i_accept) (SOCKET s, struct sockaddr *addr,int *addrlen) NO_COPY PASCAL;
int (*i_bind) (SOCKET s, const struct sockaddr *addr, int namelen) NO_COPY PASCAL;
int (*i_closesocket) (SOCKET s) NO_COPY PASCAL;
int (*i_connect) (SOCKET s, const struct sockaddr *name, int namelen) NO_COPY PASCAL;
int (*i_gethostname) (char * name, size_t len) NO_COPY PASCAL;
struct hostent * (*i_gethostbyaddr) (const char * addr, int, int) NO_COPY PASCAL;
struct hostent * (*i_gethostbyname) (const char * name) NO_COPY PASCAL;
int (*i_getpeername) (SOCKET s, struct sockaddr *name, int * namelen) NO_COPY PASCAL;
struct protoent * (*i_getprotobyname) (const char * name) NO_COPY PASCAL;
struct protoent * (*i_getprotobynumber) (int number) NO_COPY PASCAL;
struct servent * (*i_getservbyname) (const char * name,	const char * proto) NO_COPY PASCAL;
struct servent * (*i_getservbyport) (int port, const char * proto) NO_COPY PASCAL;
int (*i_getsockname) (SOCKET s, struct sockaddr *name, int * namelen) NO_COPY PASCAL;
int (*i_getsockopt) (SOCKET s, int level, int optname,
		     char * optval, int *optlen) NO_COPY PASCAL;
unsigned long (*i_inet_addr) (const char * cp) NO_COPY PASCAL;
char * (*i_inet_ntoa) (struct in_addr in) NO_COPY PASCAL;
int (*i_ioctlsocket) (SOCKET s, long cmd, u_long *argp) NO_COPY PASCAL;
int (*i_listen) (SOCKET s, int backlog) NO_COPY PASCAL;
int (*i_recv) (SOCKET s, char * buf, int len, int flags) NO_COPY PASCAL;
int (*i_recvfrom) (SOCKET s, char * buf, int len, int flags,
		   struct sockaddr  *from, int * fromlen) NO_COPY PASCAL;
int (*i_select) (int nfds, fd_set *readfds, fd_set *writefds,
		 fd_set *exceptfds, const struct timeval *timeout) NO_COPY PASCAL;
int (*i_send) (SOCKET s, const char * buf, int len, int flags) NO_COPY PASCAL;
int (*i_sendto) (SOCKET s, const char  * buf, int len, int flags,
		 const struct sockaddr  *to, int tolen) NO_COPY PASCAL;
int (*i_setsockopt) (SOCKET s, int level, int optname,
		     const char * optval, int optlen) NO_COPY PASCAL;
int (*i_shutdown) (SOCKET s, int how) NO_COPY PASCAL;
SOCKET (*i_socket) (int af, int type, int protocol) NO_COPY PASCAL;

/* These calls are non-standard, and may not present in non-MS IP stacks */
SOCKET (*i_rcmd) (char **ahost, unsigned short inport, char *locuser,
		  char *remuser, char *cmd, SOCKET *fd2p) NO_COPY PASCAL;
SOCKET (*i_rexec) (char **host, unsigned short port, char *user,
		   char *passwd, char *cmd, SOCKET *fd2p) NO_COPY PASCAL;
SOCKET (*i_rresvport) (int *port) NO_COPY PASCAL;

#define LOAD(name) if ((*(int(**)())&i_##name = GetProcAddress (wsock32, #name)) == NULL) \
		     {							      \
			system_printf ("Can't link to %s", #name);	      \
			ExitProcess (1);				      \
		     }

/* Cygwin internal */
/* Initialize WinSock */
void
winsock_init ()
{
  WSADATA p;
  int res;

  if ((wsock32 = LoadLibrary ("wsock32.dll")) == NULL)
    api_fatal ("could not load wsock32.dll.  Is TCP/IP installed?");

  /* Explicitly load Winsock functions used by Cygwin */
  LOAD (WSAAsyncSelect)
  LOAD (WSACleanup)
  LOAD (WSAGetLastError)
  LOAD (WSAStartup)
  LOAD (__WSAFDIsSet)
  LOAD (accept)
  LOAD (bind)
  LOAD (closesocket)
  LOAD (connect)
  LOAD (gethostname)
  LOAD (gethostbyaddr)
  LOAD (gethostbyname)
  LOAD (getpeername)
  LOAD (getprotobyname)
  LOAD (getprotobynumber)
  LOAD (getservbyname)
  LOAD (getservbyport)
  LOAD (getsockname)
  LOAD (getsockopt)
  LOAD (inet_addr)
  LOAD (inet_ntoa)
  LOAD (ioctlsocket)
  LOAD (listen)
  LOAD (recv)
  LOAD (recvfrom)
  LOAD (select)
  LOAD (send)
  LOAD (sendto)
  LOAD (setsockopt)
  LOAD (shutdown)
  LOAD (socket)

  res = (*i_WSAStartup) ((2<<8) | 2, &p);

  debug_printf ("res %d", res);
  debug_printf ("wVersion %d", p.wVersion);
  debug_printf ("wHighVersion %d", p.wHighVersion);
  debug_printf ("szDescription %s",p.szDescription);
  debug_printf ("szSystemStatus %s",p.szSystemStatus);
  debug_printf ("iMaxSockets %d", p.iMaxSockets);
  debug_printf ("iMaxUdpDg %d", p.iMaxUdpDg);
  debug_printf ("lpVendorInfo %d", p.lpVendorInfo);

  if (FIONBIO  != REAL_FIONBIO)
    debug_printf ("****************  FIONBIO  != REAL_FIONBIO");

  myself->process_state |= PID_SOCKETS_USED;

  return;
  MARK ();
}

/* Cygwin internal */
static SOCKET
duplicate_socket (SOCKET sock)
{
  /* Do not duplicate socket on Windows NT because of problems with
     MS winsock proxy server.
  */
  if (os_being_run == winNT)
    return sock;

  SOCKET newsock;
  if (DuplicateHandle (GetCurrentProcess(), (HANDLE) sock,
			 GetCurrentProcess(), (HANDLE *) &newsock,
			 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
      (*i_closesocket) (sock);
      sock = newsock;
    }
  else
    small_printf ("DuplicateHandle failed %E");
  return sock;
}

/* htonl: standards? */
extern "C"
unsigned long int
htonl (unsigned long int x)
{
  MARK ();
  return ((((x & 0x000000ffU) << 24) |
	   ((x & 0x0000ff00U) <<  8) |
	   ((x & 0x00ff0000U) >>  8) |
	   ((x & 0xff000000U) >> 24)));
}

/* ntohl: standards? */
extern "C"
unsigned long int
ntohl (unsigned long int x)
{
  return htonl (x);
}

/* htons: standards? */
extern "C"
unsigned short
htons (unsigned short x)
{
  MARK ();
  return ((((x & 0x000000ffU) << 8) |
	   ((x & 0x0000ff00U) >> 8)));
}

/* ntohs: standards? */
extern "C"
unsigned short
ntohs (unsigned short x)
{
  return htons (x);
}

/* Cygwin internal */
static void
dump_protoent (struct protoent *p)
{
  if (p)
    debug_printf ("protoent %s %x %x", p->p_name, p->p_aliases, p->p_proto);
}

/* exported as inet_ntoa: standards? */
extern "C"
char *
cygwin_inet_ntoa (struct in_addr in)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("inet_ntoa");
  char *res = (*i_inet_ntoa) (in);
  out ("inet_ntoa");
  return res;
}

/* exported as inet_addr: standards? */
extern "C"
unsigned long
cygwin_inet_addr (const char *cp)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("inet_addr");
  unsigned long res = (*i_inet_addr) (cp);
  out ("inet_addr");
  return res;
}

/* inet_netof is in the standard BSD sockets library.  It is useless
   for modern networks, since it assumes network values which are no
   longer meaningful, but some existing code calls it.  */

extern "C"
unsigned long
inet_netof (struct in_addr in)
{
  unsigned long i, res;

  in ("inet_netof");

  i = ntohl (in.s_addr);
  if (IN_CLASSA (i))
    res = (i & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT;
  else if (IN_CLASSB (i))
    res = (i & IN_CLASSB_NET) >> IN_CLASSB_NSHIFT;
  else
    res = (i & IN_CLASSC_NET) >> IN_CLASSC_NSHIFT;

  out ("inet_netof");

  return res;
}

/* inet_makeaddr is in the standard BSD sockets library.  It is
   useless for modern networks, since it assumes network values which
   are no longer meaningful, but some existing code calls it.  */

extern "C"
struct in_addr
inet_makeaddr (int net, int lna)
{
  unsigned long i;
  struct in_addr in;

  in ("inet_makeaddr");

  if (net < IN_CLASSA_MAX)
    i = (net << IN_CLASSA_NSHIFT) | (lna & IN_CLASSA_HOST);
  else if (net < IN_CLASSB_MAX)
    i = (net << IN_CLASSB_NSHIFT) | (lna & IN_CLASSB_HOST);
  else if (net < 0x1000000)
    i = (net << IN_CLASSC_NSHIFT) | (lna & IN_CLASSC_HOST);
  else
    i = net | lna;

  in.s_addr = htonl (i);

  out ("inet_makeaddr");

  return in;
}

struct tl
{
  int w;
  const char *s;
  int e;
};

static struct tl errmap[] =
{
 {WSAEWOULDBLOCK, "WSAEWOULDBLOCK", EWOULDBLOCK},
 {WSAEINPROGRESS, "WSAEINPROGRESS", EINPROGRESS},
 {WSAEALREADY, "WSAEALREADY", EALREADY},
 {WSAENOTSOCK, "WSAENOTSOCK", ENOTSOCK},
 {WSAEDESTADDRREQ, "WSAEDESTADDRREQ", EDESTADDRREQ},
 {WSAEMSGSIZE, "WSAEMSGSIZE", EMSGSIZE},
 {WSAEPROTOTYPE, "WSAEPROTOTYPE", EPROTOTYPE},
 {WSAENOPROTOOPT, "WSAENOPROTOOPT", ENOPROTOOPT},
 {WSAEPROTONOSUPPORT, "WSAEPROTONOSUPPORT", EPROTONOSUPPORT},
 {WSAESOCKTNOSUPPORT, "WSAESOCKTNOSUPPORT", ESOCKTNOSUPPORT},
 {WSAEOPNOTSUPP, "WSAEOPNOTSUPP", EOPNOTSUPP},
 {WSAEPFNOSUPPORT, "WSAEPFNOSUPPORT", EPFNOSUPPORT},
 {WSAEAFNOSUPPORT, "WSAEAFNOSUPPORT", EAFNOSUPPORT},
 {WSAEADDRINUSE, "WSAEADDRINUSE", EADDRINUSE},
 {WSAEADDRNOTAVAIL, "WSAEADDRNOTAVAIL", EADDRNOTAVAIL},
 {WSAENETDOWN, "WSAENETDOWN", ENETDOWN},
 {WSAENETUNREACH, "WSAENETUNREACH", ENETUNREACH},
 {WSAENETRESET, "WSAENETRESET", ENETRESET},
 {WSAECONNABORTED, "WSAECONNABORTED", ECONNABORTED},
 {WSAECONNRESET, "WSAECONNRESET", ECONNRESET},
 {WSAENOBUFS, "WSAENOBUFS", ENOBUFS},
 {WSAEISCONN, "WSAEISCONN", EISCONN},
 {WSAENOTCONN, "WSAENOTCONN", ENOTCONN},
 {WSAESHUTDOWN, "WSAESHUTDOWN", ESHUTDOWN},
 {WSAETOOMANYREFS, "WSAETOOMANYREFS", ETOOMANYREFS},
 {WSAETIMEDOUT, "WSAETIMEDOUT", ETIMEDOUT},
 {WSAECONNREFUSED, "WSAECONNREFUSED", ECONNREFUSED},
 {WSAELOOP, "WSAELOOP", ELOOP},
 {WSAENAMETOOLONG, "WSAENAMETOOLONG", ENAMETOOLONG},
 {WSAEHOSTDOWN, "WSAEHOSTDOWN", EHOSTDOWN},
 {WSAEHOSTUNREACH, "WSAEHOSTUNREACH", EHOSTUNREACH},
 {WSAENOTEMPTY, "WSAENOTEMPTY", ENOTEMPTY},
 {WSAEPROCLIM, "WSAEPROCLIM", EPROCLIM},
 {WSAEUSERS, "WSAEUSERS", EUSERS},
 {WSAEDQUOT, "WSAEDQUOT", EDQUOT},
 {WSAESTALE, "WSAESTALE", ESTALE},
 {WSAEREMOTE, "WSAEREMOTE", EREMOTE},
 {WSAEINVAL, "WSAEINVAL", EINVAL},
 {WSAEFAULT, "WSAEFAULT", EFAULT},
 {0}
};

/* Cygwin internal */
static void
set_winsock_errno ()
{
  int i;
  int why = (*i_WSAGetLastError) ();
  for (i = 0; errmap[i].w != 0; ++i)
    if (why == errmap[i].w)
      break;

  if (errmap[i].w != 0)
    {
      syscall_printf ("%d (%s) -> %d", why, errmap[i].s, errmap[i].e);
      set_errno (errmap[i].e);
    }
  else
    {
      syscall_printf ("unknown error %d", why);
      set_errno (EPERM);
    }
}

static struct tl host_errmap[] =
{
  {WSAHOST_NOT_FOUND, "WSAHOST_NOT_FOUND", HOST_NOT_FOUND},
  {WSATRY_AGAIN, "WSATRY_AGAIN", TRY_AGAIN},
  {WSANO_RECOVERY, "WSANO_RECOVERY", NO_RECOVERY},
  {WSANO_DATA, "WSANO_DATA", NO_DATA},
  {0}
};

/* Cygwin internal */
static void
set_host_errno ()
{
  int i;

  int why = (*i_WSAGetLastError) ();
  for (i = 0; i < host_errmap[i].w != 0; ++i)
    if (why == host_errmap[i].w)
      break;

  if (host_errmap[i].w != 0)
    h_errno = host_errmap[i].e;
  else
    h_errno = NETDB_INTERNAL;
}

/* exported as getprotobyname: standards? */
extern "C"
struct protoent *
cygwin_getprotobyname (const char *p)
{
  MARK ();
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getprotobyname");

  struct protoent *res = (*i_getprotobyname) (p);
  if (!res)
    set_winsock_errno ();

  dump_protoent (res);
  out ("getprotobyname");
  return res;
}

/* exported as getprotobynumber: standards? */
extern "C"
struct protoent *
cygwin_getprotobynumber (int number)
{
  MARK ();
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getprotobynumber");

  struct protoent *res = (*i_getprotobynumber) (number);
  if (!res)
    set_winsock_errno ();

  dump_protoent (res);
  out ("getprotobynumber");
  return res;
}

void
fdsock (int fd, const char *name, SOCKET soc)
{
  fhandler_base *fh = dtable.build_fhandler(fd, FH_SOCKET, name);
  fh->set_handle ((HANDLE) soc);
  fh->set_flags (O_RDWR);
}

/* exported as socket: standards? */
extern "C"
int
cygwin_socket (int af, int type, int protocol)
{
  int res = -1;
  SetResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," socket");

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("socket");
  SOCKET soc;

  int fd = dtable.find_unused_handle ();

  if (fd < 0)
    {
      set_errno (ENMFILE);
      goto done;
    }

  debug_printf ("socket (%d, %d, %d)", af, type, protocol);

  soc = (*i_socket) (af, type, protocol);

  if (soc == INVALID_SOCKET)
    {
      set_winsock_errno ();
      goto done;
    }

  soc = duplicate_socket (soc);

  const char *name;
  if (af == AF_INET)
    name = (type == SOCK_STREAM ? "/dev/tcp" : "/dev/udp");
  else
    name = (type == SOCK_STREAM ? "/dev/streamsocket" : "/dev/dgsocket");

  fdsock (fd, name, soc);
  res = fd;

done:
  out ("socket");
  syscall_printf ("%d = socket (%d, %d, %d)", res, af, type, protocol);
  ReleaseResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," socket");
  return res;
}

/* exported as sendto: standards? */
extern "C"
int
cygwin_sendto (int fd,
		 const void *buf,
		 int len,
		 unsigned int flags,
		 const struct sockaddr *to,
		 int tolen)
{
  fhandler_socket *h = (fhandler_socket *) dtable[fd];

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("sendto");
  int res = (*i_sendto) (h->get_socket (), (const char *) buf, len,
						      flags, to, tolen);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
      res = -1;
    }
  out ("sendto");
  return res;
}

/* exported as recvfrom: standards? */
extern "C"
int
cygwin_recvfrom (int fd,
		   char *buf,
		   int len,
		   int flags,
		   struct sockaddr *from,
		   int *fromlen)
{
  fhandler_socket *h = (fhandler_socket *) dtable[fd];

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("recvfrom");

  debug_printf ("recvfrom %d", h->get_socket ());
  int res = (*i_recvfrom) (h->get_socket (), buf, len, flags, from, fromlen);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
      res = -1;
    }

  out ("recvfrom");
  return res;
}

/* Cygwin internal */
fhandler_socket *
get (int fd)
{
  if (dtable.not_open (fd))
    {
      set_errno (EINVAL);
      return 0;
    }

  return dtable[fd]->is_socket ();
}

/* exported as setsockopt: standards? */
extern "C"
int
cygwin_setsockopt (int fd,
		     int level,
		     int optname,
		     const void *optval,
		     int optlen)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("setsockopt");
  fhandler_socket *h = get (fd);
  int res = -1;
  const char *name = "error";

  if (h)
    {
      /* For the following debug_printf */
      switch (optname)
	{
	case SO_DEBUG:
	  name="SO_DEBUG";
	  break;
	case SO_ACCEPTCONN:
	  name="SO_ACCEPTCONN";
	  break;
	case SO_REUSEADDR:
	  name="SO_REUSEADDR";
	  break;
	case SO_KEEPALIVE:
	  name="SO_KEEPALIVE";
	  break;
	case SO_DONTROUTE:
	  name="SO_DONTROUTE";
	  break;
	case SO_BROADCAST:
	  name="SO_BROADCAST";
	  break;
	case SO_USELOOPBACK:
	  name="SO_USELOOPBACK";
	  break;
	case SO_LINGER:
	  name="SO_LINGER";
	  break;
	case SO_OOBINLINE:
	  name="SO_OOBINLINE";
	  break;
	}

      res = (*i_setsockopt) (h->get_socket (), level, optname,
				     (const char *) optval, optlen);

      if (optlen == 4)
	syscall_printf ("setsockopt optval=%x", *(long *) optval);

      if (res)
	set_winsock_errno ();
    }

  out ("setsockopt");
  syscall_printf ("%d = setsockopt (%d, %d, %x (%s), %x, %d)",
		  res, fd, level, optname, name, optval, optlen);
  return res;
}

/* exported as getsockopt: standards? */
extern "C"
int
cygwin_getsockopt (int fd,
		     int level,
		     int optname,
		     void *optval,
		     int *optlen)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getsockopt");
  fhandler_socket *h = get (fd);
  int res = -1;
  const char *name = "error";
  if (h)
    {
      /* For the following debug_printf */
      switch (optname)
	{
	case SO_DEBUG:
	  name="SO_DEBUG";
	  break;
	case SO_ACCEPTCONN:
	  name="SO_ACCEPTCONN";
	  break;
	case SO_REUSEADDR:
	  name="SO_REUSEADDR";
	  break;
	case SO_KEEPALIVE:
	  name="SO_KEEPALIVE";
	  break;
	case SO_DONTROUTE:
	  name="SO_DONTROUTE";
	  break;
	case SO_BROADCAST:
	  name="SO_BROADCAST";
	  break;
	case SO_USELOOPBACK:
	  name="SO_USELOOPBACK";
	  break;
	case SO_LINGER:
	  name="SO_LINGER";
	  break;
	case SO_OOBINLINE:
	  name="SO_OOBINLINE";
	  break;
	}

      res = (*i_getsockopt) (h->get_socket (), level, optname,
				       (char *) optval, (int *) optlen);

      if (res)
	set_winsock_errno ();
    }

  out ("getsockopt");
  syscall_printf ("%d = getsockopt (%d, %d, %x (%s), %x, %d)",
		  res, fd, level, optname, name, optval, optlen);
  return res;
}

/* exported as connect: standards? */
extern "C"
int
cygwin_connect (int fd,
		  const struct sockaddr *name,
		  int namelen)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("connect");
  int res;
  fhandler_socket *sock = get (fd);
  if (!sock)
    {
      res = -1;
    }
  else
    {
      res = (*i_connect) (sock->get_socket (), name, namelen);
      if (res)
	set_winsock_errno ();
      out ("connect");
    }
  return res;
}

/* exported as getservbyname: standards? */
extern "C"
struct servent *
cygwin_getservbyname (const char *name, const char *proto)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getservbyname");
  struct servent *p = (*i_getservbyname) (name, proto);
  if (!p)
    set_winsock_errno ();

  syscall_printf ("%x = getservbyname (%s, %s)", p, name, proto);
  out ("getservbyname");
  return p;
}

/* exported as getservbyport: standards? */
extern "C"
struct servent *
cygwin_getservbyport (int port, const char *proto)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getservbyport");
  struct servent *p = (*i_getservbyport) (port, proto);
  if (!p)
    set_winsock_errno ();

  syscall_printf ("%x = getservbyport (%d, %s)", p, port, proto);
  out ("getservbyport");
  return p;
}

extern "C"
int
gethostname (char *name, size_t len)
{
  MARK ();

  /* Call GetComputerName() if wsock dll doesn't exist. */
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    if ((wsock32 = LoadLibrary ("wsock32.dll")) != NULL)
      winsock_init ();

  in ("gethostname");
  if (i_gethostname == NULL
      || (*i_gethostname) (name, len) == SOCKET_ERROR)
    {
      DWORD local_len = len;

      if (!GetComputerNameA (name, &local_len))
	{
	  set_winsock_errno ();
	  return -1;
	}
    }
  debug_printf ("name %s\n", name);
  h_errno = 0;
  out ("gethostname");
  return 0;
}

/* exported as gethostbyname: standards? */
extern "C"
struct hostent *
cygwin_gethostbyname (const char *name)
{
  MARK ();
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("gethostbyname");
  struct hostent *ptr = (*i_gethostbyname) (name);
  if (!ptr)
    {
      set_winsock_errno ();
      set_host_errno ();
    }
  else
    {
      debug_printf ("h_name %s", ptr->h_name);
      h_errno = 0;
    }
  out ("gethostbyname");
  return ptr;
}

/* exported as gethostbyaddr: standards? */
extern "C"
struct hostent *
cygwin_gethostbyaddr (const char *addr, int len, int type)
{
  MARK ();
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("gethostbyaddr");
  struct hostent *ptr = (*i_gethostbyaddr) (addr, len, type);
  if (!ptr)
    {
      set_winsock_errno ();
      set_host_errno ();
    }
  else
    {
      debug_printf ("h_name %s", ptr->h_name);
      h_errno = 0;
    }
  out ("gethostbyaddr");
  return ptr;
}

/* exported as accept: standards? */
extern "C"
int
cygwin_accept (int fd, struct sockaddr *peer, int *len)
{
  int res = -1;

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("accept");
  fhandler_socket *sock = get (fd);
  if (sock)
    {
      res = (*i_accept) (sock->get_socket (), peer, len);  // can't use a blocking call inside a lock

      SetResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," accept");

      int res_fd = dtable.find_unused_handle ();
      if (res_fd == -1)
	{
	  /* FIXME: what is correct errno? */
	  set_errno (EMFILE);
	  goto done;
	}
      if ((SOCKET) res == (SOCKET) INVALID_SOCKET)
	set_winsock_errno ();
      else
	{
	  res = duplicate_socket (res);

	  fdsock (res_fd, sock->get_name (), res);
	  res = res_fd;
	}
    }
done:
  out ("accept");
  syscall_printf ("%d = accept (%d, %x, %x)", res, fd, peer, len);
  ReleaseResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," accept");
  return res;
}

/* exported as bind: standards? */
extern "C"
int
cygwin_bind (int fd, struct sockaddr *my_addr, int addrlen)
{
  int res = -1;
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("bind");
  fhandler_socket *sock = get (fd);
  if (sock)
    {
      res = (*i_bind) (sock->get_socket (), my_addr, addrlen);
      if (res)
	set_winsock_errno ();
    }
  out ("bind");
  syscall_printf ("%d = bind (%d, %x, %d)", res, fd, my_addr, addrlen);
  return res;
}

/* exported as getsockname: standards? */
extern "C"
int
cygwin_getsockname (int fd, struct sockaddr *addr, int *namelen)
{
  int res = -1;
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getsockname");
  fhandler_socket *sock = get (fd);
  if (sock)
    {
      res = (*i_getsockname) (sock->get_socket (), addr, namelen);
      if (res)
	set_winsock_errno ();

    }
  out ("getsockname");
  syscall_printf ("%d = getsockname (%d, %x, %d)", res, fd, addr, namelen);
  return res;
}

/* exported as listen: standards? */
extern "C"
int
cygwin_listen (int fd, int backlog)
{
  int res = -1;
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("listen");

  fhandler_socket *sock = get (fd);
  if (sock)
    {
      res = (*i_listen) (sock->get_socket (), backlog);
      if (res)
	set_winsock_errno ();
    }
  out ("listen");
  syscall_printf ("%d = listen (%d, %d)", res, fd, backlog);
  return res;
}

/* exported as shutdown: standards? */
extern "C"
int
cygwin_shutdown (int fd, int how)
{
  int res = -1;
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("shutdown");

  fhandler_socket *sock = get (fd);
  if (sock)
    {
      res = (*i_shutdown) (sock->get_socket (), how);
      if (res)
	set_winsock_errno ();
    }
  out ("shutdown");
  syscall_printf ("%d = shutdown (%d, %d)", res, fd, how);
  return res;
}

/* exported as herror: standards? */
extern "C"
void
cygwin_herror (const char *p)
{
  debug_printf ("********%d*************", __LINE__);
}

/* exported as send: standards? */
extern "C"
int
cygwin_send (int fd, const void *buf, int len, unsigned int flags)
{
  fhandler_socket *h = (fhandler_socket *) dtable[fd];
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("send");

  int res = (*i_send) (h->get_socket (), (const char *) buf, len, flags);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
      res = -1;
    }

#if 0
  for (int i = 0; i < len; i++)
    small_printf ("send %d %x %c", i, ((char *) buf)[i], ((char *) buf)[i]);
#endif

  syscall_printf ("%d = send (%d, %x, %d, %x)", res, fd, buf, len, flags);

  out ("send");
  return res;
}

/* exported as recv: standards? */
extern "C"
int
cygwin_recv (int fd, void *buf, int len, unsigned int flags)
{
  fhandler_socket *h = (fhandler_socket *) dtable[fd];
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("recv");

  int res = (*i_recv) (h->get_socket (), (char *) buf, len, flags);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
      res = -1;
    }

#if 0
  if (res > 0 && res < 200)
    for (int i=0; i < res; i++)
      system_printf ("%d %x %c", i, ((char *) buf)[i], ((char *) buf)[i]);
#endif

  syscall_printf ("%d = recv (%d, %x, %x, %x)", res, fd, buf, len, flags);

  out ("recv");
  return res;
}

/* exported as getpeername: standards? */
extern "C"
int
cygwin_getpeername (int fd, struct sockaddr *name, int *len)
{
  fhandler_socket *h = (fhandler_socket *) dtable[fd];
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("getpeername");

  debug_printf ("getpeername %d", h->get_socket ());
  int res = (*i_getpeername) (h->get_socket (), name, len);
  if (res)
    set_winsock_errno ();

  out ("getpeername");
  debug_printf ("%d = getpeername %d", res, h->get_socket ());
  return res;
}

/* getdomainname: standards? */
extern "C"
int
getdomainname (char *domain, int len)
{
  /*
   * This works for Win95 only if the machine is configured to use MS-TCP.
   * If a third-party TCP is being used this will fail.
   * FIXME: On Win95, is there a way to portably check the TCP stack
   * in use and include paths for the Domain name in each ?
   * Punt for now and assume MS-TCP on Win95.
   */
  reg_key r (HKEY_LOCAL_MACHINE, KEY_READ,
	     (os_being_run != winNT) ? "System" : "SYSTEM",
	     "CurrentControlSet", "Services",
	     (os_being_run != winNT) ? "MSTCP" : "Tcpip",
	     NULL);

  /* FIXME: Are registry keys case sensitive? */
  if (r.error () || r.get_string ("Domain", domain, len, "") != ERROR_SUCCESS)
    {
      __seterrno ();
      return -1;
    }

  return 0;
}

/* Cygwin internal */
/* Fill out an ifconf struct.
 *
 * Windows NT:
 * Look at the Bind value in
 * HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Linkage\
 * This is a REG_MULTI_SZ with strings of the form:
 * \Device\<Netcard>, where netcard is the name of the net device.
 * Then look under:
 * HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\<NetCard>\
 *                                                  Parameters\Tcpip
 * at the IPAddress, Subnetmask and DefaultGateway values for the
 * required values.
 *
 * Windows 9x:
 * We originally just did a gethostbyname, assuming that it's pretty
 * unlikely Win9x will ever have more than one netcard.  When this
 * succeeded, we got the interface plus a loopback.
 * Currently, we read all
 * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Class\NetTrans\*"
 * entries from the Registry and use all entries that have legal
 * "IPAddress" and "IPMask" values.
 */
static int
get_ifconf (struct ifconf *ifc, int what)
{
  if (os_being_run == winNT)
    {
      HKEY key;
      DWORD type, size;
      unsigned long lip, lnp;
      int cnt = 1;
      char *binding = (char *) 0;
      struct sockaddr_in *sa;

      /* Union maps buffer to correct struct */
      struct ifreq *ifr = ifc->ifc_req;
      
      /* Ensure we have space for two struct ifreqs, fail if not. */
      if (ifc->ifc_len < (int) (2 * sizeof (struct ifreq)))
	{
	  set_errno (EFAULT);
	  return -1;
	}

      /* Set up interface lo0 first */
      strcpy (ifr->ifr_name, "lo0");
      memset (&ifr->ifr_addr, '\0', sizeof (ifr->ifr_addr));
      switch (what)
	{
	case SIOCGIFCONF:
	case SIOCGIFADDR:
	  sa = (struct sockaddr_in *) &ifr->ifr_addr;
	  sa->sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	  break;
	case SIOCGIFBRDADDR:
	  lip = htonl (INADDR_LOOPBACK);
	  lnp = cygwin_inet_addr ("255.0.0.0");
	  sa = (struct sockaddr_in *) &ifr->ifr_broadaddr;
	  sa->sin_addr.s_addr = lip & lnp | ~lnp;
	  break;
	case SIOCGIFNETMASK:
	  sa = (struct sockaddr_in *) &ifr->ifr_netmask;
	  sa->sin_addr.s_addr = cygwin_inet_addr ("255.0.0.0");
	  break;
	default:
	  set_errno (EINVAL);
	  return -1;
	}
      sa->sin_family = AF_INET;
      sa->sin_port = 0;
      
      if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,
			"SYSTEM\\"
			"CurrentControlSet\\"
			"Services\\"
			"Tcpip\\"
			"Linkage",
			0, KEY_READ, &key) == ERROR_SUCCESS)
	{
	  if (RegQueryValueEx (key, "Bind",
			       NULL, &type,
			       NULL, &size) == ERROR_SUCCESS)
	    {
	      binding = new char [ size ];
	      if (RegQueryValueEx (key, "Bind",
				   NULL, &type,
				   (unsigned char *) binding,
				   &size) != ERROR_SUCCESS)
		{
		  delete [] binding;
		  binding = (char *) 0;
		}
	    }
	  RegCloseKey (key);
	}
      
      if (binding)
	{
	  char *bp, eth[2];
	  char cardkey[256], ipaddress[256], netmask[256];
	  
	  eth[0] = '/';
	  eth[1] = '\0';
	  for (bp = binding; *bp; bp += strlen(bp) + 1)
	    {
	      bp += strlen ("\\Device\\");
	      strcpy (cardkey, "SYSTEM\\CurrentControlSet\\Services\\");
	      strcat (cardkey, bp);
	      strcat (cardkey, "\\Parameters\\Tcpip");

	      if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, cardkey,
				0, KEY_READ, &key) != ERROR_SUCCESS)
		continue;

	      if (RegQueryValueEx (key, "IPAddress",
				   NULL, &type,
				   (unsigned char *) &ipaddress,
				   (size = 256, &size)) == ERROR_SUCCESS
		  && RegQueryValueEx (key, "SubnetMask",
				      NULL, &type,
				      (unsigned char *) &netmask,
				      (size = 256, &size)) == ERROR_SUCCESS)
		{
		  char *ip, *np;
		  char sub[2];
		  char dhcpaddress[256], dhcpnetmask[256];
		  
		  sub[0] = '/';
		  sub[1] = '\0';
		  if (strncmp (bp, "NdisWan", 7))
		    ++*eth;
		  for (ip = ipaddress, np = netmask;
		       *ip && *np;
		       ip += strlen (ip) + 1, np += strlen (np) + 1)
		    {
		      if ((caddr_t) ++ifr > ifc->ifc_buf
			  + ifc->ifc_len
			  - sizeof (struct ifreq))
			break;
		      
		      if (! strncmp (bp, "NdisWan", 7))
			{
			  strcpy (ifr->ifr_name, "ppp");
			  strcat (ifr->ifr_name, bp + 7);
			}
		      else
			{
			  strcpy (ifr->ifr_name, "eth");
			  strcat (ifr->ifr_name, eth);
			}
		      ++*sub;
		      if (*sub >= '1')
			strcat (ifr->ifr_name, sub);
		      memset (&ifr->ifr_addr, '\0', sizeof ifr->ifr_addr);
		      if (cygwin_inet_addr (ip) == 0L
			  && RegQueryValueEx (key, "DhcpIPAddress",
					      NULL, &type,
					      (unsigned char *) &dhcpaddress,
					      (size = 256, &size))
			  == ERROR_SUCCESS
			  && RegQueryValueEx (key, "DhcpSubnetMask",
					      NULL, &type,
					      (unsigned char *) &dhcpnetmask,
					      (size = 256, &size))
			  == ERROR_SUCCESS)
			{
			  switch (what)
			    {
			    case SIOCGIFCONF:
			    case SIOCGIFADDR:
			      sa = (struct sockaddr_in *) &ifr->ifr_addr;
			      sa->sin_addr.s_addr =
				cygwin_inet_addr (dhcpaddress);
			      break;
			    case SIOCGIFBRDADDR:
			      lip = cygwin_inet_addr (dhcpaddress);
			      lnp = cygwin_inet_addr (dhcpnetmask);
			      sa = (struct sockaddr_in *) &ifr->ifr_broadaddr;
			      sa->sin_addr.s_addr = lip & lnp | ~lnp;
			      break;
			    case SIOCGIFNETMASK:
			      sa = (struct sockaddr_in *) &ifr->ifr_netmask;
			      sa->sin_addr.s_addr =
				cygwin_inet_addr (dhcpnetmask);
			      break;
			    }
			}
		      else
			{
			  switch (what)
			    {
			    case SIOCGIFCONF:
			    case SIOCGIFADDR:
			      sa = (struct sockaddr_in *) &ifr->ifr_addr;
			      sa->sin_addr.s_addr = cygwin_inet_addr (ip);
			      break;
			    case SIOCGIFBRDADDR:
			      lip = cygwin_inet_addr (ip);
			      lnp = cygwin_inet_addr (np);
			      sa = (struct sockaddr_in *) &ifr->ifr_broadaddr;
			      sa->sin_addr.s_addr = lip & lnp | ~lnp;
			      break;
			    case SIOCGIFNETMASK:
			      sa = (struct sockaddr_in *) &ifr->ifr_netmask;
			      sa->sin_addr.s_addr = cygwin_inet_addr (np);
			      break;
			    }
			}
		      sa->sin_family = AF_INET;
		      sa->sin_port = 0;
		      ++cnt;
		    }
		}
	      RegCloseKey (key);
	    }
	  delete [] binding;
	}
      
      /* Set the correct length */
      ifc->ifc_len = cnt * sizeof (struct ifreq);
    }
  else /* Windows 9x */
    {
      HKEY key, subkey;
      FILETIME update;
      LONG res;
      DWORD type, size;
      unsigned long lip, lnp;
      char ifname[256], ip[256], np[256];
      int cnt = 1;
      struct sockaddr_in *sa;

      /* Union maps buffer to correct struct */
      struct ifreq *ifr = ifc->ifc_req;
      char eth[2];
      
      eth[0] = '/';
      eth[1] = '\0';
      
      /* Ensure we have space for two struct ifreqs, fail if not. */
      if (ifc->ifc_len < (int) (2 * sizeof (struct ifreq)))
	{
	  set_errno (EFAULT);
	  return -1;
	}
      
      /* Set up interface lo0 first */
      strcpy (ifr->ifr_name, "lo0");
      memset (&ifr->ifr_addr, '\0', sizeof ifr->ifr_addr);
      switch (what)
	{
	case SIOCGIFCONF:
	case SIOCGIFADDR:
	  sa = (struct sockaddr_in *) &ifr->ifr_addr;
	  sa->sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	  break;
	case SIOCGIFBRDADDR:
	  lip = htonl(INADDR_LOOPBACK);
	  lnp = cygwin_inet_addr ("255.0.0.0");
	  sa = (struct sockaddr_in *) &ifr->ifr_broadaddr;
	  sa->sin_addr.s_addr = lip & lnp | ~lnp;
	  break;
	case SIOCGIFNETMASK:
	  sa = (struct sockaddr_in *) &ifr->ifr_netmask;
	  sa->sin_addr.s_addr = cygwin_inet_addr ("255.0.0.0");
	  break;
	default:
	  set_errno (EINVAL);
	  return -1;
	}
      sa->sin_family = AF_INET;
      sa->sin_port = 0;
      
      if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,
			"SYSTEM\\"
			"CurrentControlSet\\"
			"Services\\"
			"Class\\"
			"NetTrans",
			0, KEY_READ, &key) == ERROR_SUCCESS)
	{
	  for (int i = 0;
	       (res = RegEnumKeyEx (key, i, ifname,
				    (size = sizeof ifname, &size),
				    0, 0, 0, &update)) != ERROR_NO_MORE_ITEMS;
	       ++i)
	    {
	      if (res != ERROR_SUCCESS
		  || RegOpenKeyEx (key, ifname, 0,
				   KEY_READ, &subkey) != ERROR_SUCCESS)
		continue;
	      if (RegQueryValueEx (subkey, "IPAddress", 0,
				   &type, (unsigned char *) ip,
				   (size = sizeof ip, &size)) == ERROR_SUCCESS
		  || RegQueryValueEx (subkey, "IPMask", 0,
				      &type, (unsigned char *) np,
				      (size = sizeof np, &size)) == ERROR_SUCCESS)
		{
		  if ((caddr_t)++ifr > ifc->ifc_buf
		      + ifc->ifc_len
		      - sizeof(struct ifreq))
		    break;
		  ++*eth;
		  strcpy (ifr->ifr_name, "eth");
		  strcat (ifr->ifr_name, eth);
		  switch (what)
		    {
		    case SIOCGIFCONF:
		    case SIOCGIFADDR:
		      sa = (struct sockaddr_in *) &ifr->ifr_addr;
		      sa->sin_addr.s_addr = cygwin_inet_addr (ip);
		      break;
		    case SIOCGIFBRDADDR:
		      lip = cygwin_inet_addr (ip);
		      lnp = cygwin_inet_addr (np);
		      sa = (struct sockaddr_in *) &ifr->ifr_broadaddr;
		      sa->sin_addr.s_addr = lip & lnp | ~lnp;
		      break;
		    case SIOCGIFNETMASK:
		      sa = (struct sockaddr_in *) &ifr->ifr_netmask;
		      sa->sin_addr.s_addr = cygwin_inet_addr (np);
		      break;
		    }
		  sa->sin_family = AF_INET;
		  sa->sin_port = 0;
		  ++cnt;
		}
	      RegCloseKey (subkey);
	    }
	}
      
      /* Set the correct length */
      ifc->ifc_len = cnt * sizeof (struct ifreq);
    }
  
  return 0;
}

/* Cygwin internal */
/*
 * Return the flags settings for an interface.
 */
static int
get_if_flags (struct ifreq *ifr)
{
  short flags = IFF_NOTRAILERS;
  struct sockaddr_in *sa = (struct sockaddr_in *) &ifr->ifr_addr;

  if (sa->sin_addr.s_addr == INADDR_ANY)
      flags |= IFF_BROADCAST;
  else if (sa->sin_addr.s_addr == INADDR_LOOPBACK)
      flags |= IFF_LOOPBACK | IFF_UP | IFF_RUNNING;
  else
      flags |= IFF_BROADCAST | IFF_UP | IFF_RUNNING;

  ifr->ifr_flags = flags;
  return 0;
}

/* exported as rcmd: standards? */
extern "C"
int
cygwin_rcmd (char **ahost, unsigned short inport, char *locuser,
	       char *remuser, char *cmd, int *fd2p)
{
  int res = -1;
  SOCKET fd2s;

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  if (!i_rcmd)
    LOAD (rcmd)

  int res_fd = dtable.find_unused_handle ();
  if (res_fd == -1)
    goto done;

  if (fd2p)
    {
      *fd2p = dtable.find_unused_handle (res_fd + 1);
      if (*fd2p == -1)
	goto done;
    }

  res = (*i_rcmd) (ahost, inport, locuser, remuser, cmd, fd2p? &fd2s: NULL);
  if (res == (int) INVALID_SOCKET)
    goto done;
  else
    {
      res = duplicate_socket (res);

      fdsock (res_fd, "/dev/tcp", res);
      res = res_fd;
    }
  if (fd2p)
    {
      fd2s = duplicate_socket (fd2s);

      fdsock (*fd2p, "/dev/tcp", fd2s);
    }
done:
  syscall_printf ("%d = rcmd (...)", res);
  return res;
}

/* exported as rresvport: standards? */
extern "C"
int
cygwin_rresvport (int *port)
{
  int res = -1;

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  if (!i_rresvport)
    LOAD (rresvport)

  int res_fd = dtable.find_unused_handle ();
  if (res_fd == -1)
    goto done;
  res = (*i_rresvport) (port);

  if (res == (int) INVALID_SOCKET)
    goto done;
  else
    {
      res = duplicate_socket (res);

      fdsock (res_fd, "/dev/tcp", res);
      res = res_fd;
    }
done:
  syscall_printf ("%d = rresvport (%d)", res, port ? *port : 0);
  return res;
}

/* exported as rexec: standards? */
extern "C"
int
cygwin_rexec (char **ahost, unsigned short inport, char *locuser,
	       char *password, char *cmd, int *fd2p)
{
  int res = -1;
  SOCKET fd2s;

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  if (!i_rexec)
    LOAD (rexec)

  int res_fd = dtable.find_unused_handle ();
  if (res_fd == -1)
    goto done;
  if (fd2p)
    {
      *fd2p = dtable.find_unused_handle (res_fd + 1);
      if (*fd2p == -1)
	goto done;
    }
  res = (*i_rexec) (ahost, inport, locuser, password,
					    cmd, fd2p ? &fd2s : NULL);
  if (res == (int) INVALID_SOCKET)
    goto done;
  else
    {
      res = duplicate_socket (res);

      fdsock (res_fd, "/dev/tcp", res);
      res = res_fd;
    }
  if (fd2p)
    {
      fd2s = duplicate_socket (fd2s);

      fdsock (*fd2p, "/dev/tcp", fd2s);
#if 0 /* ??? */
      fhandler_socket *h;
      p->hmap.vec[*fd2p].h = h =
	  new (&p->hmap.vec[*fd2p].item) fhandler_socket (fd2s, "/dev/tcp");
#endif
    }
done:
  syscall_printf ("%d = rexec (...)", res);
  return res;
}

/* socketpair: standards? */
/* Win32 supports AF_INET only, so ignore domain and protocol arguments */
extern "C"
int
socketpair (int, int type, int, int *sb)
{
  int res = -1;
  SOCKET insock, outsock, newsock;
  struct sockaddr_in sock_in;
  int len = sizeof (sock_in);

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  SetResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," socketpair");

  sb[0] = dtable.find_unused_handle ();
  if (sb[0] == -1)
    {
      set_errno (EMFILE);
      goto done;
    }
  sb[1] = dtable.find_unused_handle (sb[0] + 1);
  if (sb[1] == -1)
    {
      set_errno (EMFILE);
      goto done;
    }

  /* create a listening socket */
  newsock = (*i_socket) (AF_INET, type, 0);
  if (newsock == INVALID_SOCKET)
    {
      set_winsock_errno ();
      goto done;
    }

  /* bind the socket to any unused port */
  sock_in.sin_family = AF_INET;
  sock_in.sin_port = 0;
  sock_in.sin_addr.s_addr = INADDR_ANY;

  if ((*i_bind) (newsock, (struct sockaddr *) &sock_in, sizeof (sock_in)) < 0)
    {
      set_winsock_errno ();
      (*i_closesocket) (newsock);
      goto done;
    }

  if ((*i_getsockname) (newsock, (struct sockaddr *) &sock_in, &len) < 0)
    {
      debug_printf ("getsockname error");
      set_winsock_errno ();
      (*i_closesocket) (newsock);
      goto done;
    }

  (*i_listen) (newsock, 2);

  /* create a connecting socket */
  outsock = (*i_socket) (AF_INET, type, 0);
  if (outsock == INVALID_SOCKET)
    {
      debug_printf ("can't create outsock");
      set_winsock_errno ();
      (*i_closesocket) (newsock);
      goto done;
    }

  sock_in.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  /* Do a connect and accept the connection */
  if ((*i_connect) (outsock, (struct sockaddr *) &sock_in,
					   sizeof (sock_in)) < 0)
    {
      debug_printf ("connect error");
      set_winsock_errno ();
      (*i_closesocket) (newsock);
      (*i_closesocket) (outsock);
      goto done;
    }

  insock = (*i_accept) (newsock, (struct sockaddr *) &sock_in, &len);
  if (insock == INVALID_SOCKET)
    {
      debug_printf ("accept error");
      set_winsock_errno ();
      (*i_closesocket) (newsock);
      (*i_closesocket) (outsock);
      goto done;
    }

  (*i_closesocket) (newsock);
  res = 0;

  insock = duplicate_socket (insock);

  fdsock (sb[0], "/dev/tcp", insock);

  outsock = duplicate_socket (outsock);
  fdsock (sb[1], "/dev/tcp", outsock);

done:
  syscall_printf ("%d = socketpair (...)", res);
  ReleaseResourceLock(LOCK_FD_LIST,WRITE_LOCK|READ_LOCK," socketpair");
  return res;
}

/**********************************************************************/
/* fhandler_socket */

fhandler_socket::fhandler_socket (const char *name) :
	fhandler_base (FH_SOCKET, name)
{
  set_cb (sizeof *this);
  number_of_sockets++;
}

/* sethostent: standards? */
extern "C"
void
sethostent (int)
{
}

/* endhostent: standards? */
extern "C"
void
endhostent (void)
{
}

fhandler_socket::~fhandler_socket ()
{
  if (--number_of_sockets < 0)
    {
      number_of_sockets = 0;
      system_printf("socket count < 0");
    }
}

int
fhandler_socket::write (const void *ptr, size_t len)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  int res = (*i_send) (get_socket (), (const char *) ptr, len, 0);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
      if (get_errno () == ECONNABORTED || get_errno () == ECONNRESET)
	_raise (SIGPIPE);
    }
  return res;
}

int
fhandler_socket::read (void *ptr, size_t len)
{
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  in ("read");
  int res = (*i_recv) (get_socket (), (char *) ptr, len, 0);
  if (res == SOCKET_ERROR)
    {
      set_winsock_errno ();
    }
  out ("read");
  return res;
}

/* Cygwin internal */
int
fhandler_socket::close ()
{
  int res = 0;
  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  if ((*i_closesocket) (get_socket ()))
    {
      set_winsock_errno ();
      res = -1;
    }

  return res;
}

#define ASYNC_MASK (FD_READ|FD_WRITE|FD_OOB|FD_ACCEPT|FD_CONNECT)

/* Cygwin internal */
int
fhandler_socket::ioctl (unsigned int cmd, void *p)
{
  int res;

  if (NOTSTATE (myself, PID_SOCKETS_USED))
    winsock_init ();

  switch (cmd)
    {
    case SIOCGIFCONF:
      {
	struct ifconf *ifc = (struct ifconf *) p;
	if (ifc == 0)
	  {
	    set_errno (EINVAL);
	    return -1;
	  }
	res = get_ifconf (ifc, cmd);
        if (res)
	  debug_printf ("error in get_ifconf\n");
	break;
      }
    case SIOCGIFFLAGS:
      {
	struct ifreq *ifr = (struct ifreq *) p;
	if (ifr == 0)
	  {
	    set_errno (EINVAL);
	    return -1;
	  }
	res = get_if_flags (ifr);
	break;
      }
    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
    case SIOCGIFADDR:
      {
        char buf[2048];
        struct ifconf ifc;
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        struct ifreq *ifrp;

        struct ifreq *ifr = (struct ifreq *) p;
        if (ifr == 0)
          {
            debug_printf("ifr == NULL\n");
            set_errno (EINVAL);
            return -1;
          }

	res = get_ifconf (&ifc, cmd);
        if (res)
	  {
	    debug_printf ("error in get_ifconf\n");
	    break;
	  }

        debug_printf("    name: %s\n", ifr->ifr_name);
        for (ifrp = ifc.ifc_req;
             (caddr_t) ifrp < ifc.ifc_buf + ifc.ifc_len;
             ++ifrp)
          {
            debug_printf("testname: %s\n", ifrp->ifr_name);
            if (! strcmp (ifrp->ifr_name, ifr->ifr_name))
              {
                switch (cmd)
		  {
		  case SIOCGIFADDR:
		    ifr->ifr_addr = ifrp->ifr_addr;
		    break;
		  case SIOCGIFBRDADDR:
		    ifr->ifr_broadaddr = ifrp->ifr_broadaddr;
		    break;
		  case SIOCGIFNETMASK:
		    ifr->ifr_netmask = ifrp->ifr_netmask;
		    break;
		  }
                break;
              }
          }
        if ((caddr_t) ifrp >= ifc.ifc_buf + ifc.ifc_len)
          {
            set_errno (EINVAL);
            return -1;
          }
        break;
      }
    case FIOASYNC:
      {
	res = (*i_WSAAsyncSelect) (get_socket (), gethwnd (), WM_ASYNCIO,
		*(int *) p ? ASYNC_MASK : 0);
	syscall_printf ("Async I/O on socket %s",
		*(int *) p ? "started" : "cancelled");
	set_async (*(int *) p);
	break;
      }
    default:
      {
      /* We must cancel WSAAsyncSelect (if any) before settting socket to
       * blocking mode
       */
	if (cmd == FIONBIO && *(int *) p == 0)
	  (*i_WSAAsyncSelect) (get_socket (), gethwnd (), 0, 0);
	res = (*i_ioctlsocket) (get_socket (), cmd, (unsigned long *) p);
	if (res == SOCKET_ERROR)
	    set_winsock_errno ();
	if (cmd == FIONBIO)
	  {
	    syscall_printf ("socket is now %sblocking",
			      *(int *) p ? "un" : "");
	    /* Start AsyncSelect if async socket unblocked */
	    if (*(int *) p && get_async ())
	      (*i_WSAAsyncSelect) (get_socket (), gethwnd (), WM_ASYNCIO,
		ASYNC_MASK);
	  }
	break;
      }
    }
  syscall_printf ("%d = ioctl_socket (%x, %x)", res, cmd, p);
  return res;
}
