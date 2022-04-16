/* File names and variables for bison,
   Copyright (C) 1984, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* 
 * The algorythm for finding parser files is as follows:
 *
 * If -S is specified on the command line, it's arg is the fully
 * qualified name of the "bison.simple" parser skeleton.
 *
 * If -H is specified on the command line, it's arg is the fully
 * qualified name of the "bison.hairy" parser skeleton.
 *
 * If no -S command line option, then if the BISON_SIMPLE environment
 * variable exists, it's value is the fully qualified name of the
 * "bison.simple" parser skeleton.
 *
 * If no -H command line option, then if the BISON_HAIRY environment
 * variable exists, it's value is the fully qualified name of the
 * "bison.hairy" parser skeleton.
 *
 * if -L is specified on the command line, then we look for XPFILE and
 * XPFILE1 in the directory it names.
 *
 * If no -L command line option, then if the BISONLIB environment
 * variable exists, it's value names the directory in which we look
 * for XPFILE and XPFILE1.
 *
 * If no -L command line option, and no BISONLIB environment variable,
 * then we look in XPLIB.
 *
 * XPFILE defaults to "bison.simple".
 *
 * XPFILE defaults to "bison.hairy".
 *
 * XPLIB defaults to /usr/local/lib.
 *
 */
   
/* This should be a directory name usually with a trailing slash. */

#define PLIB	XPLIB

/* These two should be simple, unqualified, pathnames for opening the
   sample parser files.  When bison is installed, they should be absolute 
   pathnames.  XPFILE1 and XPFILE2 normally come from the Makefile.  */

#define PFILE	XPFILE		/* Simple parser */
#define PFILE1	XPFILE1		/* Semantic parser */

extern FILE *finput;   /* read grammar specifications */
extern FILE *foutput;  /* optionally output messages describing the actions taken */
extern FILE *fdefines; /* optionally output #define's for token numbers. */
extern FILE *ftable;   /* output the tables and the parser */
extern FILE *fattrs;   /* if semantic parser, output a .h file that defines YYSTYPE */
		       /* and also contains all the %{ ... %} definitions.  */
extern FILE *fguard;   /* if semantic parser, output yyguard, containing all the guard code */
extern FILE *faction;  /* output all the action code; precise form depends on which parser */
extern FILE *fparser;  /* read the parser to copy into ftable */
/* CYGNUS LOCAL: internalize bison.* files */
extern const char **fparser_internal;

/* File name specified with -o for the output file, or 0 if no -o.  */
extern char *spec_outfile;

extern char *spec_name_prefix; /* for -a, from getargs.c */

extern char *bison_library;	/* for -L, from getargs.c */
extern char *simple_parser; /* for -S, from getargs.c */
extern char *hairy_parser; /* for -H, from getargs.c */

/* File name pfx specified with -b, or 0 if no -b.  */
extern char *spec_file_prefix;

extern char *infile;
extern char *outfile;
extern char *defsfile;
extern char *tabfile;
extern char *attrsfile;
extern char *guardfile;
extern char *actfile;
