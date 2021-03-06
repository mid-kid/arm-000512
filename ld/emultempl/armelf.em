# This shell script emits a C file. -*- C -*-
# It does some substitutions.
cat >e${EMULATION_NAME}.c <<EOF
/* This file is is generated by a shell script.  DO NOT EDIT! */

/* emulate the original gld for the given ${EMULATION_NAME}
   Copyright (C) 1991, 93, 96, 97, 98, 1999 Free Software Foundation, Inc.
   Written by Steve Chamberlain steve@cygnus.com

This file is part of GLD, the Gnu Linker.

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

#define TARGET_IS_${EMULATION_NAME}

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "getopt.h"

#include "ld.h"
#include "ldmain.h"
#include "ldemul.h"
#include "ldfile.h"
#include "ldmisc.h"

#include "ldexp.h"
#include "ldlang.h"

static void gld${EMULATION_NAME}_before_parse PARAMS ((void));
static void gld${EMULATION_NAME}_before_allocation PARAMS ((void));
static char *gld${EMULATION_NAME}_get_script PARAMS ((int *isfile));
static int  gld${EMULATION_NAME}_parse_args PARAMS((int, char **));
static void gld${EMULATION_NAME}_list_options PARAMS ((FILE *));


static int no_pipeline_knowledge = 0;

static struct option longopts[] =
{
  { "no-pipeline-knowledge", no_argument, NULL, 'p'},
  { NULL, no_argument, NULL, 0 }
};

static void
gld${EMULATION_NAME}_list_options (file)
     FILE * file;
{
  fprintf (file, _("  -p --no-pipeline-knowledge  Stop the linker knowing about the pipeline length\n"));
}

static int
gld${EMULATION_NAME}_parse_args (argc, argv)
     int     argc;
     char ** argv;
{
  int        longind;
  int        optc;
  int        prevoptind = optind;
  int        prevopterr = opterr;
  int        wanterror;
  static int lastoptind = -1;

  if (lastoptind != optind)
    opterr = 0;
  
  wanterror  = opterr;
  lastoptind = optind;

  optc   = getopt_long_only (argc, argv, "-p", longopts, & longind);
  opterr = prevopterr;

  switch (optc)
    {
    default:
      if (wanterror)
	xexit (1);
      optind =  prevoptind;
      return 0;

    case 'p':
      no_pipeline_knowledge = 1;
      break;
    }
  
  return 1;
}


static void
gld${EMULATION_NAME}_before_parse ()
{
#ifndef TARGET_			/* I.e., if not generic.  */
  ldfile_set_output_arch ("`echo ${ARCH}`");
#endif /* not TARGET_ */
}

/* This is called after the sections have been attached to output
   sections, but before any sizes or addresses have been set.  */

static void
gld${EMULATION_NAME}_before_allocation ()
{
  /* we should be able to set the size of the interworking stub section */

  /* Here we rummage through the found bfds to collect glue information */
  /* FIXME: should this be based on a command line option? krk@cygnus.com */
  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	if (!bfd_elf32_arm_process_before_allocation (is->the_bfd, &link_info,
						      no_pipeline_knowledge))
	  {
	    /* xgettext:c-format */
	    einfo (_("Errors encountered processing file %s"), is->filename);
	  }
      }
  }

  /* We have seen it all. Allocate it, and carry on */
  bfd_elf32_arm_allocate_interworking_sections (& link_info);
}

static void
gld${EMULATION_NAME}_after_open ()
{

  LANG_FOR_EACH_INPUT_STATEMENT (is)
    {
      /* The interworking bfd must be the last one to be processed */
      if (!is->next)
         bfd_elf32_arm_get_bfd_for_interworking (is->the_bfd, & link_info);
    }
}

static char *
gld${EMULATION_NAME}_get_script (isfile)
     int *isfile;
EOF

if test -n "$COMPILE_IN"
then
# Scripts compiled in.

# sed commands to quote an ld script as a C string.
sc="-f ${srcdir}/emultempl/stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 0;

  if (link_info.relocateable == true && config.build_constructors == true)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                     >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocateable == true) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                     >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'         >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                    >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                     >> e${EMULATION_NAME}.c
echo '  ; else return'                                     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                      >> e${EMULATION_NAME}.c
echo '; }'                                                 >> e${EMULATION_NAME}.c

else
# Scripts read from the filesystem.

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 1;

  if (link_info.relocateable == true && config.build_constructors == true)
    return "ldscripts/${EMULATION_NAME}.xu";
  else if (link_info.relocateable == true)
    return "ldscripts/${EMULATION_NAME}.xr";
  else if (!config.text_read_only)
    return "ldscripts/${EMULATION_NAME}.xbn";
  else if (!config.magic_demand_paged)
    return "ldscripts/${EMULATION_NAME}.xn";
  else
    return "ldscripts/${EMULATION_NAME}.x";
}
EOF

fi

cat >>e${EMULATION_NAME}.c <<EOF

struct ld_emulation_xfer_struct ld_${EMULATION_NAME}_emulation = 
{
  gld${EMULATION_NAME}_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  gld${EMULATION_NAME}_after_open,
  after_allocation_default,
  set_output_arch_default,
  ldemul_default_target,
  gld${EMULATION_NAME}_before_allocation,
  gld${EMULATION_NAME}_get_script,
  "${EMULATION_NAME}",
  "${OUTPUT_FORMAT}",
  NULL, /* finish */
  NULL, /* create output section statements */
  NULL, /* open dynamic archive */
  NULL, /* place orphan */
  NULL, /* set_symbols */
  gld${EMULATION_NAME}_parse_args,
  NULL, /* unrecognised file */
  gld${EMULATION_NAME}_list_options,
  NULL /* recognized file */
};
EOF
