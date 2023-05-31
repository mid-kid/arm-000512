/* This is a modification of file AOUT2EXE.C */
/*
** Copyright (C) 1993 DJ Delorie, 24 Kirsten Ave, Rochester NH 03867-2954
**
** This file is distributed under the terms listed in the document
** "copying.dj", available from DJ Delorie at the address above.
** A copy of "copying.dj" should accompany this file; if not, a copy
** should be available from where this file was obtained.  This file
** may not be distributed without a verbatim copy of "copying.dj".
**
** This file is distributed WITHOUT ANY WARRANTY; without even the implied
** warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

/* Local modifications have been made by Cygnus */

/* these includes should be autoconfiscated */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "stubbyte.h"

int stub_file = -1;
long stub_length = 0;

unsigned long align_hsize(unsigned long s, unsigned char *h)
{
  long hsize = (s+511) & ~511;
  h[2] = 0;
  h[3] = 0;
  h[4] = ( hsize / 512 ) & 0xff;
  h[5] = ( hsize / 512 ) >> 8;
  return hsize;
}

void coff2exe(char *fname)
{
  int ifile;
  int ofile;
  char *ofname;
  char buf[4096];
  int rbytes;
  long hsize;

  ifile = open(fname, O_RDONLY|O_BINARY);
  if (ifile < 0)
  {
    perror(fname);
    return;
  }

  ofname = strrchr(fname, '/');
  if (ofname == 0)
    ofname = strrchr(fname, '\\');
  if (ofname == 0)
    ofname = fname;
  ofname = strrchr(ofname, '.');
  if (ofname) *ofname = 0;

  ofname = (char *)malloc(strlen(fname)+5);
  sprintf(ofname, "%s.exe", fname);
  ofile = open(ofname, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
  if (ofile < 0)
  {
    perror(ofname);
    free(ofname);
    return;
  }

  if (stub_length)
  {
    long left = stub_length;
    long wrote = 0;
    lseek(stub_file, 0L, 0);
    while (left)
    {
      char buf[512];
      int r = read(stub_file, buf, 512);
      if (wrote == 0)
        hsize = align_hsize(stub_length, (unsigned char *)buf);
      write(ofile, buf, r);
      left -= r;
      wrote += r;
    }
    write(ofile, buf, 512-((int)wrote&511));
  }
  else
  {
    hsize = align_hsize(sizeof(stub_bytes), (unsigned char *)stub_bytes);
    write(ofile, stub_bytes, (int)hsize);
  }
  
  while ((rbytes=read(ifile, buf, 4096)) > 0)
  {
    int wb = write(ofile, buf, rbytes);
    if (wb < 0)
    {
      perror(ofname);
      break;
    }
    if (wb < rbytes)
    {
      fprintf(stderr, "%s: disk full\n", ofname);
      exit(1);
    }
  }
  close(ifile);
  close(ofile);
  free(ofname);
/*  remove(fname); */
}

int main(int argc, char **argv)
{
  int i;
  if (argc > 2 && strcmp(argv[1], "-s") == 0)
  {
    struct stat s;
    stub_file = open(argv[2], O_RDONLY);
    if (stub_file < 0)
    {
      perror(argv[2]);
      exit(1);
    }
    fstat(stub_file, &s);
    stub_length = s.st_size;
    argc -= 2;
    argv += 2;
  }
  for (i=1; i<argc; i++)
    coff2exe(argv[i]);
  return 0;
}

