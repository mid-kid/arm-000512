/*
 * Copyright (c) 1995, 1996 Cygnus Support.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * at Cygnus Support, Inc.  Cygnus Support, Inc. may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
int
main(ac, av)
int ac;
char *av[];
{
    FILE *in;
    FILE *out;
    int ch;
    char tmpfile[BUFSIZ];
    
    sprintf(tmpfile, "/tmp/udtmp%d", getpid());

    if (ac != 3) {
        fprintf(stderr,"%s: Usage %s in out\n", av[0], av[0]);
        exit(1);
    }

    in  = fopen (av[1],"r");
    out = fopen (tmpfile,"w");

    if (!in) {
        fprintf(stderr,"%s: Can't open input file %s\n", av[0], av[1]);
        exit(1);
    }
    if (!out) {
        fprintf(stderr,"%s: Can't open output file %s\n", av[0], av[1]);
        exit(1);
    }

    ch = fgetc (in);
    while (ch != EOF) {
        if (ch == '\n')
            fputc('\r', out);
        fputc(ch, out);

        ch = fgetc (in);
    }

    fclose (in);
    fclose (out);

    in  = fopen (tmpfile,"r");
    out = fopen (av[2],"w");

    if (!in) {
        fprintf(stderr,"%s: Can't open input file %s\n", av[0], av[1]);
        exit(1);
    }
    if (!out) {
        fprintf(stderr,"%s: Can't open output file %s\n", av[0], av[1]);
        exit(1);
    }

    ch = fgetc (in);
    while (ch != EOF) {
        fputc(ch, out);
        ch = fgetc (in);
    }

    fclose (in);
    fclose (out);

    exit(0);
}
