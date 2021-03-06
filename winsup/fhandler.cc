/* fhandler.cc.  See console.cc for fhandler_console functions.

   Copyright 1996, 1997, 1998, 1999 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <sys/termios.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "winsup.h"

static NO_COPY const int CHUNK_SIZE = 1024; /* Used for crlf conversions */

uid_t
get_file_owner (const char *filename)
{
  if (os_being_run == winNT)
    {
      char psd_buffer[1024];
      PSECURITY_DESCRIPTOR psd = (PSECURITY_DESCRIPTOR) psd_buffer;
      DWORD requested_length;
      PSID psid;
      BOOL bOwnerDefaulted = TRUE;

      if (!GetFileSecurity (filename, OWNER_SECURITY_INFORMATION,
			   psd, 1024, &requested_length))
	return getuid();

      if (!GetSecurityDescriptorOwner (psd, &psid, &bOwnerDefaulted))
	return getuid ();

      return psid ? get_id_from_sid (psid) : getuid ();
    }

  return getuid();
}

gid_t
get_file_group (const char *filename)
{
  if (os_being_run == winNT)
    {
      char psd_buffer[1024];
      PSECURITY_DESCRIPTOR psd = (PSECURITY_DESCRIPTOR) psd_buffer;
      DWORD requested_length;
      PSID psid;
      BOOL bGroupDefaulted = TRUE;

      /* obtain the file's group information */
      if (!GetFileSecurity (filename, GROUP_SECURITY_INFORMATION, psd,
			    1024, &requested_length))
	return getgid();

      /* extract the group sid from the security descriptor */
      if(!GetSecurityDescriptorGroup (psd, &psid, &bGroupDefaulted))
	return getgid ();

      return psid ? get_id_from_sid (psid) : getuid ();
    }

  return getgid ();
}

/**********************************************************************/
/* fhandler_base */

/* Record the file name.
   Filenames are used mostly for debugging messages, and it's hoped that
   in cases where the name is really required, the filename wouldn't ever
   be too long (e.g. devices or some such).
*/

void
fhandler_base::set_name (const char *unix, const char *win32, int unit)
{
  if (!no_free_names ())
    {
      if (unix_path_name_ != NULL)
	{
	  free (unix_path_name_);
	}
      if (win32_path_name_ != NULL)
	{
	  free (win32_path_name_);
	}
    }

  if (unix == NULL || !*unix)
    {
      win32_path_name_ = unix_path_name_ = NULL;
      return;
    }

  unix_path_name_ = strdup (unix);
  if (unix_path_name_ == NULL)
    {
      system_printf ("fatal error. strdup failed");
      exit (ENOMEM);
    }

  if (win32)
    win32_path_name_ = strdup (win32);
  else
    {
      const char *fmt = get_native_name ();
      win32_path_name_ = (char *) malloc (strlen(fmt) + 8);
      __small_sprintf (win32_path_name_, fmt, unit);
    }

  if (win32_path_name_ == NULL)
    {
      system_printf ("fatal error. strdup failed");
      exit (ENOMEM);
    }
}

/* Normal file i/o handlers.  */

/* Cover function to ReadFile to achieve (as much as possible) Posix style
   semantics and use of errno.  */
int
fhandler_base::raw_read (void *ptr, size_t ulen)
{
  DWORD bytes_read;

  if (!ReadFile (get_handle(), ptr, ulen, &bytes_read, 0))
    {
      int errcode;

      /* Some errors are not really errors.  Detect such cases here.  */

      errcode = GetLastError ();
      switch (errcode)
	{
	case ERROR_BROKEN_PIPE:
	  /* This is really EOF.  */
	  bytes_read = 0;
	  break;
	case ERROR_MORE_DATA:
	  /* `bytes_read' is supposedly valid.  */
	  break;
	default:
	  syscall_printf ("ReadFile %s failed, %E", unix_path_name_);
	  set_errno (EACCES);
	  return -1;
	  break;
	}
    }

  return bytes_read;
}

int
fhandler_base::linearize (unsigned char *buf)
{
  unsigned char *orig_buf = buf;
#define cbuf ((char *)buf)
  strcpy (cbuf, get_name() ?: "");
  char *p = strcpy (strchr (cbuf, '\0') + 1, get_win32_name ());
  buf = (unsigned char *)memcpy (strchr (p, '\0') + 1, this, cb);
  debug_printf ("access_ %p, status_ %p, input_handle %p, output_handle %p",
		access_, status_, get_input_handle (), get_output_handle ());
  return (buf + cb) - orig_buf;
#undef cbuf
}

int
fhandler_base::de_linearize (const unsigned char *buf)
{
  int thiscb = cb;
  char *unix = unix_path_name_;
  char *win32 = win32_path_name_;
  memcpy(this, buf, cb);
  unix_path_name_ = unix;
  win32_path_name_ = win32;
  debug_printf ("access_ %p, status_ %p, input_handle %p, output_handle %p",
		access_, status_, get_input_handle (), get_output_handle ());
  if (thiscb != cb)
    system_printf ("mismatch in linearize/delinearize %d != %d", thiscb, cb);
  return cb;
}

/* Cover function to WriteFile to provide Posix interface and semantics
   (as much as possible).  */
int
fhandler_base::raw_write (const void *ptr, size_t len)
{
  DWORD bytes_written;

  if (!WriteFile (get_handle(), ptr, len, &bytes_written, 0))
    {
      if (GetLastError () == ERROR_DISK_FULL)
 	return bytes_written;
      __seterrno ();
      if (get_errno () == EPIPE)
	raise (SIGPIPE);
      return -1;
    }
  return bytes_written;
}

/* Open system call handler function.
   Path is now already checked for symlinks */
int
fhandler_base::open (int flags, mode_t mode)
{
  int res = 0;
  HANDLE x;
  int file_attributes;
  int shared;
  int creation_distribution;

  syscall_printf ("(%s, %p)", get_win32_name (), flags);

  set_flags (flags);

  if (get_win32_name () == NULL)
    {
      set_errno (ENOENT);
      goto done;
    }

  if (get_device () == FH_TAPE)
    {
      access_ = GENERIC_READ | GENERIC_WRITE;
    }
  else if ((flags & (O_RDONLY | O_WRONLY | O_RDWR)) == O_RDONLY)
    {
      access_ = GENERIC_READ;
    }
  else if ((flags & (O_RDONLY | O_WRONLY | O_RDWR)) == O_WRONLY)
    {
      access_ = GENERIC_WRITE;
    }
  else
    {
      access_ = GENERIC_READ | GENERIC_WRITE;
    }

  /* FIXME: O_EXCL handling?  */

  if ((flags & O_TRUNC) && ((flags & O_ACCMODE) != O_RDONLY))
    {
      if (flags & O_CREAT)
	{
	  creation_distribution = CREATE_ALWAYS;
	}
      else
	{
	  creation_distribution = TRUNCATE_EXISTING;
	}
    }
  else if (flags & O_CREAT)
    creation_distribution = OPEN_ALWAYS;
  else
    creation_distribution = OPEN_EXISTING;

  if ((flags & O_EXCL) && (flags & O_CREAT))
    {
      creation_distribution = CREATE_NEW;
    }

  if (flags & O_APPEND)
    set_append_p();

  /* These flags are host dependent. */
  shared = host_dependent.shared;

  file_attributes = FILE_ATTRIBUTE_NORMAL;
  if (flags & O_DIROPEN)
    file_attributes |= FILE_FLAG_BACKUP_SEMANTICS;
  if (get_device () == FH_SERIAL)
    file_attributes |= FILE_FLAG_OVERLAPPED;

  x = CreateFileA (get_win32_name (), access_, shared,
		   &sec_none, creation_distribution,
		   file_attributes,
		   0);

  syscall_printf ("%d = CreateFileA (%s, %p, %p, %p, %p, %p, 0)",
		  x,
		  get_win32_name (), access_, shared,
		  &sec_none, creation_distribution,
		  file_attributes);

  if (x == INVALID_HANDLE_VALUE)
    {
      if (GetLastError () == ERROR_INVALID_HANDLE)
	set_errno (ENOENT);
      else
	__seterrno ();
      goto done;
    }

  if (flags & O_CREAT)
    {
      set_file_attribute (get_win32_name (), mode);
    }

  namehash_ = hash_path_name (0, get_win32_name ());
  set_handle (x);
  rpos_ = 0;
  rsize_ = -1;
  int bin;
  if (flags & (O_BINARY | O_TEXT))
    bin = flags & O_TEXT ? 0 : O_BINARY;
  else if (get_device () == FH_DISK)
    bin = get_w_binary () || get_r_binary ();
  else
    bin = (__fmode & O_BINARY) || get_w_binary () || get_r_binary ();

  set_r_binary (bin);
  set_w_binary (bin);
  syscall_printf ("filemode set to %s", bin ? "binary" : "text");

  if (get_device () != FH_TAPE && get_device () != FH_FLOPPY)
    {
      if (flags & O_APPEND)
        SetFilePointer (get_handle(), 0, 0, FILE_END);
      else
        SetFilePointer (get_handle(), 0, 0, FILE_BEGIN);
    }

  res = 1;
done:
  syscall_printf ("%d = fhandler_base::open (%s, %p)", res, get_win32_name (),
		  flags);
  return res;
}

/* states:
   open buffer in binary mode?  Just do the read.

   open buffer in text mode?  Scan buffer for control zs and handle
   the first one found.  Then scan buffer, converting every \r\n into
   an \n.  If last char is an \r, look ahead one more char, if \n then
   modify \r, if not, remember char.
*/
int
fhandler_base::read (void *in_ptr, size_t in_len)
{
  int len = (int) in_len;
  char *ctrlzpos;
  char *ptr = (char *) in_ptr;

  if (len == 0)
    return 0;

  if (get_readahead_valid ())
    {
      *ptr++ = readahead_;
      set_readahead_valid (0);
      len--;
    }

  int chars_to_process = len ? raw_read (ptr, len) : 0;
  if (chars_to_process < 0)
    return chars_to_process;

  chars_to_process += in_len - len;

  if (chars_to_process == 0 || get_r_binary ())
    return chars_to_process;

  /* If the first character is a control-z we're at virtual EOF.  */
  if (((char *) ptr)[0] == 0x1a)
    return 0;

  /* Scan buffer for a control-z and shorten the buffer to that length */

  ctrlzpos = (char *) memchr ((char *) ptr, 0x1a, chars_to_process);
  if (ctrlzpos)
    {
      lseek ((ctrlzpos - ((char *) ptr + chars_to_process)), SEEK_CUR);
      chars_to_process = ctrlzpos - (char *) ptr;
    }

  /* Scan buffer and turn \r\n into \n */
  register char *src= (char *) ptr;
  register char *dst = (char *) ptr;
  register char *end = src + chars_to_process - 1;

  /* Read up to the last but one char - the last char needs special handling */
  while (src < end)
    {
      if (src[0] == '\r' && src[1] == '\n')
	{
	  /* Ignore this. */
	  src++;
	}
      else
	{
	  *dst++ = *src++;
	}
    }

  /* if last char is a '\r' then read one more to see if we should
     translate this one too */
  if (*src == '\r')
    {
      char c;
      len = raw_read (&c, 1);
      if (len > 0)
	{
	  if (c == '\n')
	    *dst++ = '\n';
	  else
	    set_readahead_valid (1, c);
	}
    }
  else
    {
      *dst++ = *src;
    }

  chars_to_process = dst - (char *) ptr;

  rpos_ += chars_to_process;

#ifndef NOSTRACE
  if (myself->strace_mask & (_STRACE_DEBUG | _STRACE_ALL))
    {
      char buf[16 * 6 + 1];
      char *p = buf;

      for (int i = 0; i < chars_to_process && i < 16; ++i)
	{
	  unsigned char c = ((unsigned char *) ptr)[i];
	  /* >= 33 so space prints in hex */
	  __small_sprintf (p, c >= 33 && c <= 127 ? " %c" : " %p", c);
	  p += strlen (p);
	}
      debug_printf ("read %d bytes (%s%s)", chars_to_process, buf,
		    chars_to_process > 16 ? " ..." : "");
    }
#endif

  return chars_to_process;
}

int
fhandler_base::write (const void *ptr, size_t len)
{
  int res;

  if (get_append_p ())
    SetFilePointer (get_handle(), 0, 0, FILE_END);
  else if (os_being_run != winNT && get_check_win95_lseek_bug ())
    {
      /* Note: this bug doesn't happen on NT4, even though the documentation
	 for WriteFile() says that it *may* happen on any OS. */
      int actual_length, current_position;
      set_check_win95_lseek_bug (0); /* don't do it again */
      actual_length = GetFileSize (get_handle (), NULL);
      current_position = SetFilePointer (get_handle (), 0, 0, FILE_CURRENT);
      if (current_position > actual_length)
	{
	  /* Oops, this is the bug case - Win95 uses whatever is on the disk
	     instead of some known (safe) value, so we must seek back and
	     fill in the gap with zeros. - DJ */
	  char zeros[512];
	  int number_of_zeros_to_write = current_position - actual_length;
	  memset(zeros, 0, 512);
	  SetFilePointer (get_handle (), 0, 0, FILE_END);
	  while (number_of_zeros_to_write > 0)
	    {
	      DWORD zeros_this_time = (number_of_zeros_to_write > 512
				     ? 512 : number_of_zeros_to_write);
	      DWORD written;
	      if (!WriteFile (get_handle (), zeros, zeros_this_time, &written,
			      NULL))
		{
		  __seterrno ();
		  if (get_errno () == EPIPE)
		    raise (SIGPIPE);
		  /* This might fail, but it's the best we can hope for */
		  SetFilePointer (get_handle (), current_position, 0, FILE_BEGIN);
		  return -1;

		}
	      if (written < zeros_this_time) /* just in case */
		{
		  set_errno (ENOSPC);
		  /* This might fail, but it's the best we can hope for */
		  SetFilePointer (get_handle (), current_position, 0, FILE_BEGIN);
		  return -1;
		}
	      number_of_zeros_to_write -= written;
	    }
	}
    }

  if (get_w_binary ())
    {
      res = raw_write (ptr, len);
    }
  else
    {
#ifdef NOTDEF
      /* Keep track of previous \rs, we don't want to turn existing
	 \r\n's into \r\n\n's */
      register int pr = 0;

      /* Copy things in chunks */
      char buf[CHUNK_SIZE];

      for (unsigned int i = 0; i < len; i += sizeof (buf) / 2)
	{
	  register const char *src = (char *)ptr + i;
	  int todo;
	  if ((todo = len - i) > sizeof (buf) / 2)
	    todo = sizeof (buf) / 2;
	  register const char *end = src + todo;
	  register char *dst = buf;
	  while (src < end)
	    {
	      if (*src == '\n' && !pr)
		{
		  /* Emit a cr lf here */
		  *dst ++ = '\r';
		  *dst ++ = '\n';
		}
	      else if (*src == '\r')
		{
		  *dst ++ = '\r';
		  pr = 1;
		}
	      else
		{
		  *dst ++ = *src;
		  pr = 0;
		}
	      src++;
	    }
	  int want = dst - buf;
	  if ((res = raw_write (buf, want)) != want)
	    {
	      if (res == -1)
		return -1;
	      /* FIXME: */
	      /* Tricky... Didn't write everything we wanted.. How can
		 we work out exactly which chars were sent?  We don't...
		 This will only happen in pretty nasty circumstances. */
	      rpos_ += i;
	      return i;
	    }
	}
#else
      /* This is the Microsoft/DJGPP way.  Still not ideal, but it's
	 compatible. */

      int left_in_data = len;
      char *data = (char *)ptr;

      while (left_in_data > 0)
	{
	  char buf[CHUNK_SIZE], *buf_ptr = buf;
	  int left_in_buf = CHUNK_SIZE;

	  while (left_in_buf > 0 && left_in_data > 0)
	    {
	      if (*data == '\n')
		{
		  if (left_in_buf == 1)
		    {
		      /* Not enough room for \r and \n */
		      break;
		    }
		  *buf_ptr++ = '\r';
		  left_in_buf--;
		}
	      *buf_ptr++ = *data++;
	      left_in_buf--;
	      left_in_data--;
	    }

	  /* We've got a buffer-full, or we're out of data.  Write it out */
	  int want = buf_ptr - buf;
	  if ((res = raw_write (buf, want)) != want)
	    {
	      if (res == -1)
		return -1;
	      /* FIXME: */
	      /* Tricky... Didn't write everything we wanted.. How can
		 we work out exactly which chars were sent?  We don't...
		 This will only happen in pretty nasty circumstances. */
	      int i = (len-left_in_data) - left_in_buf;
	      rpos_ += i;
	      /* just in case the math is off, guarantee it looks like
		 a disk full error */
	      if (i >= (int)len)
		i = len-1;
	      if (i < 0)
		i = 0;
	      return i;
	    }
	}
#endif

      /* Done everything, update by the chars that the user sent */
      rpos_ += len;
      /* Length of file has changed */
      rsize_ = -1;
      res = len;
      debug_printf ("after write, name %s, rpos %d", unix_path_name_, rpos_);
    }
  return res;
}

off_t
fhandler_base::lseek (off_t offset, int whence)
{
  off_t res;

  /* Seeks on text files is tough, we rewind and read till we get to the
     right place.  */

  set_readahead_valid (0);

  debug_printf ("lseek (%s, %d, %d)", unix_path_name_, offset, whence);

#if 0	/* lseek has no business messing about with text-mode stuff */

  if (!get_r_binary ())
    {
      int newplace;

      if (whence == 0)
	{
	  newplace = offset;
	}
      else if (whence ==1)
	{
	  newplace = rpos +  offset;
	}
      else
	{
	  /* Seek from the end of a file.. */
	  if (rsize == -1)
	    {
	      /* Find the size of the file by reading till the end */

	      char b[CHUNK_SIZE];
	      while (read (b, sizeof (b)) > 0)
		;
	      rsize = rpos;
	    }
	  newplace = rsize + offset;
	}

      if (rpos > newplace)
	{
	  SetFilePointer (handle, 0, 0, 0);
	  rpos = 0;
	}

      /* You can never shrink something more than 50% by turning CRLF into LF,
	 so we binary chop looking for the right place */

      while (rpos < newplace)
	{
	  char b[CHUNK_SIZE];
	  size_t span = (newplace - rpos) / 2;
	  if (span == 0)
	    span = 1;
	  if (span > sizeof (b))
	    span = sizeof (b);

	  debug_printf ("lseek (%s, %d, %d) span %d, rpos %d newplace %d",
		       name, offset, whence,span,rpos, newplace);
	  read (b, span);
	}

      debug_printf ("Returning %d", newplace);
      return newplace;
    }
#endif	/* end of deleted code dealing with text mode */

  DWORD win32_whence = whence == SEEK_SET ? FILE_BEGIN
		       : (whence == SEEK_CUR ? FILE_CURRENT : FILE_END);

  res = SetFilePointer (get_handle(), offset, 0, win32_whence);
  if (res == -1)
    {
      __seterrno ();
    }
  else
    {
      /* When next we write(), we will check to see if *this* seek went beyond
	 the end of the file, and back-seek and fill with zeros if so - DJ */
      set_check_win95_lseek_bug ();
    }

  return res;
}

int
fhandler_base::close (void)
{
  int res = -1;

  syscall_printf ("handle %p", get_handle());
  if (CloseHandle (get_handle()))
    res = 0;
  else
    {
      paranoid_printf ("CloseHandle (%d <%s>) failed", get_handle(),
		       get_name ());

      __seterrno ();
    }
  return res;
}

int
fhandler_base::ioctl (unsigned int cmd, void *buf)
{
  if (cmd == FIONBIO)
    syscall_printf ("ioctl (FIONBIO, %p)", buf);
  else
    syscall_printf ("ioctl (%x, %p)", cmd, buf);

  set_errno (EINVAL);
  return -1;
}

int
fhandler_base::lock (int, struct flock *)
{
  set_errno (ENOSYS);
  return -1;
}

int
fhandler_base::fstat (struct stat *buf)
{
  return stat_dev (get_device (), get_unit (), get_namehash (), buf);
  return 0;
}

int
fhandler_disk_file::fstat (struct stat *buf)
{
  int res = 0;	// avoid a compiler warning
  BY_HANDLE_FILE_INFORMATION local;
  int old_errno = get_errno ();

  memset (buf, 0, sizeof (*buf));

  if (is_device ())
    return stat_dev (get_device (), get_unit (), get_namehash (), buf);

  /* NT 3.5.1 seems to have a bug when attempting to get vol serial
     numbers.  This loop gets around this. */
  for (int i = 0; i < 2; i++)
    {
      if (!(res = GetFileInformationByHandle (get_handle (), &local)))
	break;
      if (local.dwVolumeSerialNumber && (long) local.dwVolumeSerialNumber != -1)
	break;
    }
  debug_printf ("%d = GetFileInformationByHandle (%s, %d)",
		res, get_win32_name (), get_handle ());
  if (res == 0)
    {
      /* GetFileInformationByHandle will fail if it's given stdin/out/err 
	 or a pipe*/
      DWORD lsize, hsize;

      if (GetFileType (get_handle ()) != FILE_TYPE_DISK)
	buf->st_mode = S_IFCHR;

      lsize = GetFileSize (get_handle (), &hsize);
      if (lsize == 0xffffffff && GetLastError () != NO_ERROR)
	buf->st_mode = S_IFCHR;
      else
	buf->st_size = lsize;
      /* We expect these to fail! */
      buf->st_mode |= STD_RBITS | STD_WBITS;
      buf->st_blksize = S_BLKSIZE;
      buf->st_ino = get_namehash ();
      syscall_printf ("0 = fstat (, %p)",  buf);
      return 0;
    }

  if (!get_win32_name ())
    {
      set_errno (ENOENT);
      return -1;
    }
  set_errno (old_errno);

  buf->st_atime   = to_time_t (&local.ftLastAccessTime);
  buf->st_mtime   = to_time_t (&local.ftLastWriteTime);
  buf->st_ctime   = to_time_t (&local.ftCreationTime);
  buf->st_nlink   = local.nNumberOfLinks;
  buf->st_dev     = local.dwVolumeSerialNumber;
  buf->st_size    = local.nFileSizeLow;
  buf->st_ino     = local.nFileIndexLow ^ get_namehash ();
  buf->st_blksize = S_BLKSIZE;
  buf->st_blocks  = (buf->st_size + S_BLKSIZE-1) / S_BLKSIZE;
  buf->st_uid     = get_file_owner (get_win32_name ());
  buf->st_gid     = get_file_group (get_win32_name ());
  if (get_file_attribute (get_win32_name (), (int *) &buf->st_mode) > 0)
    {
      buf->st_mode &= ~S_IFMT;
      if (get_symlink_p ())
	buf->st_mode |= S_IFLNK;
      else
	buf->st_mode |= S_IFREG;
    }
  else
    {
      buf->st_mode = 0;
      buf->st_mode |= STD_RBITS;

      if (! (local.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
	buf->st_mode |= STD_WBITS;
      /* | S_IWGRP | S_IWOTH; we don't give write to group etc */

      if (get_symlink_p ())
	buf->st_mode |= S_IFLNK;
      else
	switch (GetFileType (get_handle ()))
	  {
	  case FILE_TYPE_CHAR:
	  case FILE_TYPE_UNKNOWN:
	    buf->st_mode |= S_IFCHR;
	    break;
	  case FILE_TYPE_DISK:
	    buf->st_mode |= S_IFREG;
	    if (get_execable_p ())
	      buf->st_mode |= STD_XBITS;
	    break;
	  case FILE_TYPE_PIPE:
	    buf->st_mode |= S_IFSOCK;
	    break;
	  }
    }

  syscall_printf ("0 = fstat (, %p) st_atime=%x st_size=%d, st_mode=%p, st_ino=%d, sizeof=%d",
		 buf, buf->st_atime, buf->st_size, buf->st_mode,
		 (int) buf->st_ino, sizeof (*buf));

  return 0;
}

void
fhandler_base::init (HANDLE f, DWORD a, mode_t bin)
{
  set_handle (f);
  set_r_binary (bin);
  set_w_binary (bin);
  access_ = a;
  a &= GENERIC_READ | GENERIC_WRITE;
  if (a == GENERIC_READ)
    set_flags (O_RDONLY);
  if (a == GENERIC_WRITE)
    set_flags (O_WRONLY);
  if (a == (GENERIC_READ | GENERIC_WRITE))
    set_flags (O_RDWR);
  debug_printf ("created new fhandler_base for handle %p", f);
}

void
fhandler_base::dump (void)
{
  paranoid_printf ("here");
}

void
fhandler_base::set_handle (HANDLE x)
{
  debug_printf ("set handle to %p", x);
  handle_ = x;
}

int
fhandler_base::dup (fhandler_base *child)
{
  debug_printf ("in fhandler_base dup");

  const HANDLE proc = GetCurrentProcess ();
  HANDLE nh;
  if (!DuplicateHandle (proc, get_handle(), proc, &nh, 0, TRUE,
			DUPLICATE_SAME_ACCESS))
    {
      system_printf ("dup(%s) failed, handle %x, %E",
		     get_name (), get_handle());
      __seterrno ();
      return -1;
    }

  child->set_handle (nh);
  return 0;
}

/* Base terminal handlers.  These just return errors.  */

int
fhandler_base::tcflush (int queue)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcsendbreak (int duration)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcdrain (void)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcflow (int action)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcsetattr (int a, const struct termios *t)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcgetattr (struct termios *t)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcsetpgrp (const pid_t pid)
{
  set_errno (ENOTTY);
  return -1;
}

int
fhandler_base::tcgetpgrp (void)
{
  set_errno (ENOTTY);
  return -1;
}

/* Normal I/O constructor */
fhandler_base::fhandler_base (DWORD devtype, const char *name, int unit)
{
  status_ = devtype;
  if (status_ != FH_DISK)
    {
      int bin = __fmode & O_TEXT ? 0 : 1;
      set_r_binary (bin);
      set_w_binary (bin);
    }
  handle_ = NULL;
  access_ = 0;
  rpos_ = 0;
  rsize_ = 0;
  namehash_ = 0;
  unix_path_name_ = NULL;
  win32_path_name_ = NULL;
  openflags_ = 0;
  set_name (name, NULL, unit);
}

/* Normal I/O destructor */
fhandler_base::~fhandler_base (void)
{
  if (!no_free_names ())
    {
      free (unix_path_name_);
      free (win32_path_name_);
    }
}

/**********************************************************************/
/* fhandler_disk_file */

static char fhandler_disk_dummy_name[] = "some disk file";

fhandler_disk_file::fhandler_disk_file (const char *name) :
	fhandler_base (FH_DISK, name)
{
  set_cb (sizeof *this);
  set_no_free_names ();
  unix_path_name_ = win32_path_name_ = fhandler_disk_dummy_name;
}

const char *fhandler_disk_file::get_native ()
{
  static path_conv  w (get_name (), SYMLINK_FOLLOW, TRUE);
  if (w.binary_p)
    {
      set_r_binary (1);
      set_w_binary (1);
    }
  return w.get_win32 ();
}

int
fhandler_disk_file::open (const char *path, int flags, mode_t mode)
{
  syscall_printf ("(%s, %p)", path, flags);

  /* O_NOSYMLINK is an internal flag for implementing lstat, nothing more. */
  path_conv real_path (path, (flags & O_NOSYMLINK) ? SYMLINK_NOFOLLOW:SYMLINK_FOLLOW);

  if (real_path.error &&
      (flags & O_NOSYMLINK || real_path.error != ENOENT || !(flags & O_CREAT)))
    {
      set_errno (real_path.error);
      syscall_printf ("0 = fhandler_disk_file::open (%s, %p)", path, flags);
      return 0;
    }

  set_name (path, real_path.get_win32 ());
  set_no_free_names (0);
  return open (real_path, flags, mode);
}

int
fhandler_disk_file::open (path_conv& real_path, int flags, mode_t mode)
{
  if (get_win32_name () == fhandler_disk_dummy_name)
    {
      win32_path_name_ = real_path.get_win32 ();
      set_no_free_names ();
    }
  /* If necessary, do various other things to see if path is a program.  */
  if (!real_path.exec_p)
    real_path.exec_p = check_execable_p (get_win32_name ());

  if (real_path.binary_p)
    {
      set_r_binary (1);
      set_w_binary (1);
    }

  int res = this->fhandler_base::open (flags, mode);

  if (!res)
    goto out;

  extern BOOL allow_ntea;

  if (!real_path.exec_p && !allow_ntea)
    {
      DWORD done;
      char magic[3];
      /* FIXME should we use /etc/magic ? */
      magic[0] = magic[1] = magic[2] = '\0';
      ReadFile (get_handle (), magic, 3, &done, 0);
      if (magic[0] == ':' && magic[1] == '\n')
	  real_path.exec_p = 1;

      if (magic[0] == '#' && magic[1] == '!')
	  real_path.exec_p = 1;
      if (!(flags & O_APPEND))
	SetFilePointer (get_handle(), 0, 0, FILE_BEGIN);
    }

  if (flags & O_APPEND)
    SetFilePointer (get_handle(), 0, 0, FILE_END);

  set_symlink_p (real_path.symlink_p);
  set_execable_p (real_path.exec_p);

out:
  syscall_printf ("%d = fhandler_disk_file::open (%s, %p)", res,
		  get_win32_name (), flags);
  return res;
}

int
fhandler_disk_file::close ()
{
  int res;
  if ((res = this->fhandler_base::close ()) == 0)
    cygwin_shared->delqueue.process_queue ();
  return res;
}

/*
 * FIXME !!!
 * The correct way to do this to get POSIX locking
 * semantics is to keep a linked list of posix lock
 * requests and map them into Win32 locks. The problem
 * is that Win32 does not deal correctly with overlapping
 * lock requests. Also another pain is that Win95 doesn't do
 * non-blocking or non exclusive locks at all. For '95 just
 * convert all lock requests into blocking,exclusive locks.
 * This shouldn't break many apps but denying all locking
 * would.
 * For now just convert to Win32 locks and hope for the best.
 */

int
fhandler_disk_file::lock (int cmd, struct flock *fl)
{
  DWORD win32_start;
  DWORD win32_len;
  DWORD win32_upper;
  DWORD startpos;

  /*
   * We don't do getlck calls yet.
   */

  if (cmd == F_GETLK)
    {
      set_errno (ENOSYS);
      return -1;
    }

  /*
   * Calculate where in the file to start from,
   * then adjust this by fl->l_start.
   */

  switch (fl->l_whence)
    {
      case SEEK_SET:
	startpos = 0;
	break;
      case SEEK_CUR:
	if ((startpos = lseek (0, SEEK_CUR)) < 0)
	  return -1;
	break;
      case SEEK_END:
	{
	  BY_HANDLE_FILE_INFORMATION finfo;
	  if (GetFileInformationByHandle (get_handle(), &finfo) == 0)
	    {
	      __seterrno ();
	      return -1;
	    }
	  startpos = finfo.nFileSizeLow; /* Nowhere to keep high word */
	  break;
	}
      default:
	set_errno (EINVAL);
	return -1;
    }

  /*
   * Now the fun starts. Adjust the start and length
   *  fields until they make sense.
   */

  win32_start = startpos + fl->l_start;
  if (fl->l_len < 0)
    {
      win32_start -= fl->l_len;
      win32_len = -fl->l_len;
    }
  else
    win32_len = fl->l_len;

  if (win32_start < 0)
    {
      win32_len -= win32_start;
      if (win32_len <= 0)
	{
	  /* Failure ! */
	  set_errno (EINVAL);
	  return -1;
	}
      win32_start = 0;
    }

  /*
   * Special case if len == 0 for POSIX means lock
   * to the end of the entire file (and all future extensions).
   */
  if (win32_len == 0)
    {
      win32_len = 0xffffffff;
      win32_upper = host_dependent.win32_upper;
    }
  else
    win32_upper = 0;

  BOOL res;

  if (os_being_run == winNT)
    {
      DWORD lock_flags = (cmd == F_SETLK) ? LOCKFILE_FAIL_IMMEDIATELY : 0;
      lock_flags |= (fl->l_type == F_WRLCK) ? LOCKFILE_EXCLUSIVE_LOCK : 0;

      OVERLAPPED ov;

      ov.Internal = 0;
      ov.InternalHigh = 0;
      ov.Offset = win32_start;
      ov.OffsetHigh = 0;
      ov.hEvent = (HANDLE) 0;

      if (fl->l_type == F_UNLCK)
	{
	  res = UnlockFileEx (get_handle (), 0, win32_len, win32_upper, &ov);
	}
      else
	{
	  res = LockFileEx (get_handle (), lock_flags, 0, win32_len,
							win32_upper, &ov);
	  /* Deal with the fail immediately case. */
	  /*
	   * FIXME !! I think this is the right error to check for
	   * but I must admit I haven't checked....
	   */
	  if ((res == 0) && (lock_flags & LOCKFILE_FAIL_IMMEDIATELY) &&
			    (GetLastError () == ERROR_LOCK_FAILED))
	    {
	      set_errno (EAGAIN);
	      return -1;
	    }
	}
    }
  else
    {
      /* Windows 95 -- use primitive lock call */
      if (fl->l_type == F_UNLCK)
	res = UnlockFile (get_handle (), win32_start, 0, win32_len,
							win32_upper);
      else
	res = LockFile (get_handle (), win32_start, 0, win32_len, win32_upper);
    }

  if (res == 0)
    {
      __seterrno ();
      return -1;
    }

  return 0;
}

/* Perform various heuristics on PATH to see if it's a program. */

int
fhandler_disk_file::check_execable_p (const char *path)
{
  int len = strlen (path);
  const char *ch = path + (len > 4 ? len - 4 : len);

  if (strcasematch (".exe", ch)
      || strcasematch (".bat", ch)
      || strcasematch (".com", ch))
    return 1;
  return 0;
}

/**********************************************************************/
/* /dev/null */

fhandler_dev_null::fhandler_dev_null (const char *name) :
	fhandler_base (FH_NULL, name)
{
  set_cb (sizeof *this);
}

void
fhandler_dev_null::dump (void)
{
  paranoid_printf ("here");
}

/**********************************************************************/
/* fhandler_pipe */

fhandler_pipe::fhandler_pipe (const char *name) :
	fhandler_base (FH_PIPE, name)
{
  set_cb (sizeof *this);
}

off_t
fhandler_pipe::lseek (off_t offset, int whence)
{
  debug_printf ("(%d, %d)", offset, whence);
  set_errno (ESPIPE);
  return -1;
}

void
set_inheritance (HANDLE &h, int val)
{
  HANDLE newh;
  HANDLE me = GetCurrentProcess ();

  if (!DuplicateHandle (me, h, me, &newh, 0, !val, DUPLICATE_SAME_ACCESS))
    debug_printf ("DuplicateHandle %E");
  else
    {
      CloseHandle (h);
      h = newh;
    }
}

void
fhandler_base::fork_fixup (HANDLE parent, HANDLE &h)
{
  HANDLE me = GetCurrentProcess ();
  if (!DuplicateHandle (parent, h, me, &h, 0, !get_close_on_exec (),
			DUPLICATE_SAME_ACCESS))
    system_printf ("%s - %E, handle %p", get_name (), h);
}

void
fhandler_base::set_close_on_exec (int val)
{
  set_inheritance (handle_, val);
  set_close_on_exec_flag (val);
  debug_printf ("set close_on_exec for %s to %d", get_name (), val);
}

void
fhandler_base::fixup_after_fork (HANDLE parent)
{
  debug_printf ("inheriting '%s' from parent", get_name ());
  fork_fixup (parent, handle_);
}
