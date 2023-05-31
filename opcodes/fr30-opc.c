/* Generic opcode table support for targets using CGEN. -*- C -*-
   CGEN: Cpu tools GENerator

THIS FILE IS USED TO GENERATE fr30-opc.c.

Copyright (C) 1998, 1999 Free Software Foundation, Inc.

This file is part of the GNU Binutils and GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "fr30-opc.h"
#include "opintl.h"

/* Used by the ifield rtx function.  */
#define FLD(f) (fields->f)

/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int asm_hash_insn PARAMS ((const char *));
static int dis_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int dis_hash_insn PARAMS ((const char *, CGEN_INSN_INT));

/* Look up instruction INSN_VALUE and extract its fields.
   INSN, if non-null, is the insn table entry.
   Otherwise INSN_VALUE is examined to compute it.
   LENGTH is the bit length of INSN_VALUE if known, otherwise 0.
   0 is only valid if `insn == NULL && ! CGEN_INT_INSN_P'.
   If INSN != NULL, LENGTH must be valid.
   ALIAS_P is non-zero if alias insns are to be included in the search.

   The result is a pointer to the insn table entry, or NULL if the instruction
   wasn't recognized.  */

const CGEN_INSN *
fr30_cgen_lookup_insn (od, insn, insn_value, length, fields, alias_p)
     CGEN_OPCODE_DESC od;
     const CGEN_INSN *insn;
     CGEN_INSN_BYTES insn_value;
     int length;
     CGEN_FIELDS *fields;
     int alias_p;
{
  unsigned char buf[CGEN_MAX_INSN_SIZE];
  unsigned char *bufp;
  CGEN_INSN_INT base_insn;
#if CGEN_INT_INSN_P
  CGEN_EXTRACT_INFO *info = NULL;
#else
  CGEN_EXTRACT_INFO ex_info;
  CGEN_EXTRACT_INFO *info = &ex_info;
#endif

#if CGEN_INT_INSN_P
  cgen_put_insn_value (od, buf, length, insn_value);
  bufp = buf;
  base_insn = insn_value; /*???*/
#else
  ex_info.dis_info = NULL;
  ex_info.insn_bytes = insn_value;
  ex_info.valid = -1;
  base_insn = cgen_get_insn_value (od, buf, length);
  bufp = insn_value;
#endif

  if (!insn)
    {
      const CGEN_INSN_LIST *insn_list;

      /* The instructions are stored in hash lists.
	 Pick the first one and keep trying until we find the right one.  */

      insn_list = CGEN_DIS_LOOKUP_INSN (od, bufp, base_insn);
      while (insn_list != NULL)
	{
	  insn = insn_list->insn;

	  if (alias_p
	      || ! CGEN_INSN_ATTR (insn, CGEN_INSN_ALIAS))
	    {
	      /* Basic bit mask must be correct.  */
	      /* ??? May wish to allow target to defer this check until the
		 extract handler.  */
	      if ((base_insn & CGEN_INSN_BASE_MASK (insn))
		  == CGEN_INSN_BASE_VALUE (insn))
		{
		  /* ??? 0 is passed for `pc' */
		  int elength = (*CGEN_EXTRACT_FN (insn)) (od, insn, info,
							   base_insn, fields,
							   (bfd_vma) 0);
		  if (elength > 0)
		    {
		      /* sanity check */
		      if (length != 0 && length != elength)
			abort ();
		      return insn;
		    }
		}
	    }

	  insn_list = CGEN_DIS_NEXT_INSN (insn_list);
	}
    }
  else
    {
      /* Sanity check: can't pass an alias insn if ! alias_p.  */
      if (! alias_p
	  && CGEN_INSN_ATTR (insn, CGEN_INSN_ALIAS))
	abort ();
      /* Sanity check: length must be correct.  */
      if (length != CGEN_INSN_BITSIZE (insn))
	abort ();

      /* ??? 0 is passed for `pc' */
      length = (*CGEN_EXTRACT_FN (insn)) (od, insn, info, base_insn, fields,
					  (bfd_vma) 0);
      /* Sanity check: must succeed.
	 Could relax this later if it ever proves useful.  */
      if (length == 0)
	abort ();
      return insn;
    }

  return NULL;
}

/* Fill in the operand instances used by INSN whose operands are FIELDS.
   INDICES is a pointer to a buffer of MAX_OPERAND_INSTANCES ints to be filled
   in.  */

void
fr30_cgen_get_insn_operands (od, insn, fields, indices)
     CGEN_OPCODE_DESC od;
     const CGEN_INSN * insn;
     const CGEN_FIELDS * fields;
     int *indices;
{
  const CGEN_OPERAND_INSTANCE *opinst;
  int i;

  for (i = 0, opinst = CGEN_INSN_OPERANDS (insn);
       opinst != NULL
	 && CGEN_OPERAND_INSTANCE_TYPE (opinst) != CGEN_OPERAND_INSTANCE_END;
       ++i, ++opinst)
    {
      const CGEN_OPERAND *op = CGEN_OPERAND_INSTANCE_OPERAND (opinst);
      if (op == NULL)
	indices[i] = CGEN_OPERAND_INSTANCE_INDEX (opinst);
      else
	indices[i] = fr30_cgen_get_int_operand (CGEN_OPERAND_INDEX (op),
						  fields);
    }
}

/* Cover function to fr30_cgen_get_insn_operands when either INSN or FIELDS
   isn't known.
   The INSN, INSN_VALUE, and LENGTH arguments are passed to
   fr30_cgen_lookup_insn unchanged.

   The result is the insn table entry or NULL if the instruction wasn't
   recognized.  */

const CGEN_INSN *
fr30_cgen_lookup_get_insn_operands (od, insn, insn_value, length, indices)
     CGEN_OPCODE_DESC od;
     const CGEN_INSN *insn;
     CGEN_INSN_BYTES insn_value;
     int length;
     int *indices;
{
  CGEN_FIELDS fields;

  /* Pass non-zero for ALIAS_P only if INSN != NULL.
     If INSN == NULL, we want a real insn.  */
  insn = fr30_cgen_lookup_insn (od, insn, insn_value, length, &fields,
				  insn != NULL);
  if (! insn)
    return NULL;

  fr30_cgen_get_insn_operands (od, insn, &fields, indices);
  return insn;
}
/* Attributes.  */

static const CGEN_ATTR_ENTRY bool_attr[] =
{
  { "#f", 0 },
  { "#t", 1 },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY MACH_attr[] =
{
  { "base", MACH_BASE },
  { "fr30", MACH_FR30 },
  { "max", MACH_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_ifield_attr_table[] =
{
  { "MACH", & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNSIGNED", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "RESERVED", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNSIGNED", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "FUN-ACCESS", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_operand_attr_table[] =
{
  { "MACH", & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNSIGNED", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "NEGATIVE", &bool_attr[0], &bool_attr[0] },
  { "RELAX", &bool_attr[0], &bool_attr[0] },
  { "SEM-ONLY", &bool_attr[0], &bool_attr[0] },
  { "HASH-PREFIX", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_insn_attr_table[] =
{
  { "MACH", & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNCOND-CTI", &bool_attr[0], &bool_attr[0] },
  { "COND-CTI", &bool_attr[0], &bool_attr[0] },
  { "SKIP-CTI", &bool_attr[0], &bool_attr[0] },
  { "DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { "RELAXABLE", &bool_attr[0], &bool_attr[0] },
  { "RELAX", &bool_attr[0], &bool_attr[0] },
  { "ALIAS", &bool_attr[0], &bool_attr[0] },
  { "NO-DIS", &bool_attr[0], &bool_attr[0] },
  { "PBB", &bool_attr[0], &bool_attr[0] },
  { "NOT-IN-DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_gr_entries[] = 
{
  { "r0", 0 },
  { "r1", 1 },
  { "r2", 2 },
  { "r3", 3 },
  { "r4", 4 },
  { "r5", 5 },
  { "r6", 6 },
  { "r7", 7 },
  { "r8", 8 },
  { "r9", 9 },
  { "r10", 10 },
  { "r11", 11 },
  { "r12", 12 },
  { "r13", 13 },
  { "r14", 14 },
  { "r15", 15 },
  { "ac", 13 },
  { "fp", 14 },
  { "sp", 15 }
};

CGEN_KEYWORD fr30_cgen_opval_h_gr = 
{
  & fr30_cgen_opval_h_gr_entries[0],
  19
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_cr_entries[] = 
{
  { "cr0", 0 },
  { "cr1", 1 },
  { "cr2", 2 },
  { "cr3", 3 },
  { "cr4", 4 },
  { "cr5", 5 },
  { "cr6", 6 },
  { "cr7", 7 },
  { "cr8", 8 },
  { "cr9", 9 },
  { "cr10", 10 },
  { "cr11", 11 },
  { "cr12", 12 },
  { "cr13", 13 },
  { "cr14", 14 },
  { "cr15", 15 }
};

CGEN_KEYWORD fr30_cgen_opval_h_cr = 
{
  & fr30_cgen_opval_h_cr_entries[0],
  16
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_dr_entries[] = 
{
  { "tbr", 0 },
  { "rp", 1 },
  { "ssp", 2 },
  { "usp", 3 },
  { "mdh", 4 },
  { "mdl", 5 }
};

CGEN_KEYWORD fr30_cgen_opval_h_dr = 
{
  & fr30_cgen_opval_h_dr_entries[0],
  6
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_ps_entries[] = 
{
  { "ps", 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_ps = 
{
  & fr30_cgen_opval_h_ps_entries[0],
  1
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r13_entries[] = 
{
  { "r13", 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r13 = 
{
  & fr30_cgen_opval_h_r13_entries[0],
  1
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r14_entries[] = 
{
  { "r14", 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r14 = 
{
  & fr30_cgen_opval_h_r14_entries[0],
  1
};

CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r15_entries[] = 
{
  { "r15", 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r15 = 
{
  & fr30_cgen_opval_h_r15_entries[0],
  1
};


/* The hardware table.  */

#define HW_ENT(n) fr30_cgen_hw_entries[n]
static const CGEN_HW_ENTRY fr30_cgen_hw_entries[] =
{
  { HW_H_PC, & HW_ENT (HW_H_PC + 1), "h-pc", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_PROFILE)|(1<<CGEN_HW_PC), { (1<<MACH_BASE) } } },
  { HW_H_MEMORY, & HW_ENT (HW_H_MEMORY + 1), "h-memory", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_SINT, & HW_ENT (HW_H_SINT + 1), "h-sint", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_UINT, & HW_ENT (HW_H_UINT + 1), "h-uint", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_ADDR, & HW_ENT (HW_H_ADDR + 1), "h-addr", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_IADDR, & HW_ENT (HW_H_IADDR + 1), "h-iaddr", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_GR, & HW_ENT (HW_H_GR + 1), "h-gr", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_gr, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_CACHE_ADDR)|(1<<CGEN_HW_PROFILE), { (1<<MACH_BASE) } } },
  { HW_H_CR, & HW_ENT (HW_H_CR + 1), "h-cr", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_cr, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_DR, & HW_ENT (HW_H_DR + 1), "h-dr", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_dr, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { HW_H_PS, & HW_ENT (HW_H_PS + 1), "h-ps", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_ps, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { HW_H_R13, & HW_ENT (HW_H_R13 + 1), "h-r13", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r13, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_R14, & HW_ENT (HW_H_R14 + 1), "h-r14", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r14, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_R15, & HW_ENT (HW_H_R15 + 1), "h-r15", CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r15, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_NBIT, & HW_ENT (HW_H_NBIT + 1), "h-nbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_ZBIT, & HW_ENT (HW_H_ZBIT + 1), "h-zbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_VBIT, & HW_ENT (HW_H_VBIT + 1), "h-vbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_CBIT, & HW_ENT (HW_H_CBIT + 1), "h-cbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_IBIT, & HW_ENT (HW_H_IBIT + 1), "h-ibit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_SBIT, & HW_ENT (HW_H_SBIT + 1), "h-sbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { HW_H_TBIT, & HW_ENT (HW_H_TBIT + 1), "h-tbit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_D0BIT, & HW_ENT (HW_H_D0BIT + 1), "h-d0bit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_D1BIT, & HW_ENT (HW_H_D1BIT + 1), "h-d1bit", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } } },
  { HW_H_CCR, & HW_ENT (HW_H_CCR + 1), "h-ccr", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { HW_H_SCR, & HW_ENT (HW_H_SCR + 1), "h-scr", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { HW_H_ILM, & HW_ENT (HW_H_ILM + 1), "h-ilm", CGEN_ASM_KEYWORD, (PTR) 0, { CGEN_HW_NBOOL_ATTRS, 0|(1<<CGEN_HW_FUN_ACCESS), { (1<<MACH_BASE) } } },
  { 0 }
};

/* The instruction field table.  */

static const CGEN_IFLD fr30_cgen_ifld_table[] =
{
  { FR30_F_NIL, "f-nil", 0, 0, 0, 0, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP1, "f-op1", 0, 16, 0, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_OP2, "f-op2", 0, 16, 4, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_OP3, "f-op3", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_OP4, "f-op4", 0, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_OP5, "f-op5", 0, 16, 4, 1, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_CC, "f-cc", 0, 16, 4, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_CCC, "f-ccc", 16, 16, 0, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RJ, "f-Rj", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RI, "f-Ri", 0, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RS1, "f-Rs1", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RS2, "f-Rs2", 0, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RJC, "f-Rjc", 16, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_RIC, "f-Ric", 16, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_CRJ, "f-CRj", 16, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_CRI, "f-CRi", 16, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_U4, "f-u4", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_U4C, "f-u4c", 0, 16, 12, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_I4, "f-i4", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_M4, "f-m4", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_U8, "f-u8", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_I8, "f-i8", 0, 16, 4, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_I20_4, "f-i20-4", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_I20_16, "f-i20-16", 16, 16, 0, 16, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_I32, "f-i32", 16, 32, 0, 32, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_SIGN_OPT)|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_UDISP6, "f-udisp6", 0, 16, 8, 4, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_DISP8, "f-disp8", 0, 16, 4, 8, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DISP9, "f-disp9", 0, 16, 4, 8, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DISP10, "f-disp10", 0, 16, 4, 8, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_S10, "f-s10", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
  { FR30_F_U10, "f-u10", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_REL9, "f-rel9", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { FR30_F_DIR8, "f-dir8", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_DIR9, "f-dir9", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_DIR10, "f-dir10", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_REL12, "f-rel12", 0, 16, 5, 11, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_HI_ST, "f-reglist_hi_st", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_LOW_ST, "f-reglist_low_st", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_HI_LD, "f-reglist_hi_ld", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_LOW_LD, "f-reglist_low_ld", 0, 16, 8, 8, { CGEN_IFLD_NBOOL_ATTRS, 0|(1<<CGEN_IFLD_UNSIGNED), { (1<<MACH_BASE) } }  },
  { 0 }
};

/* The operand table.  */

#define OPERAND(op) CONCAT2 (FR30_OPERAND_,op)
#define OP_ENT(op) fr30_cgen_operand_table[OPERAND (op)]

const CGEN_OPERAND fr30_cgen_operand_table[MAX_OPERANDS] =
{
/* pc: program counter */
  { "pc", & HW_ENT (HW_H_PC), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* Ri: destination register */
  { "Ri", & HW_ENT (HW_H_GR), 12, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* Rj: source register */
  { "Rj", & HW_ENT (HW_H_GR), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* Ric: target register coproc insn */
  { "Ric", & HW_ENT (HW_H_GR), 12, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* Rjc: source register coproc insn */
  { "Rjc", & HW_ENT (HW_H_GR), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* CRi: coprocessor register */
  { "CRi", & HW_ENT (HW_H_CR), 12, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* CRj: coprocessor register */
  { "CRj", & HW_ENT (HW_H_CR), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* Rs1: dedicated register */
  { "Rs1", & HW_ENT (HW_H_DR), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* Rs2: dedicated register */
  { "Rs2", & HW_ENT (HW_H_DR), 12, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* R13: General Register 13 */
  { "R13", & HW_ENT (HW_H_R13), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
/* R14: General Register 14 */
  { "R14", & HW_ENT (HW_H_R14), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
/* R15: General Register 15 */
  { "R15", & HW_ENT (HW_H_R15), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
/* ps: Program Status register */
  { "ps", & HW_ENT (HW_H_PS), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }  },
/* u4: 4  bit unsigned immediate */
  { "u4", & HW_ENT (HW_H_UINT), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* u4c: 4  bit unsigned immediate */
  { "u4c", & HW_ENT (HW_H_UINT), 12, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* u8: 8  bit unsigned immediate */
  { "u8", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* i8: 8  bit unsigned immediate */
  { "i8", & HW_ENT (HW_H_UINT), 4, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* udisp6: 6  bit unsigned immediate */
  { "udisp6", & HW_ENT (HW_H_UINT), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* disp8: 8  bit signed   immediate */
  { "disp8", & HW_ENT (HW_H_SINT), 4, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* disp9: 9  bit signed   immediate */
  { "disp9", & HW_ENT (HW_H_SINT), 4, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* disp10: 10 bit signed   immediate */
  { "disp10", & HW_ENT (HW_H_SINT), 4, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* s10: 10 bit signed   immediate */
  { "s10", & HW_ENT (HW_H_SINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* u10: 10 bit unsigned immediate */
  { "u10", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* i32: 32 bit immediate */
  { "i32", & HW_ENT (HW_H_UINT), 0, 32,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_SIGN_OPT)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* m4: 4  bit negative immediate */
  { "m4", & HW_ENT (HW_H_SINT), 8, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* i20: 20 bit immediate */
  { "i20", & HW_ENT (HW_H_UINT), 0, 20,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED)|(1<<CGEN_OPERAND_VIRTUAL), { (1<<MACH_BASE) } }  },
/* dir8: 8  bit direct address */
  { "dir8", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* dir9: 9  bit direct address */
  { "dir9", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* dir10: 10 bit direct address */
  { "dir10", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* label9: 9  bit pc relative address */
  { "label9", & HW_ENT (HW_H_IADDR), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* label12: 12 bit pc relative address */
  { "label12", & HW_ENT (HW_H_IADDR), 5, 11,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* reglist_low_ld: 8 bit low register mask for ldm */
  { "reglist_low_ld", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* reglist_hi_ld: 8 bit high register mask for ldm */
  { "reglist_hi_ld", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* reglist_low_st: 8 bit low register mask for stm */
  { "reglist_low_st", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* reglist_hi_st: 8 bit high register mask for stm */
  { "reglist_hi_st", & HW_ENT (HW_H_UINT), 8, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* cc: condition codes */
  { "cc", & HW_ENT (HW_H_UINT), 4, 4,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* ccc: coprocessor calc */
  { "ccc", & HW_ENT (HW_H_UINT), 0, 8,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_HASH_PREFIX)|(1<<CGEN_OPERAND_UNSIGNED), { (1<<MACH_BASE) } }  },
/* nbit: negative   bit */
  { "nbit", & HW_ENT (HW_H_NBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* vbit: overflow   bit */
  { "vbit", & HW_ENT (HW_H_VBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* zbit: zero       bit */
  { "zbit", & HW_ENT (HW_H_ZBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* cbit: carry      bit */
  { "cbit", & HW_ENT (HW_H_CBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ibit: interrupt  bit */
  { "ibit", & HW_ENT (HW_H_IBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* sbit: stack      bit */
  { "sbit", & HW_ENT (HW_H_SBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* tbit: trace trap bit */
  { "tbit", & HW_ENT (HW_H_TBIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* d0bit: division 0 bit */
  { "d0bit", & HW_ENT (HW_H_D0BIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* d1bit: division 1 bit */
  { "d1bit", & HW_ENT (HW_H_D1BIT), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ccr: condition code bits */
  { "ccr", & HW_ENT (HW_H_CCR), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* scr: system condition bits */
  { "scr", & HW_ENT (HW_H_SCR), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ilm: interrupt level mask */
  { "ilm", & HW_ENT (HW_H_ILM), 0, 0,
    { CGEN_OPERAND_NBOOL_ATTRS, 0|(1<<CGEN_OPERAND_SEM_ONLY), { (1<<MACH_BASE) } }  },
};

/* Instruction formats.  */

#define F(f) & fr30_cgen_ifld_table[CONCAT2 (FR30_,f)]

static const CGEN_IFMT ifmt_empty = {
  0, 0, 0x0, { 0 }
};

static const CGEN_IFMT ifmt_add = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_RJ), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_addi = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_U4), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_add2 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_M4), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_div0s = {
  16, 16, 0xfff0, { F (F_OP1), F (F_OP2), F (F_OP3), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_div3 = {
  16, 16, 0xffff, { F (F_OP1), F (F_OP2), F (F_OP3), F (F_OP4), 0 }
};

static const CGEN_IFMT ifmt_ldi8 = {
  16, 16, 0xf000, { F (F_OP1), F (F_I8), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldi20 = {
  16, 32, 0xff00, { F (F_OP1), F (F_I20), F (F_OP2), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldi32 = {
  16, 48, 0xfff0, { F (F_OP1), F (F_I32), F (F_OP2), F (F_OP3), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldr14 = {
  16, 16, 0xf000, { F (F_OP1), F (F_DISP10), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldr14uh = {
  16, 16, 0xf000, { F (F_OP1), F (F_DISP9), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldr14ub = {
  16, 16, 0xf000, { F (F_OP1), F (F_DISP8), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldr15 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_UDISP6), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldr15dr = {
  16, 16, 0xfff0, { F (F_OP1), F (F_OP2), F (F_OP3), F (F_RS2), 0 }
};

static const CGEN_IFMT ifmt_movdr = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_RS1), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_call = {
  16, 16, 0xf800, { F (F_OP1), F (F_OP5), F (F_REL12), 0 }
};

static const CGEN_IFMT ifmt_int = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_U8), 0 }
};

static const CGEN_IFMT ifmt_brad = {
  16, 16, 0xff00, { F (F_OP1), F (F_CC), F (F_REL9), 0 }
};

static const CGEN_IFMT ifmt_dmovr13 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_DIR10), 0 }
};

static const CGEN_IFMT ifmt_dmovr13h = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_DIR9), 0 }
};

static const CGEN_IFMT ifmt_dmovr13b = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_DIR8), 0 }
};

static const CGEN_IFMT ifmt_copop = {
  16, 32, 0xfff0, { F (F_OP1), F (F_CCC), F (F_OP2), F (F_OP3), F (F_CRJ), F (F_U4C), F (F_CRI), 0 }
};

static const CGEN_IFMT ifmt_copld = {
  16, 32, 0xfff0, { F (F_OP1), F (F_CCC), F (F_OP2), F (F_OP3), F (F_RJC), F (F_U4C), F (F_CRI), 0 }
};

static const CGEN_IFMT ifmt_copst = {
  16, 32, 0xfff0, { F (F_OP1), F (F_CCC), F (F_OP2), F (F_OP3), F (F_CRJ), F (F_U4C), F (F_RIC), 0 }
};

static const CGEN_IFMT ifmt_addsp = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_S10), 0 }
};

static const CGEN_IFMT ifmt_ldm0 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_REGLIST_LOW_LD), 0 }
};

static const CGEN_IFMT ifmt_ldm1 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_REGLIST_HI_LD), 0 }
};

static const CGEN_IFMT ifmt_stm0 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_REGLIST_LOW_ST), 0 }
};

static const CGEN_IFMT ifmt_stm1 = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_REGLIST_HI_ST), 0 }
};

static const CGEN_IFMT ifmt_enter = {
  16, 16, 0xff00, { F (F_OP1), F (F_OP2), F (F_U10), 0 }
};

#undef F

#define A(a) (1 << CONCAT2 (CGEN_INSN_,a))
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.
   This is currently non-static because the simulator accesses it
   directly.  */

const CGEN_INSN fr30_cgen_insn_table_entries[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0 }, 0 },
/* add $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADD, "add", "add",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* add $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDI, "addi", "add",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* add2 $m4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADD2, "add2", "add2",
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* addc $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDC, "addc", "addc",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* addn $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDN, "addn", "addn",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* addn $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDNI, "addni", "addn",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* addn2 $m4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDN2, "addn2", "addn2",
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* sub $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_SUB, "sub", "sub",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xac00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* subc $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_SUBC, "subc", "subc",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xad00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* subn $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_SUBN, "subn", "subn",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xae00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* cmp $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CMP, "cmp", "cmp",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xaa00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* cmp $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CMPI, "cmpi", "cmp",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* cmp2 $m4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CMP2, "cmp2", "cmp2",
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* and $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_AND, "and", "and",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* or $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_OR, "or", "or",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x9200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* eor $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EOR, "eor", "eor",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x9a00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* and $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ANDM, "andm", "and",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* andh $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ANDH, "andh", "andh",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* andb $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ANDB, "andb", "andb",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* or $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ORM, "orm", "or",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* orh $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ORH, "orh", "orh",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* orb $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ORB, "orb", "orb",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eor $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EORM, "eorm", "eor",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9c00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eorh $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EORH, "eorh", "eorh",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9d00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eorb $Rj,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EORB, "eorb", "eorb",
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9e00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bandl $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BANDL, "bandl", "bandl",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* borl $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BORL, "borl", "borl",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beorl $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BEORL, "beorl", "beorl",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bandh $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BANDH, "bandh", "bandh",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* borh $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BORH, "borh", "borh",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beorh $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BEORH, "beorh", "beorh",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* btstl $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BTSTL, "btstl", "btstl",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* btsth $u4,@$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BTSTH, "btsth", "btsth",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mul $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MUL, "mul", "mul",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xaf00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mulu $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MULU, "mulu", "mulu",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xab00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mulh $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MULH, "mulh", "mulh",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xbf00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* muluh $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MULUH, "muluh", "muluh",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xbb00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* div0s $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV0S, "div0s", "div0s",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9740 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* div0u $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV0U, "div0u", "div0u",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9750 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* div1 $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV1, "div1", "div1",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9760 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* div2 $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV2, "div2", "div2",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9770 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* div3 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV3, "div3", "div3",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f60 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* div4s */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DIV4S, "div4s", "div4s",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f70 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsl $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSL, "lsl", "lsl",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xb600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsl $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSLI, "lsli", "lsl",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsl2 $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSL2, "lsl2", "lsl2",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsr $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSR, "lsr", "lsr",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xb200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsr $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSRI, "lsri", "lsr",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lsr2 $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LSR2, "lsr2", "lsr2",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* asr $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ASR, "asr", "asr",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xba00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* asr $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ASRI, "asri", "asr",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* asr2 $u4,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ASR2, "asr2", "asr2",
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldi:8 $i8,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDI8, "ldi8", "ldi:8",
    { { MNEM, ' ', OP (I8), ',', OP (RI), 0 } },
    & ifmt_ldi8, { 0xc000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldi:20 $i20,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDI20, "ldi20", "ldi:20",
    { { MNEM, ' ', OP (I20), ',', OP (RI), 0 } },
    & ifmt_ldi20, { 0x9b00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldi:32 $i32,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDI32, "ldi32", "ldi:32",
    { { MNEM, ' ', OP (I32), ',', OP (RI), 0 } },
    & ifmt_ldi32, { 0x9f80 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ld @$Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LD, "ld", "ld",
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lduh @$Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDUH, "lduh", "lduh",
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldub @$Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDUB, "ldub", "ldub",
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @($R13,$Rj),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR13, "ldr13", "ld",
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lduh @($R13,$Rj),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR13UH, "ldr13uh", "lduh",
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldub @($R13,$Rj),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR13UB, "ldr13ub", "ldub",
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @($R14,$disp10),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR14, "ldr14", "ld",
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP10), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14, { 0x2000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* lduh @($R14,$disp9),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR14UH, "ldr14uh", "lduh",
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP9), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14uh, { 0x4000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldub @($R14,$disp8),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR14UB, "ldr14ub", "ldub",
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP8), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14ub, { 0x6000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @($R15,$udisp6),$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR15, "ldr15", "ld",
    { { MNEM, ' ', '@', '(', OP (R15), ',', OP (UDISP6), ')', ',', OP (RI), 0 } },
    & ifmt_ldr15, { 0x300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR15GR, "ldr15gr", "ld",
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (RI), 0 } },
    & ifmt_div0s, { 0x700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$Rs2 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR15DR, "ldr15dr", "ld",
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (RS2), 0 } },
    & ifmt_ldr15dr, { 0x780 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$ps */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDR15PS, "ldr15ps", "ld",
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (PS), 0 } },
    & ifmt_div3, { 0x790 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* st $Ri,@$Rj */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ST, "st", "st",
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@$Rj */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STH, "sth", "sth",
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@$Rj */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STB, "stb", "stb",
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R13,$Rj) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR13, "str13", "st",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@($R13,$Rj) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR13H, "str13h", "sth",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@($R13,$Rj) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR13B, "str13b", "stb",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R14,$disp10) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR14, "str14", "st",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP10), ')', 0 } },
    & ifmt_ldr14, { 0x3000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@($R14,$disp9) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR14H, "str14h", "sth",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP9), ')', 0 } },
    & ifmt_ldr14uh, { 0x5000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@($R14,$disp8) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR14B, "str14b", "stb",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP8), ')', 0 } },
    & ifmt_ldr14ub, { 0x7000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R15,$udisp6) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR15, "str15", "st",
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R15), ',', OP (UDISP6), ')', 0 } },
    & ifmt_ldr15, { 0x1300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@-$R15 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR15GR, "str15gr", "st",
    { { MNEM, ' ', OP (RI), ',', '@', '-', OP (R15), 0 } },
    & ifmt_div0s, { 0x1700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $Rs2,@-$R15 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR15DR, "str15dr", "st",
    { { MNEM, ' ', OP (RS2), ',', '@', '-', OP (R15), 0 } },
    & ifmt_ldr15dr, { 0x1780 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* st $ps,@-$R15 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STR15PS, "str15ps", "st",
    { { MNEM, ' ', OP (PS), ',', '@', '-', OP (R15), 0 } },
    & ifmt_div3, { 0x1790 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* mov $Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MOV, "mov", "mov",
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8b00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* mov $Rs1,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MOVDR, "movdr", "mov",
    { { MNEM, ' ', OP (RS1), ',', OP (RI), 0 } },
    & ifmt_movdr, { 0xb700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* mov $ps,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MOVPS, "movps", "mov",
    { { MNEM, ' ', OP (PS), ',', OP (RI), 0 } },
    & ifmt_div0s, { 0x1710 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* mov $Ri,$Rs1 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MOV2DR, "mov2dr", "mov",
    { { MNEM, ' ', OP (RI), ',', OP (RS1), 0 } },
    & ifmt_movdr, { 0xb300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* mov $Ri,$ps */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_MOV2PS, "mov2ps", "mov",
    { { MNEM, ' ', OP (RI), ',', OP (PS), 0 } },
    & ifmt_div0s, { 0x710 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* jmp @$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_JMP, "jmp", "jmp",
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* jmp:d @$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_JMPD, "jmpd", "jmp:d",
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9f00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* call @$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CALLR, "callr", "call",
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9710 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* call:d @$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CALLRD, "callrd", "call:d",
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9f10 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* call $label12 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CALL, "call", "call",
    { { MNEM, ' ', OP (LABEL12), 0 } },
    & ifmt_call, { 0xd000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* call:d $label12 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_CALLD, "calld", "call:d",
    { { MNEM, ' ', OP (LABEL12), 0 } },
    & ifmt_call, { 0xd800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ret */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_RET, "ret", "ret",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9720 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* ret:d */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_RET_D, "ret:d", "ret:d",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f20 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* int $u8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_INT, "int", "int",
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x1f00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* inte */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_INTE, "inte", "inte",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f30 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* reti */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_RETI, "reti", "reti",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9730 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bra:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BRAD, "brad", "bra:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bra $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BRA, "bra", "bra",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* bno:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNOD, "bnod", "bno:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bno $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNO, "bno", "bno",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe100 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beq:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BEQD, "beqd", "beq:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beq $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BEQ, "beq", "beq",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe200 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bne:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNED, "bned", "bne:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bne $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNE, "bne", "bne",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bc:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BCD, "bcd", "bc:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bc $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BC, "bc", "bc",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe400 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bnc:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNCD, "bncd", "bnc:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bnc $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNC, "bnc", "bnc",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe500 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bn:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BND, "bnd", "bn:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bn $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BN, "bn", "bn",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe600 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bp:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BPD, "bpd", "bp:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bp $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BP, "bp", "bp",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bv:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BVD, "bvd", "bv:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bv $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BV, "bv", "bv",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bnv:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNVD, "bnvd", "bnv:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bnv $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BNV, "bnv", "bnv",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* blt:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLTD, "bltd", "blt:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfa00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* blt $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLT, "blt", "blt",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xea00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bge:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BGED, "bged", "bge:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfb00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bge $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BGE, "bge", "bge",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xeb00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* ble:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLED, "bled", "ble:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfc00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ble $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLE, "ble", "ble",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xec00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bgt:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BGTD, "bgtd", "bgt:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfd00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgt $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BGT, "bgt", "bgt",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xed00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bls:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLSD, "blsd", "bls:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfe00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bls $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BLS, "bls", "bls",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xee00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bhi:d $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BHID, "bhid", "bhi:d",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xff00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bhi $label9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_BHI, "bhi", "bhi",
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xef00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* dmov $R13,@$dir10 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13, "dmovr13", "dmov",
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmovh $R13,@$dir9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13H, "dmovr13h", "dmovh",
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR9), 0 } },
    & ifmt_dmovr13h, { 0x1900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmovb $R13,@$dir8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13B, "dmovr13b", "dmovb",
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR8), 0 } },
    & ifmt_dmovr13b, { 0x1a00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmov @$R13+,@$dir10 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13PI, "dmovr13pi", "dmov",
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1c00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovh @$R13+,@$dir9 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13PIH, "dmovr13pih", "dmovh",
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR9), 0 } },
    & ifmt_dmovr13h, { 0x1d00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovb @$R13+,@$dir8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR13PIB, "dmovr13pib", "dmovb",
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR8), 0 } },
    & ifmt_dmovr13b, { 0x1e00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$R15+,@$dir10 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOVR15PI, "dmovr15pi", "dmov",
    { { MNEM, ' ', '@', OP (R15), '+', ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1b00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,$R13 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13, "dmov2r13", "dmov",
    { { MNEM, ' ', '@', OP (DIR10), ',', OP (R13), 0 } },
    & ifmt_dmovr13, { 0x800 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmovh @$dir9,$R13 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13H, "dmov2r13h", "dmovh",
    { { MNEM, ' ', '@', OP (DIR9), ',', OP (R13), 0 } },
    & ifmt_dmovr13h, { 0x900 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmovb @$dir8,$R13 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13B, "dmov2r13b", "dmovb",
    { { MNEM, ' ', '@', OP (DIR8), ',', OP (R13), 0 } },
    & ifmt_dmovr13b, { 0xa00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,@$R13+ */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13PI, "dmov2r13pi", "dmov",
    { { MNEM, ' ', '@', OP (DIR10), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13, { 0xc00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovh @$dir9,@$R13+ */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13PIH, "dmov2r13pih", "dmovh",
    { { MNEM, ' ', '@', OP (DIR9), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13h, { 0xd00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovb @$dir8,@$R13+ */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R13PIB, "dmov2r13pib", "dmovb",
    { { MNEM, ' ', '@', OP (DIR8), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13b, { 0xe00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,@-$R15 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_DMOV2R15PD, "dmov2r15pd", "dmov",
    { { MNEM, ' ', '@', OP (DIR10), ',', '@', '-', OP (R15), 0 } },
    & ifmt_dmovr13, { 0xb00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldres @$Ri+,$u4 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDRES, "ldres", "ldres",
    { { MNEM, ' ', '@', OP (RI), '+', ',', OP (U4), 0 } },
    & ifmt_addi, { 0xbc00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* stres $u4,@$Ri+ */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STRES, "stres", "stres",
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), '+', 0 } },
    & ifmt_addi, { 0xbd00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* copop $u4c,$ccc,$CRj,$CRi */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_COPOP, "copop", "copop",
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (CRI), 0 } },
    & ifmt_copop, { 0x9fc0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copld $u4c,$ccc,$Rjc,$CRi */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_COPLD, "copld", "copld",
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (RJC), ',', OP (CRI), 0 } },
    & ifmt_copld, { 0x9fd0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copst $u4c,$ccc,$CRj,$Ric */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_COPST, "copst", "copst",
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (RIC), 0 } },
    & ifmt_copst, { 0x9fe0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copsv $u4c,$ccc,$CRj,$Ric */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_COPSV, "copsv", "copsv",
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (RIC), 0 } },
    & ifmt_copst, { 0x9ff0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* nop */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_NOP, "nop", "nop",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9fa0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* andccr $u8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ANDCCR, "andccr", "andccr",
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x8300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* orccr $u8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ORCCR, "orccr", "orccr",
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x9300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* stilm $u8 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STILM, "stilm", "stilm",
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x8700 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* addsp $s10 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ADDSP, "addsp", "addsp",
    { { MNEM, ' ', OP (S10), 0 } },
    & ifmt_addsp, { 0xa300 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* extsb $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EXTSB, "extsb", "extsb",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9780 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* extub $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EXTUB, "extub", "extub",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9790 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* extsh $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EXTSH, "extsh", "extsh",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x97a0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* extuh $Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_EXTUH, "extuh", "extuh",
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x97b0 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* ldm0 ($reglist_low_ld) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDM0, "ldm0", "ldm0",
    { { MNEM, ' ', '(', OP (REGLIST_LOW_LD), ')', 0 } },
    & ifmt_ldm0, { 0x8c00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldm1 ($reglist_hi_ld) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LDM1, "ldm1", "ldm1",
    { { MNEM, ' ', '(', OP (REGLIST_HI_LD), ')', 0 } },
    & ifmt_ldm1, { 0x8d00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* stm0 ($reglist_low_st) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STM0, "stm0", "stm0",
    { { MNEM, ' ', '(', OP (REGLIST_LOW_ST), ')', 0 } },
    & ifmt_stm0, { 0x8e00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* stm1 ($reglist_hi_st) */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_STM1, "stm1", "stm1",
    { { MNEM, ' ', '(', OP (REGLIST_HI_ST), ')', 0 } },
    & ifmt_stm1, { 0x8f00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* enter $u10 */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_ENTER, "enter", "enter",
    { { MNEM, ' ', OP (U10), 0 } },
    & ifmt_enter, { 0xf00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* leave */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_LEAVE, "leave", "leave",
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f90 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0, { (1<<MACH_BASE) } }
  },
/* xchb @$Rj,$Ri */
  {
    { 1, 1, 1, 1 },
    FR30_INSN_XCHB, "xchb", "xchb",
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8a00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
};

#undef A
#undef MNEM
#undef OP

static const CGEN_INSN_TABLE insn_table =
{
  & fr30_cgen_insn_table_entries[0],
  sizeof (CGEN_INSN),
  MAX_INSNS,
  NULL
};

/* Formats for ALIAS macro-insns.  */

#define F(f) & fr30_cgen_ifld_table[CONCAT2 (FR30_,f)]

static const CGEN_IFMT ifmt_ldi8m = {
  16, 16, 0xf000, { F (F_OP1), F (F_I8), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldi20m = {
  16, 32, 0xff00, { F (F_OP1), F (F_I20), F (F_OP2), F (F_RI), 0 }
};

static const CGEN_IFMT ifmt_ldi32m = {
  16, 48, 0xfff0, { F (F_OP1), F (F_I32), F (F_OP2), F (F_OP3), F (F_RI), 0 }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#define A(a) (1 << CONCAT2 (CGEN_INSN_,a))
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_INSN macro_insn_table_entries[] =
{
/* ldi8 $i8,$Ri */
  {
    { 1, 1, 1, 1 },
    -1, "ldi8m", "ldi8",
    { { MNEM, ' ', OP (I8), ',', OP (RI), 0 } },
    & ifmt_ldi8m, { 0xc000 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* ldi20 $i20,$Ri */
  {
    { 1, 1, 1, 1 },
    -1, "ldi20m", "ldi20",
    { { MNEM, ' ', OP (I20), ',', OP (RI), 0 } },
    & ifmt_ldi20m, { 0x9b00 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* ldi32 $i32,$Ri */
  {
    { 1, 1, 1, 1 },
    -1, "ldi32m", "ldi32",
    { { MNEM, ' ', OP (I32), ',', OP (RI), 0 } },
    & ifmt_ldi32m, { 0x9f80 },
    (PTR) 0,
    { CGEN_INSN_NBOOL_ATTRS, 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
};

#undef A
#undef MNEM
#undef OP

static const CGEN_INSN_TABLE macro_insn_table =
{
  & macro_insn_table_entries[0],
  sizeof (CGEN_INSN),
  (sizeof (macro_insn_table_entries) /
   sizeof (macro_insn_table_entries[0])),
  NULL
};

static void
init_tables ()
{
}

/* Return non-zero if INSN is to be added to the hash table.
   Targets are free to override CGEN_{ASM,DIS}_HASH_P in the .opc file.  */

static int
asm_hash_insn_p (insn)
     const CGEN_INSN * insn;
{
  return CGEN_ASM_HASH_P (insn);
}

static int
dis_hash_insn_p (insn)
     const CGEN_INSN * insn;
{
  /* If building the hash table and the NO-DIS attribute is present,
     ignore.  */
  if (CGEN_INSN_ATTR (insn, CGEN_INSN_NO_DIS))
    return 0;
  return CGEN_DIS_HASH_P (insn);
}

/* The result is the hash value of the insn.
   Targets are free to override CGEN_{ASM,DIS}_HASH in the .opc file.  */

static unsigned int
asm_hash_insn (mnem)
     const char * mnem;
{
  return CGEN_ASM_HASH (mnem);
}

/* BUF is a pointer to the insn's bytes in target order.
   VALUE is an integer of the first CGEN_BASE_INSN_BITSIZE bits,
   host order.  */

static unsigned int
dis_hash_insn (buf, value)
     const char * buf;
     CGEN_INSN_INT value;
{
  return CGEN_DIS_HASH (buf, value);
}

/* Initialize an opcode table and return a descriptor.
   It's much like opening a file, and must be the first function called.  */

CGEN_OPCODE_DESC
fr30_cgen_opcode_open (mach, endian)
     int mach;
     enum cgen_endian endian;
{
  CGEN_OPCODE_TABLE * table = (CGEN_OPCODE_TABLE *) xmalloc (sizeof (CGEN_OPCODE_TABLE));
  static int init_p;

  if (! init_p)
    {
      init_tables ();
      init_p = 1;
    }

  memset (table, 0, sizeof (*table));

  CGEN_OPCODE_MACH (table) = mach;
  CGEN_OPCODE_ENDIAN (table) = endian;
  /* FIXME: for the sparc case we can determine insn-endianness statically.
     The worry here is where both data and insn endian can be independently
     chosen, in which case this function will need another argument.
     Actually, will want to allow for more arguments in the future anyway.  */
  CGEN_OPCODE_INSN_ENDIAN (table) = endian;

  CGEN_OPCODE_HW_LIST (table) = & fr30_cgen_hw_entries[0];

  CGEN_OPCODE_IFLD_TABLE (table) = & fr30_cgen_ifld_table[0];

  CGEN_OPCODE_OPERAND_TABLE (table) = & fr30_cgen_operand_table[0];

  * CGEN_OPCODE_INSN_TABLE (table) = insn_table;

  * CGEN_OPCODE_MACRO_INSN_TABLE (table) = macro_insn_table;

  CGEN_OPCODE_ASM_HASH_P (table) = asm_hash_insn_p;
  CGEN_OPCODE_ASM_HASH (table) = asm_hash_insn;
  CGEN_OPCODE_ASM_HASH_SIZE (table) = CGEN_ASM_HASH_SIZE;

  CGEN_OPCODE_DIS_HASH_P (table) = dis_hash_insn_p;
  CGEN_OPCODE_DIS_HASH (table) = dis_hash_insn;
  CGEN_OPCODE_DIS_HASH_SIZE (table) = CGEN_DIS_HASH_SIZE;

  return (CGEN_OPCODE_DESC) table;
}

/* Close an opcode table.  */

void
fr30_cgen_opcode_close (desc)
     CGEN_OPCODE_DESC desc;
{
  free (desc);
}

/* Getting values from cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they return.
   TODO: floating point, inlining support, remove cases where result type
   not appropriate.  */

int
fr30_cgen_get_int_operand (opindex, fields)
     int opindex;
     const CGEN_FIELDS * fields;
{
  int value;

  switch (opindex)
    {
    case FR30_OPERAND_RI :
      value = fields->f_Ri;
      break;
    case FR30_OPERAND_RJ :
      value = fields->f_Rj;
      break;
    case FR30_OPERAND_RIC :
      value = fields->f_Ric;
      break;
    case FR30_OPERAND_RJC :
      value = fields->f_Rjc;
      break;
    case FR30_OPERAND_CRI :
      value = fields->f_CRi;
      break;
    case FR30_OPERAND_CRJ :
      value = fields->f_CRj;
      break;
    case FR30_OPERAND_RS1 :
      value = fields->f_Rs1;
      break;
    case FR30_OPERAND_RS2 :
      value = fields->f_Rs2;
      break;
    case FR30_OPERAND_R13 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_R14 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_R15 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_PS :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_U4 :
      value = fields->f_u4;
      break;
    case FR30_OPERAND_U4C :
      value = fields->f_u4c;
      break;
    case FR30_OPERAND_U8 :
      value = fields->f_u8;
      break;
    case FR30_OPERAND_I8 :
      value = fields->f_i8;
      break;
    case FR30_OPERAND_UDISP6 :
      value = fields->f_udisp6;
      break;
    case FR30_OPERAND_DISP8 :
      value = fields->f_disp8;
      break;
    case FR30_OPERAND_DISP9 :
      value = fields->f_disp9;
      break;
    case FR30_OPERAND_DISP10 :
      value = fields->f_disp10;
      break;
    case FR30_OPERAND_S10 :
      value = fields->f_s10;
      break;
    case FR30_OPERAND_U10 :
      value = fields->f_u10;
      break;
    case FR30_OPERAND_I32 :
      value = fields->f_i32;
      break;
    case FR30_OPERAND_M4 :
      value = fields->f_m4;
      break;
    case FR30_OPERAND_I20 :
      value = fields->f_i20;
      break;
    case FR30_OPERAND_DIR8 :
      value = fields->f_dir8;
      break;
    case FR30_OPERAND_DIR9 :
      value = fields->f_dir9;
      break;
    case FR30_OPERAND_DIR10 :
      value = fields->f_dir10;
      break;
    case FR30_OPERAND_LABEL9 :
      value = fields->f_rel9;
      break;
    case FR30_OPERAND_LABEL12 :
      value = fields->f_rel12;
      break;
    case FR30_OPERAND_REGLIST_LOW_LD :
      value = fields->f_reglist_low_ld;
      break;
    case FR30_OPERAND_REGLIST_HI_LD :
      value = fields->f_reglist_hi_ld;
      break;
    case FR30_OPERAND_REGLIST_LOW_ST :
      value = fields->f_reglist_low_st;
      break;
    case FR30_OPERAND_REGLIST_HI_ST :
      value = fields->f_reglist_hi_st;
      break;
    case FR30_OPERAND_CC :
      value = fields->f_cc;
      break;
    case FR30_OPERAND_CCC :
      value = fields->f_ccc;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting int operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

bfd_vma
fr30_cgen_get_vma_operand (opindex, fields)
     int opindex;
     const CGEN_FIELDS * fields;
{
  bfd_vma value;

  switch (opindex)
    {
    case FR30_OPERAND_RI :
      value = fields->f_Ri;
      break;
    case FR30_OPERAND_RJ :
      value = fields->f_Rj;
      break;
    case FR30_OPERAND_RIC :
      value = fields->f_Ric;
      break;
    case FR30_OPERAND_RJC :
      value = fields->f_Rjc;
      break;
    case FR30_OPERAND_CRI :
      value = fields->f_CRi;
      break;
    case FR30_OPERAND_CRJ :
      value = fields->f_CRj;
      break;
    case FR30_OPERAND_RS1 :
      value = fields->f_Rs1;
      break;
    case FR30_OPERAND_RS2 :
      value = fields->f_Rs2;
      break;
    case FR30_OPERAND_R13 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_R14 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_R15 :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_PS :
      value = fields->f_nil;
      break;
    case FR30_OPERAND_U4 :
      value = fields->f_u4;
      break;
    case FR30_OPERAND_U4C :
      value = fields->f_u4c;
      break;
    case FR30_OPERAND_U8 :
      value = fields->f_u8;
      break;
    case FR30_OPERAND_I8 :
      value = fields->f_i8;
      break;
    case FR30_OPERAND_UDISP6 :
      value = fields->f_udisp6;
      break;
    case FR30_OPERAND_DISP8 :
      value = fields->f_disp8;
      break;
    case FR30_OPERAND_DISP9 :
      value = fields->f_disp9;
      break;
    case FR30_OPERAND_DISP10 :
      value = fields->f_disp10;
      break;
    case FR30_OPERAND_S10 :
      value = fields->f_s10;
      break;
    case FR30_OPERAND_U10 :
      value = fields->f_u10;
      break;
    case FR30_OPERAND_I32 :
      value = fields->f_i32;
      break;
    case FR30_OPERAND_M4 :
      value = fields->f_m4;
      break;
    case FR30_OPERAND_I20 :
      value = fields->f_i20;
      break;
    case FR30_OPERAND_DIR8 :
      value = fields->f_dir8;
      break;
    case FR30_OPERAND_DIR9 :
      value = fields->f_dir9;
      break;
    case FR30_OPERAND_DIR10 :
      value = fields->f_dir10;
      break;
    case FR30_OPERAND_LABEL9 :
      value = fields->f_rel9;
      break;
    case FR30_OPERAND_LABEL12 :
      value = fields->f_rel12;
      break;
    case FR30_OPERAND_REGLIST_LOW_LD :
      value = fields->f_reglist_low_ld;
      break;
    case FR30_OPERAND_REGLIST_HI_LD :
      value = fields->f_reglist_hi_ld;
      break;
    case FR30_OPERAND_REGLIST_LOW_ST :
      value = fields->f_reglist_low_st;
      break;
    case FR30_OPERAND_REGLIST_HI_ST :
      value = fields->f_reglist_hi_st;
      break;
    case FR30_OPERAND_CC :
      value = fields->f_cc;
      break;
    case FR30_OPERAND_CCC :
      value = fields->f_ccc;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting vma operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

/* Stuffing values in cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they accept.
   TODO: floating point, inlining support, remove cases where argument type
   not appropriate.  */

void
fr30_cgen_set_int_operand (opindex, fields, value)
     int opindex;
     CGEN_FIELDS * fields;
     int value;
{
  switch (opindex)
    {
    case FR30_OPERAND_RI :
      fields->f_Ri = value;
      break;
    case FR30_OPERAND_RJ :
      fields->f_Rj = value;
      break;
    case FR30_OPERAND_RIC :
      fields->f_Ric = value;
      break;
    case FR30_OPERAND_RJC :
      fields->f_Rjc = value;
      break;
    case FR30_OPERAND_CRI :
      fields->f_CRi = value;
      break;
    case FR30_OPERAND_CRJ :
      fields->f_CRj = value;
      break;
    case FR30_OPERAND_RS1 :
      fields->f_Rs1 = value;
      break;
    case FR30_OPERAND_RS2 :
      fields->f_Rs2 = value;
      break;
    case FR30_OPERAND_R13 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_R14 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_R15 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_PS :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_U4 :
      fields->f_u4 = value;
      break;
    case FR30_OPERAND_U4C :
      fields->f_u4c = value;
      break;
    case FR30_OPERAND_U8 :
      fields->f_u8 = value;
      break;
    case FR30_OPERAND_I8 :
      fields->f_i8 = value;
      break;
    case FR30_OPERAND_UDISP6 :
      fields->f_udisp6 = value;
      break;
    case FR30_OPERAND_DISP8 :
      fields->f_disp8 = value;
      break;
    case FR30_OPERAND_DISP9 :
      fields->f_disp9 = value;
      break;
    case FR30_OPERAND_DISP10 :
      fields->f_disp10 = value;
      break;
    case FR30_OPERAND_S10 :
      fields->f_s10 = value;
      break;
    case FR30_OPERAND_U10 :
      fields->f_u10 = value;
      break;
    case FR30_OPERAND_I32 :
      fields->f_i32 = value;
      break;
    case FR30_OPERAND_M4 :
      fields->f_m4 = value;
      break;
    case FR30_OPERAND_I20 :
      fields->f_i20 = value;
      break;
    case FR30_OPERAND_DIR8 :
      fields->f_dir8 = value;
      break;
    case FR30_OPERAND_DIR9 :
      fields->f_dir9 = value;
      break;
    case FR30_OPERAND_DIR10 :
      fields->f_dir10 = value;
      break;
    case FR30_OPERAND_LABEL9 :
      fields->f_rel9 = value;
      break;
    case FR30_OPERAND_LABEL12 :
      fields->f_rel12 = value;
      break;
    case FR30_OPERAND_REGLIST_LOW_LD :
      fields->f_reglist_low_ld = value;
      break;
    case FR30_OPERAND_REGLIST_HI_LD :
      fields->f_reglist_hi_ld = value;
      break;
    case FR30_OPERAND_REGLIST_LOW_ST :
      fields->f_reglist_low_st = value;
      break;
    case FR30_OPERAND_REGLIST_HI_ST :
      fields->f_reglist_hi_st = value;
      break;
    case FR30_OPERAND_CC :
      fields->f_cc = value;
      break;
    case FR30_OPERAND_CCC :
      fields->f_ccc = value;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting int operand.\n"),
		       opindex);
      abort ();
  }
}

void
fr30_cgen_set_vma_operand (opindex, fields, value)
     int opindex;
     CGEN_FIELDS * fields;
     bfd_vma value;
{
  switch (opindex)
    {
    case FR30_OPERAND_RI :
      fields->f_Ri = value;
      break;
    case FR30_OPERAND_RJ :
      fields->f_Rj = value;
      break;
    case FR30_OPERAND_RIC :
      fields->f_Ric = value;
      break;
    case FR30_OPERAND_RJC :
      fields->f_Rjc = value;
      break;
    case FR30_OPERAND_CRI :
      fields->f_CRi = value;
      break;
    case FR30_OPERAND_CRJ :
      fields->f_CRj = value;
      break;
    case FR30_OPERAND_RS1 :
      fields->f_Rs1 = value;
      break;
    case FR30_OPERAND_RS2 :
      fields->f_Rs2 = value;
      break;
    case FR30_OPERAND_R13 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_R14 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_R15 :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_PS :
      fields->f_nil = value;
      break;
    case FR30_OPERAND_U4 :
      fields->f_u4 = value;
      break;
    case FR30_OPERAND_U4C :
      fields->f_u4c = value;
      break;
    case FR30_OPERAND_U8 :
      fields->f_u8 = value;
      break;
    case FR30_OPERAND_I8 :
      fields->f_i8 = value;
      break;
    case FR30_OPERAND_UDISP6 :
      fields->f_udisp6 = value;
      break;
    case FR30_OPERAND_DISP8 :
      fields->f_disp8 = value;
      break;
    case FR30_OPERAND_DISP9 :
      fields->f_disp9 = value;
      break;
    case FR30_OPERAND_DISP10 :
      fields->f_disp10 = value;
      break;
    case FR30_OPERAND_S10 :
      fields->f_s10 = value;
      break;
    case FR30_OPERAND_U10 :
      fields->f_u10 = value;
      break;
    case FR30_OPERAND_I32 :
      fields->f_i32 = value;
      break;
    case FR30_OPERAND_M4 :
      fields->f_m4 = value;
      break;
    case FR30_OPERAND_I20 :
      fields->f_i20 = value;
      break;
    case FR30_OPERAND_DIR8 :
      fields->f_dir8 = value;
      break;
    case FR30_OPERAND_DIR9 :
      fields->f_dir9 = value;
      break;
    case FR30_OPERAND_DIR10 :
      fields->f_dir10 = value;
      break;
    case FR30_OPERAND_LABEL9 :
      fields->f_rel9 = value;
      break;
    case FR30_OPERAND_LABEL12 :
      fields->f_rel12 = value;
      break;
    case FR30_OPERAND_REGLIST_LOW_LD :
      fields->f_reglist_low_ld = value;
      break;
    case FR30_OPERAND_REGLIST_HI_LD :
      fields->f_reglist_hi_ld = value;
      break;
    case FR30_OPERAND_REGLIST_LOW_ST :
      fields->f_reglist_low_st = value;
      break;
    case FR30_OPERAND_REGLIST_HI_ST :
      fields->f_reglist_hi_st = value;
      break;
    case FR30_OPERAND_CC :
      fields->f_cc = value;
      break;
    case FR30_OPERAND_CCC :
      fields->f_ccc = value;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting vma operand.\n"),
		       opindex);
      abort ();
  }
}

