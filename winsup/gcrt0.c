/* gcrt0.c

   Copyright 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <sys/types.h>
#include <stdlib.h>

extern u_char etext asm("etext");
extern u_char eprol asm("eprol");
extern void _mcleanup();
extern void monstartup (u_long, u_long);

/* startup initialization for -pg support */
void
_monstartup()
{
    atexit(_mcleanup);
    monstartup((u_long)&eprol, (u_long)&etext);
    return;
}

asm(".text");
asm("eprol:");

