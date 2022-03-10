/* Support code for various pieces of CGEN simulators.
   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sim-main.h"
#include "dis-asm.h"
#include "cpu-opc.h"

#define MEMOPS_DEFINE_INLINE
#include "cgen-mem.h"

#define SEMOPS_DEFINE_INLINE
#include "cgen-ops.h"

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

const char *mode_names[] = {
  "VM",
  "BI",
  "QI",
  "HI",
  "SI",
  "DI",
  "UQI",
  "UHI",
  "USI",
  "UDI",
  "SF",
  "DF",
  "XF",
  "TF",
  0, /* MODE_TARGET_MAX */
  "INT",
  "UINT",
  "PTR"
};

/* Opcode table for virtual insns used by the simulator.  */

#define V CGEN_ATTR_MASK (CGEN_INSN_VIRTUAL)

const struct cgen_insn cgen_virtual_opcode_table[] =
{
  {
    { 0 }, VIRTUAL_INSN_X_INVALID, "--invalid--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  },
  {
    { 0 }, VIRTUAL_INSN_X_BEFORE, "--before--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  },
  {
    { 0 }, VIRTUAL_INSN_X_AFTER, "--after--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  },
  {
    { 0 }, VIRTUAL_INSN_X_BEGIN, "--begin--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  },
  {
    { 0 }, VIRTUAL_INSN_X_CHAIN, "--chain--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  },
  {
    { 0 }, VIRTUAL_INSN_X_CTI_CHAIN, "--cti-chain--", NULL,
    { { 0 } }, 0, { 0 }, 0, { CGEN_INSN_NBOOL_ATTRS, V, { 0 } }
  }
};

#undef V

/* Initialize cgen things.
   This is called after sim_post_argv_init.  */

void
cgen_init (SIM_DESC sd)
{
  /* If no profiling or tracing has been enabled, run in fast mode.  */
  {
    int i, c;
    int run_fast_p = 1;

    for (c = 0; c < MAX_NR_PROCESSORS; ++c)
      {
	sim_cpu *cpu = STATE_CPU (sd, c);

	for (i = 0; i < MAX_PROFILE_VALUES; ++i)
	  if (CPU_PROFILE_FLAGS (cpu) [i])
	    {
	      run_fast_p = 0;
	      break;
	    }
	for (i = 0; i < MAX_TRACE_VALUES; ++i)
	  if (CPU_TRACE_FLAGS (cpu) [i])
	    {
	      run_fast_p = 0;
	      break;
	    }
	if (! run_fast_p)
	  break;
      }
    STATE_RUN_FAST_P (sd) = run_fast_p;
  }
}

/* Return the name of insn number I.  */

const char *
cgen_insn_name (SIM_CPU *cpu, int i)
{
  return CGEN_INSN_NAME ((* CPU_OPCODE (cpu)) ((cpu), (i)));
}

/* Return the maximum number of extra bytes required for a sim_cpu struct.  */

int
cgen_cpu_max_extra_bytes (void)
{
  int i;
  int extra = 0;

  for (i = 0; sim_machs[i] != 0; ++i)
    {
      int size = IMP_PROPS_SIM_CPU_SIZE (MACH_IMP_PROPS (sim_machs[i]));
      if (size > extra)
	extra = size;
    }
  return extra;
}

/* Disassembly support.
   ??? While executing an instruction, the insn has been decoded and all its
   fields have been extracted.  It is certainly possible to do the disassembly
   with that data.  This seems simpler, but maybe in the future the already
   extracted fields will be used.  */

/* Pseudo FILE object for strings.  */
typedef struct {
  char *buffer;
  char *current;
} SFILE;

/* sprintf to a "stream" */

static int
disasm_sprintf VPARAMS ((SFILE *f, const char *format, ...))
{
#ifndef __STDC__
  SFILE *f;
  const char *format;
#endif
  int n;
  va_list args;

  VA_START (args, format);
#ifndef __STDC__
  f = va_arg (args, SFILE *);
  format = va_arg (args, char *);
#endif
  vsprintf (f->current, format, args);
  f->current += n = strlen (f->current);
  va_end (args);
  return n;
}

void
sim_disassemble_insn (SIM_CPU *cpu, const CGEN_INSN *insn,
		      const ARGBUF *abuf, IADDR pc, char *buf,
		      CGEN_FIELDS *fields)
{
  unsigned int length;
  unsigned long insn_value;
  struct disassemble_info disasm_info;
  SFILE sfile;
  union {
    unsigned8 bytes[CGEN_MAX_INSN_SIZE];
    unsigned16 shorts[8];
    unsigned32 words[4];
  } insn_buf;
  SIM_DESC sd = CPU_STATE (cpu);
  CGEN_OPCODE_DESC od = STATE_OPCODE_TABLE (sd);
  CGEN_EXTRACT_INFO ex_info;

  sfile.buffer = sfile.current = buf;
  INIT_DISASSEMBLE_INFO (disasm_info, (FILE *) &sfile,
			 (fprintf_ftype) disasm_sprintf);
  disasm_info.endian =
    (bfd_big_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_BIG
     : bfd_little_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_LITTLE
     : BFD_ENDIAN_UNKNOWN);

  length = sim_core_read_buffer (sd, cpu, read_map, &insn_buf, pc,
				 CGEN_INSN_BITSIZE (insn) / 8);

  switch (min (CGEN_BASE_INSN_SIZE, CGEN_INSN_BITSIZE (insn) / 8))
    {
    case 0 : return; /* fake insn, typically "compile" (aka "invalid") */
    case 1 : insn_value = insn_buf.bytes[0]; break;
    case 2 : insn_value = T2H_2 (insn_buf.shorts[0]); break;
    case 4 : insn_value = T2H_4 (insn_buf.words[0]); break;
    default: abort ();
    }

  disasm_info.buffer_vma = pc;
  disasm_info.buffer = insn_buf.bytes;
  disasm_info.buffer_length = length;

  ex_info.dis_info = (PTR) &disasm_info;
  ex_info.valid = (1 << length) - 1;
  ex_info.insn_bytes = insn_buf.bytes;

  length = (*CGEN_EXTRACT_FN (insn)) (od, insn, &ex_info, insn_value, fields, pc);
  /* Result of extract fn is in bits.  */
  /* ??? This assumes that each instruction has a fixed length (and thus
     for insns with multiple versions of variable lengths they would each
     have their own table entry).  */
  if (length == CGEN_INSN_BITSIZE (insn))
    {
      (*CGEN_PRINT_FN (insn)) (od, &disasm_info, insn, fields, pc, length);
    }
  else
    {
      /* This shouldn't happen, but aborting is too drastic.  */
      strcpy (buf, "***unknown***");
    }
}

#ifdef DI_FN_SUPPORT

DI
make_struct_di (hi, lo)
     SI hi, lo;
{
  DI result;

  result.hi = hi;
  result.lo = lo;
  return result;
}

DI
ANDDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  SI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  SI blo = GETLODI (b);
  return MAKEDI (ahi & bhi, alo & blo);
}

DI
ORDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  SI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  SI blo = GETLODI (b);
  return MAKEDI (ahi | bhi, alo | blo);
}

DI
ADDDI (a, b)
     DI a, b;
{
  USI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  USI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  USI x = alo + blo;
  return MAKEDI (ahi + bhi + (x < alo), x);
}

DI
MULDI (a, b)
     DI a, b;
{
  USI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  USI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  USI rhi,rlo;
  USI x0, x1, x2, x3;

  x0 = alo * blo;
  x1 = alo * bhi;
  x2 = ahi * blo;
  x3 = ahi * bhi;

#define SI_TYPE_SIZE 32
#define BITS4 (SI_TYPE_SIZE / 4)
#define ll_B (1L << (SI_TYPE_SIZE / 2))
#define ll_lowpart(t) ((USI) (t) % ll_B)
#define ll_highpart(t) ((USI) (t) / ll_B)
  x1 += ll_highpart (x0);	/* this can't give carry */
  x1 += x2;			/* but this indeed can */
  if (x1 < x2)			/* did we get it? */
    x3 += ll_B;			/* yes, add it in the proper pos. */

  rhi = x3 + ll_highpart (x1);
  rlo = ll_lowpart (x1) * ll_B + ll_lowpart (x0);
  return MAKEDI (rhi + (alo * bhi) + (ahi * blo), rlo);
}

DI
SHLDI (val, shift)
     DI val;
     SI shift;
{
  USI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI ((hi << shift) | (lo >> (32 - shift)), lo << shift);
}

DI
SLADI (val, shift)
     DI val;
     SI shift;
{
  SI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI ((hi << shift) | (lo >> (32 - shift)), lo << shift);
}

DI
SRADI (val, shift)
     DI val;
     SI shift;
{
  SI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* We use SRASI because the result is implementation defined if hi < 0.  */
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI (SRASI (hi, shift), (hi << (32 - shift)) | (lo >> shift));
}

int
GEDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  if (ahi > bhi)
    return 1;
  if (ahi == bhi)
    return alo >= blo;
  return 0;
}

int
LEDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  if (ahi < bhi)
    return 1;
  if (ahi == bhi)
    return alo <= blo;
  return 0;
}

DI
CONVHIDI (val)
     HI val;
{
  if (val < 0)
    return MAKEDI (-1, val);
  else
    return MAKEDI (0, val);
}

DI
CONVSIDI (val)
     SI val;
{
  if (val < 0)
    return MAKEDI (-1, val);
  else
    return MAKEDI (0, val);
}

SI
CONVDISI (val)
     DI val;
{
  return GETLODI (val);
}

#endif /* DI_FN_SUPPORT */
