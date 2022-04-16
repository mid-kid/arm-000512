/* Target machine parameters for MIPS r4000
   Copyright 1994, 1996 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor (ian@cygnus.com)

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

#define GDB_TARGET_IS_MIPS64 1

/* I've #if 0'd this as an eCos-specific hack. This will mean that all mips64
 * GDB's will be *broken* for normal 64-bit use. These hacks are to allow
 * GDB to work with eCos which puts the vr4300 into 32-bit mode. -Jifl */

#if 0
/* Use eight byte registers.  */
#define MIPS_REGSIZE 8

/* define 8 byte register type */
#define REGISTER_VIRTUAL_TYPE(N) \
        (((N) >= FP0_REGNUM && (N) < FP0_REGNUM+32) ? builtin_type_double \
	 : ((N) == 32 /*SR*/) ? builtin_type_uint32 \
	 : ((N) >= 70 && (N) <= 89) ? builtin_type_uint32 \
	 : builtin_type_long_long)

#else

#define MIPS_REGSIZE 4

#define REGISTER_BYTES ((NUM_REGS*MIPS_REGSIZE*2))

#define REGISTER_BYTE(N) ((N) * MIPS_REGSIZE * 2)

#define REGISTER_VIRTUAL_TYPE(N) \
        (((N) >= FP0_REGNUM && (N) < FP0_REGNUM+32) ? builtin_type_double \
	 : builtin_type_uint32 )

#define REGISTER_RAW_SIZE(N) 8

#endif

/* Load double words in CALL_DUMMY.  */
#define OP_LDFPR 065	/* ldc1 */
#define OP_LDGPR 067	/* ld */

#if defined(MIPS_EABI) && (MIPS_EABI != 0)
/* Define sizes for 64-bit data types, allow specific targets to override
   these values.  Doing so may violate the strict EABI, but it's necessary
   for some MIPS III and MIPS IV machines that want 64bit longs, but 32bit
   pointers.  */
#ifndef TARGET_LONG_BIT
#define TARGET_LONG_BIT      64
#endif
#ifndef TARGET_LONG_LONG_BIT
#define TARGET_LONG_LONG_BIT 64
#endif
#ifndef TARGET_PTR_BIT
#define TARGET_PTR_BIT       64
#endif
#endif /* MIPS_EABI */

/* Get the basic MIPS definitions.  */
#include "tm-mips.h"
