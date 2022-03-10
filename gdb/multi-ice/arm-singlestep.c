/* Single-stepping code for the Acorn Risc Machine (ARM)
   for the Multi-ICE gdb server.
   Copyright (C) 1999
   Free Software Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "symfile.h"
#include "gdb_string.h"
#include "coff/internal.h"	/* Internal format of COFF symbols in BFD */

/* These from server.h.  I am not including that because this file
   is still hacked bleeding out of gdb, and I don't want to deal with
   the include file conflicts (for instance registers) yet...
*/

extern void output (char *format, ...);
extern void output_error (char *format, ...);
extern LONGEST extract_signed_integer (PTR addr, int len);
extern int low_read_memory_raw (CORE_ADDR start, void *buffer,
				unsigned int *nbytes);
extern unsigned int restore_register (int regno);

/* The routines in this file are taken from arm-tdep.c.  I didn't want to
   use that file, because there was just too much other stuff it would
   drag in that was not necessary in the server.
*/

/* Set to true if the 32-bit mode is in use. */

int arm_apcs_32 = 1;

/* Tell if the program counter value in MEMADDR is in a Thumb function.  */

/* Thumb function addresses are odd (bit 0 is set).  Here are some
   macros to test, set, or clear bit 0 of addresses.  */
#define IS_THUMB_ADDR(addr)	((addr) & 1)
#define MAKE_THUMB_ADDR(addr)	((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

/* This is the main functions in this file - used to single step...
  Despite its name it works for both arm & thumb... */
CORE_ADDR server_arm_get_next_pc (CORE_ADDR pc, unsigned short *is_thumb);

/* Declarations for functions used only in this file */

static CORE_ADDR thumb_get_next_pc (CORE_ADDR pc, unsigned short *is_thumb);
static int condition_true (unsigned long cond, unsigned long status_reg);
int arm_pc_is_thumb (bfd_vma memaddr);
CORE_ADDR arm_addr_bits_remove (CORE_ADDR val);
static unsigned long shifted_reg_val (unsigned long inst, int carry,
		 unsigned long pc_val, unsigned long status_reg);
static int bitcount (unsigned long val);
unsigned long read_register (int regno);


static int
condition_true (unsigned long cond, unsigned long status_reg)
{
  if (cond == INST_AL || cond == INST_NV)
    return 1;

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) != 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) == 0);
    case INST_CS:
      return ((status_reg & FLAG_C) != 0);
    case INST_CC:
      return ((status_reg & FLAG_C) == 0);
    case INST_MI:
      return ((status_reg & FLAG_N) != 0);
    case INST_PL:
      return ((status_reg & FLAG_N) == 0);
    case INST_VS:
      return ((status_reg & FLAG_V) != 0);
    case INST_VC:
      return ((status_reg & FLAG_V) == 0);
    case INST_HI:
      return ((status_reg & (FLAG_C | FLAG_Z)) == FLAG_C);
    case INST_LS:
      return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
    case INST_GE:
      return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return (((status_reg & FLAG_Z) == 0) &&
	      (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return (((status_reg & FLAG_Z) != 0) ||
	      (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
    }
  return 1;
}

/*
 * This is a restricted version of the function that is in
 * arm-tdep.c.  That version relies on the minimal symbols to
 * really get this right, since, as Zedenek pointed out the IS_THUMB_ADDR
 * macro is bogus.  However, we don't have the minimal symbols.  So
 * we will use the CPSR register instead.  This means this function will
 * ONLY work when the pb passed in is the ACTUAL PC...
 *
 * Since we only use this for predicting branches for stepping,
 * that should be okay.
 */

int
arm_pc_is_thumb (bfd_vma memaddr)
{
  unsigned int cpsr, bit_5;
  
  cpsr = read_register (PS_REGNUM);
  bit_5 = ((cpsr & 0x20) == 0x20);
 
  return bit_5;
  
}

CORE_ADDR
arm_addr_bits_remove (CORE_ADDR val)
{
  if (arm_pc_is_thumb (val))
    return (val & (arm_apcs_32 ? 0xfffffffe : 0x03fffffe));
  else
    return (val & (arm_apcs_32 ? 0xfffffffc : 0x03fffffc));
}

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (unsigned long inst, int carry,
		 unsigned long pc_val, unsigned long status_reg)
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);
 
  if (bit(inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);
 
  res = (rm == 15 
	 ? ((pc_val | (ARM_PC_32 ? 0 : status_reg))
	    + (bit (inst, 4) ? 12 : 8)) 
	 : read_register (rm));

  switch (shifttype)
    {
    case 0: /* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;
      
    case 1: /* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2: /* ASR */
      if (shift >= 32) shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3: /* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32-shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (unsigned long val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;	/* delete rightmost 1-bit in val */
  return nbits;
}

static CORE_ADDR
thumb_get_next_pc (CORE_ADDR pc, unsigned short *is_thumb)
{
  unsigned long pc_val = ((unsigned long)pc) + 4;	/* PC after prefetch */
  unsigned short inst1 = read_memory_integer (pc, 2);
  CORE_ADDR nextpc = pc + 2;		/* default is next instruction */
  unsigned long offset;

  *is_thumb = 1;

  if ((inst1 & 0xff00) == 0xbd00)	/* pop {rlist, pc} */
    {
      CORE_ADDR sp;

      /* Fetch the saved PC from the stack.  It's stored above
         all of the other registers.  */
      offset = bitcount (bits (inst1, 0, 7)) * REGISTER_SIZE;
      sp = read_register (SP_REGNUM);
      nextpc = (CORE_ADDR) read_memory_integer (sp + offset, 4);
      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	{
	  output_error ("Infinite loop detected\n");
	  return 0;
	}
    }
  else if ((inst1 & 0xf000) == 0xd000)	/* conditional branch */
    {
      unsigned long status = read_register (PS_REGNUM);
      unsigned long cond = bits (inst1, 8, 11); 
      if (cond != 0x0f && condition_true (cond, status))	/* 0x0f = SWI */
	nextpc = pc_val + (sbits (inst1, 0, 7) << 1);
    }
  else if ((inst1 & 0xf800) == 0xe000)	/* unconditional branch */
    {
      nextpc = pc_val + (sbits (inst1, 0, 10) << 1);
    }
  else if ((inst1 & 0xf800) == 0xf000)	/* long branch with link */
    {
      unsigned short inst2 = read_memory_integer (pc + 2, 2);
      offset = (sbits (inst1, 0, 10) << 12) + (bits  (inst2, 0, 10) << 1);
      nextpc = pc_val + offset;
    }
  else if ((inst1 & 0xff80) == 0x4600) /* Branch & Exchange IS */
    {
      unsigned short pc_register;

      pc_register = (inst1 & 0x0038) >> 3;
      nextpc = read_register (pc_register);
      *is_thumb = (nextpc & 0x1);
      nextpc <<= 1;
    } 
      
  return nextpc;
}

CORE_ADDR
server_arm_get_next_pc (CORE_ADDR pc, unsigned short *is_thumb)
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  CORE_ADDR nextpc;

  if (arm_pc_is_thumb (pc))
    return thumb_get_next_pc (pc, is_thumb);

  *is_thumb = 0;
  pc_val = (unsigned long) pc;
  this_instr = read_memory_integer (pc, 4);
  status = read_register (PS_REGNUM);
  nextpc = (CORE_ADDR) (pc_val + 4);  /* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
    {
      /* First test for the "BX" instruction, since it looks like a data
	 processing instruction if you only look at bits 24-27 */

      if (bits (this_instr, 20, 27) == 0x12) /* BX Instruction */
	{	  
	  unsigned short pc_register;
	  
	  pc_register = this_instr & 0x008;
	  nextpc = read_register (pc_register);
	  *is_thumb = (nextpc & 0x1);
	  nextpc <<= 1;
	  nextpc = ADDR_BITS_REMOVE (nextpc);	  
	}
      else
	{
	  switch (bits (this_instr, 24, 27))
	    {
	    case 0x0: case 0x1: /* data processing */
	    case 0x2: case 0x3:
	      {
		unsigned long operand1, operand2, result = 0;
		unsigned long rn;
		int c;
		
		if (bits (this_instr, 12, 15) != 15)
		  break;
		
		if (bits (this_instr, 22, 25) == 0
		    && bits (this_instr, 4, 7) == 9)  /* multiply */
		  {
		    output_error ("Illegal update to pc in instruction\n");
		    return 0;
		  }
		
		/* Multiply into PC */
		c = (status & FLAG_C) ? 1 : 0;
		rn = bits (this_instr, 16, 19);
		operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);
		
		if (bit (this_instr, 25))
		  {
		    unsigned long immval = bits (this_instr, 0, 7);
		    unsigned long rotate = 2 * bits (this_instr, 8, 11);
		    operand2 = ((immval >> rotate) | (immval << (32-rotate)))
		      & 0xffffffff;
		  }
		else  /* operand 2 is a shifted register */
		  operand2 = shifted_reg_val (this_instr, c, pc_val, status);
		
		switch (bits (this_instr, 21, 24))
		  {
		  case 0x0: /*and*/
		    result = operand1 & operand2;
		    break;
		    
		  case 0x1: /*eor*/
		    result = operand1 ^ operand2;
		    break;
		    
		  case 0x2: /*sub*/
		    result = operand1 - operand2;
		    break;
		    
		  case 0x3: /*rsb*/
		    result = operand2 - operand1;
		    break;
		    
		  case 0x4:  /*add*/
		    result = operand1 + operand2;
		    break;
		    
		  case 0x5: /*adc*/
		    result = operand1 + operand2 + c;
		    break;
		    
		  case 0x6: /*sbc*/
		    result = operand1 - operand2 + c;
		    break;
		    
		  case 0x7: /*rsc*/
		    result = operand2 - operand1 + c;
		    break;
		    
		  case 0x8: case 0x9: case 0xa: case 0xb: /* tst, teq, cmp, cmn */
		    result = (unsigned long) nextpc;
		    break;
		    
		  case 0xc: /*orr*/
		    result = operand1 | operand2;
		    break;
		    
		  case 0xd: /*mov*/
		    /* Always step into a function.  */
		    result = operand2;
		    break;
		    
		  case 0xe: /*bic*/
		    result = operand1 & ~operand2;
		    break;
		    
		  case 0xf: /*mvn*/
		    result = ~operand2;
		    break;
		  }
		nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);
		
		if (nextpc == pc)
		  {
		    output_error ("Infinite loop detected\n");
		    return 0;
		  }
		break;
	      }
	      
	    case 0x4: case 0x5: /* data transfer */
	    case 0x6: case 0x7:
	      if (bit (this_instr, 20))
		{
		  /* load */
		  if (bits (this_instr, 12, 15) == 15)
		    {
		      /* rd == pc */
		      unsigned long  rn;
		      unsigned long base;
		      
		      if (bit (this_instr, 22))
			{
			  output_error ("Illegal update to pc in instruction\n");
			  return 0;
			}
		      
		      /* byte write to PC */
		      rn = bits (this_instr, 16, 19);
		      base = (rn == 15) ? pc_val + 8 : read_register (rn);
		      if (bit (this_instr, 24))
			{
			  /* pre-indexed */
			  int c = (status & FLAG_C) ? 1 : 0;
			  /* In the original code in arm-tdep.c this didn't have 
			     the status argument, it only had 3 arguments! */
			  unsigned long offset =
			    (bit (this_instr, 25)
			     ? shifted_reg_val (this_instr, c, pc_val, status)
			     : bits (this_instr, 0, 11));
			  
			  if (bit (this_instr, 23))
			    base += offset;
			  else
			    base -= offset;
			}
		      nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base, 
								4);
		      
		      nextpc = ADDR_BITS_REMOVE (nextpc);
		      
		      if (nextpc == pc)
			{
			  output_error ("Infinite loop detected\n");
			  return 0;
			}
		    }
		}
	      break;
	      
	    case 0x8: case 0x9: /* block transfer */
	      if (bit (this_instr, 20))
		{
		  /* LDM */
		  if (bit (this_instr, 15))
		    {
		      /* loading pc */
		      int offset = 0;
		      
		      if (bit (this_instr, 23))
			{
			  /* up */
			  unsigned long reglist = bits (this_instr, 0, 14);
			  offset = bitcount (reglist) * 4;
			  if (bit (this_instr, 24)) /* pre */
			    offset += 4;
			}
		      else if (bit (this_instr, 24))
			offset = -4;
 
		      {
			unsigned long rn_val = 
			  read_register (bits (this_instr, 16, 19));
			nextpc =
			  (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
									+ offset),
							   4);
		      }
		      nextpc = ADDR_BITS_REMOVE (nextpc);
		      if (nextpc == pc)
			{
			  output_error ("Infinite loop detected\n");
			  return 0;
			}
		    }
		}
	      break;
 
	    case 0xb:           /* branch & link */
	    case 0xa:           /* branch */
	      {
		nextpc = BranchDest (pc, this_instr);

		nextpc = ADDR_BITS_REMOVE (nextpc);
		if (nextpc == pc)
		  {
		    output_error ("Infinite loop detected");
		    return 0;
		  }
		break;
	      }
 
	    case 0xc: case 0xd:
	    case 0xe:           /* coproc ops */
	    case 0xf:           /* SWI */
	      break;

	    default:
	      output_error ("Bad bit-field extraction\n");
	      return (pc);
	    }
	}
    }

  return nextpc;
}

/* These wrappers are just to reduce diffs... */

unsigned long
read_register (int regno)
{
    return restore_register (regno);  
}

LONGEST
read_memory_integer (CORE_ADDR memaddr, int len)
{
  LONGEST rawmem = 0;
  if (len > sizeof (rawmem))
    {
      output_error ("Asked for an integer longer that LONGEST!");
      return 0;
    }
	
  low_read_memory_raw (memaddr, &rawmem, &len);
  return rawmem;
}
