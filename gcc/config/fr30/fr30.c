/* CYGNUS LOCAL -- nickc/fr30 */

/*{{{  Introduction */ 

/* FR30 specific functions.
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/*}}}*/
/*{{{  Includes */ 

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h> /* so that MIn and MAX are defined before machmode.h */
#include "config.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "expr.h"
#include "obstack.h"
#include "tree.h"
#include "except.h"
#include "function.h"

/*}}}*/
/*{{{  Function Prologues & Epilogues */ 

/* Define the information needed to generate branch and scc insns.  This is
   stored from the compare operation.  */

struct rtx_def * fr30_compare_op0;
struct rtx_def * fr30_compare_op1;

/* The FR30 stack looks like this:

             Before call                       After call
   FP ->|                       |       |                       |
        +-----------------------+       +-----------------------+       high 
        |                       |       |                       |       memory
        |  local variables,     |       |  local variables,     |
        |  reg save area, etc.  |       |  reg save area, etc.  |
        |                       |       |                       |
        +-----------------------+       +-----------------------+
        |                       |       |                       |
        | args to the func that |       |  args to this func.   |
        | is being called that  |       |                       |
   SP ->| do not fit in regs    |       |                       |
        +-----------------------+       +-----------------------+
                                        |  args that used to be |  \
                                        | in regs; only created |   |  pretend_size 
                                   AP-> |   for vararg funcs    |  /  
                                        +-----------------------+    
                                        |                       |  \  
                                        |  register save area   |   |
                                        |                       |   |
					+-----------------------+   |  reg_size
                                        |    return address     |   | 
					+-----------------------+   |
                                   FP ->|   previous frame ptr  |  /
                                        +-----------------------+    
                                        |                       |  \   
                                        |  local variables      |   |  var_size 
                                        |                       |  /  
                                        +-----------------------+    
                                        |                       |  \       
     low                                |  room for args to     |   |
     memory                             |  other funcs called   |   |  args_size     
                                        |  from this one        |   |
                                   SP ->|                       |  /  
                                        +-----------------------+    
   
   Note, AP is a fake hard register.  It will be eliminated in favour of
   SP or FP as appropriate.

   Note, Some or all of the stack sections above may be omitted if they 
   are not needed.  */

/* Structure to be filled in by fr30_compute_frame_size() with register
   save masks, and offsets for the current function.  */
struct fr30_frame_info
{
  unsigned int total_size;	/* # Bytes that the entire frame takes up. */
  unsigned int pretend_size;	/* # Bytes we push and pretend caller did. */
  unsigned int args_size;	/* # Bytes that outgoing arguments take up. */
  unsigned int reg_size;	/* # Bytes needed to store regs. */
  unsigned int var_size;	/* # Bytes that variables take up. */
  unsigned int frame_size;      /* # Bytes in current frame.  */
  unsigned int gmask;		/* Mask of saved registers. */
  unsigned int save_fp;		/* Nonzero if frame pointer must be saved. */
  unsigned int save_rp;		/* Nonzero if return popinter must be saved. */
  int          initialised;	/* Nonzero if frame size already calculated. */
};

/* Current frame information calculated by fr30_compute_frame_size().  */
static struct fr30_frame_info 	current_frame_info;

/* Zero structure to initialize current_frame_info.  */
static struct fr30_frame_info 	zero_frame_info;

#define FRAME_POINTER_MASK 	(1 << (FRAME_POINTER_REGNUM))
#define RETURN_POINTER_MASK 	(1 << (RETURN_POINTER_REGNUM))

/* Tell prologue and epilogue if register REGNO should be saved / restored.
   The return address and frame pointer are treated separately.
   Don't consider them here.  */
#define MUST_SAVE_REGISTER(regno)      \
  (   (regno) != RETURN_POINTER_REGNUM \
   && (regno) != FRAME_POINTER_REGNUM  \
   &&   regs_ever_live [regno]         \
   && ! call_used_regs [regno]         )

#define MUST_SAVE_FRAME_POINTER	 (regs_ever_live [FRAME_POINTER_REGNUM]  || frame_pointer_needed)
#define MUST_SAVE_RETURN_POINTER (regs_ever_live [RETURN_POINTER_REGNUM] || profile_flag)

#if UNITS_PER_WORD == 4
#define WORD_ALIGN(SIZE) (((SIZE) + 3) & ~3)
#endif
     
/* Returns the number of bytes offset between FROM_REG and TO_REG
   for the current function.  As a side effect it fills in the 
   current_frame_info structure, if the data is available.  */
unsigned int
fr30_compute_frame_size (from_reg, to_reg)
     int from_reg;
     int to_reg;
{
  int 		regno;
  unsigned int 	return_value;
  unsigned int	var_size;
  unsigned int	args_size;
  unsigned int	pretend_size;
  unsigned int	extra_size;
  unsigned int 	reg_size;
  unsigned int 	gmask;

  var_size	= WORD_ALIGN (get_frame_size ());
  args_size	= WORD_ALIGN (current_function_outgoing_args_size);
  pretend_size	= current_function_pretend_args_size;

  reg_size	= 0;
  gmask		= 0;

  /* Calculate space needed for registers.  */
  for (regno = 0; regno < FIRST_PSEUDO_REGISTER; regno ++)
    {
      if (MUST_SAVE_REGISTER (regno))
	{
	  reg_size += UNITS_PER_WORD;
	  gmask |= 1 << regno;
	}
    }

  current_frame_info.save_fp = MUST_SAVE_FRAME_POINTER;
  current_frame_info.save_rp = MUST_SAVE_RETURN_POINTER;

  reg_size += (current_frame_info.save_fp + current_frame_info.save_rp)
	       * UNITS_PER_WORD;

  /* Save computed information.  */
  current_frame_info.pretend_size = pretend_size;
  current_frame_info.var_size     = var_size;
  current_frame_info.args_size    = args_size;
  current_frame_info.reg_size	  = reg_size;
  current_frame_info.frame_size   = args_size + var_size;
  current_frame_info.total_size   = args_size + var_size + reg_size + pretend_size;
  current_frame_info.gmask	  = gmask;
  current_frame_info.initialised  = reload_completed;

  /* Calculate the required distance.  */
  return_value = 0;
  
  if (to_reg == STACK_POINTER_REGNUM)
    return_value += args_size + var_size;
  
  if (from_reg == ARG_POINTER_REGNUM)
    return_value += reg_size;

  return return_value;
}

/* Called after register allocation to add any instructions needed for the
   prologue.  Using a prologue insn is favored compared to putting all of the
   instructions in the FUNCTION_PROLOGUE macro, since it allows the scheduler
   to intermix instructions with the saves of the caller saved registers.  In
   some cases, it might be necessary to emit a barrier instruction as the last
   insn to prevent such scheduling.  */

void
fr30_expand_prologue ()
{
  int regno;

  if (! current_frame_info.initialised)
    fr30_compute_frame_size (0, 0);

  /* This cases shouldn't happen.  Catch it now.  */
  if (current_frame_info.total_size == 0
      && current_frame_info.gmask)
    abort ();

  /* Allocate space for register arguments if this is a variadic function.  */
  if (current_frame_info.pretend_size)
    {
      int regs_to_save = current_frame_info.pretend_size / UNITS_PER_WORD;
      
      /* Push argument registers into the pretend arg area.  */
      for (regno = FIRST_ARG_REGNUM + FR30_NUM_ARG_REGS; regno --, regs_to_save --;)
	emit_insn (gen_movsi_push (gen_rtx_REG (Pmode, regno)));
    }

  if (current_frame_info.gmask)
    {
      /* Save any needed call-saved regs.  */
      for (regno = STACK_POINTER_REGNUM; regno--;)
	{
	  if ((current_frame_info.gmask & (1 << regno)) != 0)
	    emit_insn (gen_movsi_push (gen_rtx_REG (Pmode, regno)));
	}
    }

  /* Save return address if necessary.  */
  if (current_frame_info.save_rp)
    emit_insn (gen_movsi_push (gen_rtx_REG (Pmode, RETURN_POINTER_REGNUM)));

  /* Save old frame pointer and create new one, if necessary.  */
  if (current_frame_info.save_fp)
    {
      if (current_frame_info.frame_size < ((1 << 10) - UNITS_PER_WORD))
	emit_insn (gen_enter_func (GEN_INT (current_frame_info.frame_size + UNITS_PER_WORD)));
      else
	{
	  emit_insn (gen_movsi_push (frame_pointer_rtx));
	  emit_insn (gen_movsi (frame_pointer_rtx, stack_pointer_rtx));
	}
    }

  /* Allocate the stack frame.  */
  if (current_frame_info.frame_size == 0)
    ; /* Nothing to do.  */
  else if (current_frame_info.save_fp
	   && current_frame_info.frame_size < ((1 << 10) - UNITS_PER_WORD))
    ; /* Nothing to do.  */
  else if (current_frame_info.frame_size <= 512)
    emit_insn (gen_add_to_stack (GEN_INT (- current_frame_info.frame_size)));
  else
    {
      rtx tmp = gen_rtx_REG (Pmode, PROLOGUE_TMP_REGNUM);
      emit_insn (gen_movsi (tmp, GEN_INT (current_frame_info.frame_size)));
      emit_insn (gen_subsi3 (stack_pointer_rtx, stack_pointer_rtx, tmp));
    }

  if (profile_flag || profile_block_flag)
    emit_insn (gen_blockage ());
}

/* Called after register allocation to add any instructions needed for the
   epilogue.  Using a epilogue insn is favored compared to putting all of the
   instructions in the FUNCTION_EPILOGUE macro, since it allows the scheduler
   to intermix instructions with the restores of the caller saved registers.
   In some cases, it might be necessary to emit a barrier instruction as the
   first insn to prevent such scheduling.  */
void
fr30_expand_epilogue ()
{
  int regno;

  /* Perform the inversion operations of the prologue.  */
  if (! current_frame_info.initialised)
    abort ();

  /* Pop local variables and arguments off the stack.
     If frame_pointer_needed is TRUE then the frame pointer register
     has actually been used as a frame pointer, and we can recover
     the stack pointer from it, otherwise we must unwind the stack
     manually.  */
  if (current_frame_info.frame_size > 0)
    {
      if (current_frame_info.save_fp && frame_pointer_needed)
	{
	  emit_insn (gen_leave_func ());
	  current_frame_info.save_fp = 0;
	}
      else if (current_frame_info.frame_size <= 508)
	emit_insn (gen_add_to_stack
		   (GEN_INT (current_frame_info.frame_size)));
      else
	{
	  rtx tmp = gen_rtx_REG (Pmode, PROLOGUE_TMP_REGNUM);
	  emit_insn (gen_movsi (tmp, GEN_INT (current_frame_info.frame_size)));
	  emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx, tmp));
	}
    }
  
  if (current_frame_info.save_fp)
    emit_insn (gen_movsi_pop (frame_pointer_rtx));
  
  /* Pop all the registers that were pushed.  */
  if (current_frame_info.save_rp)
    emit_insn (gen_movsi_pop (gen_rtx_REG (Pmode, RETURN_POINTER_REGNUM)));
    
  for (regno = 0; regno < STACK_POINTER_REGNUM; regno ++)
    if (current_frame_info.gmask & (1 << regno))
      emit_insn (gen_movsi_pop (gen_rtx_REG (Pmode, regno)));
  
  if (current_frame_info.pretend_size)
    emit_insn (gen_add_to_stack (GEN_INT (current_frame_info.pretend_size)));

  /* Reset state info for each function.  */
  current_frame_info = zero_frame_info;

  emit_insn (gen_return_from_func ());
}

/* Do any needed setup for a variadic function.  We must create a register
   parameter block, and then copy any anonymous arguments, plus the last
   named argument, from registers into memory.  * copying actually done in
   fr30_expand_prologue().

   ARG_REGS_USED_SO_FAR has *not* been updated for the last named argument
   which has type TYPE and mode MODE, and we rely on this fact.  */
void
fr30_setup_incoming_varargs (arg_regs_used_so_far, int_mode, type, pretend_size)
     CUMULATIVE_ARGS arg_regs_used_so_far;
     int             int_mode;
     tree            type;
     int *           pretend_size;
{
  enum machine_mode mode = (enum machine_mode)int_mode;
  int               first_anon_arg;
  int               size;
  rtx               regblock;

  
  /* All BLKmode values are passed by reference.  */
  if (mode == BLKmode)
    abort ();

#if STRICT_ARGUMENT_NAMING
  /* We must treat `__builtin_va_alist' as an anonymous arg.
     But otherwise if STRICT_ARGUMENT_NAMING is true then the
     last named arg must not be treated as an anonymous arg. */
  if (! current_function_varargs)
    arg_regs_used_so_far += fr30_num_arg_regs (int_mode, type);
#endif
  
  size = FR30_NUM_ARG_REGS - arg_regs_used_so_far;

  if (size <= 0)
    return;

  * pretend_size = (size * UNITS_PER_WORD);
}

/*}}}*/
/*{{{  Printing operands */ 

/* Print a memory address as an operand to reference that memory location.  */

void
fr30_print_operand_address (stream, address)
     FILE * stream;
     rtx    address;
{
  switch (GET_CODE (address))
    {
    case SYMBOL_REF:
      output_addr_const (stream, address);
      break;
      
    default:
      fprintf (stderr, "code = %x\n", GET_CODE (address));
      debug_rtx (address);
      output_operand_lossage ("fr30_print_operand_address: unhandled address");
      break;
    }
}

/* Print an operand.  */

void
fr30_print_operand (file, x, code)
     FILE * file;
     rtx    x;
     int    code;
{
  rtx x0;
  
  switch (code)
    {
    case '#':
      /* Output a :D if this instruction is delayed.  */
      if (dbr_sequence_length () != 0)
	fputs (":D", file);
      return;
      
    case 'p':
      /* Compute the register name of the second register in a hi/lo
	 register pair.  */
      if (GET_CODE (x) != REG)
	output_operand_lossage ("fr30_print_operand: unrecognised %p code");
      else
	fprintf (file, "r%d", REGNO (x) + 1);
      return;
      
    case 'b':
      /* Convert GCC's comparison operators into FR30 comparison codes.  */
      switch (GET_CODE (x))
	{
	case EQ:  fprintf (file, "eq"); break;
	case NE:  fprintf (file, "ne"); break;
	case LT:  fprintf (file, "lt"); break;
	case LE:  fprintf (file, "le"); break;
	case GT:  fprintf (file, "gt"); break;
	case GE:  fprintf (file, "ge"); break;
	case LTU: fprintf (file, "c"); break;
	case LEU: fprintf (file, "ls"); break;
	case GTU: fprintf (file, "hi"); break;
	case GEU: fprintf (file, "nc");  break;
	default:
	  output_operand_lossage ("fr30_print_operand: unrecognised %b code");
	  break;
	}
      return;
      
    case 'B':
      /* Convert GCC's comparison operators into the complimentary FR30
	 comparison codes.  */
      switch (GET_CODE (x))
	{
	case EQ:  fprintf (file, "ne"); break;
	case NE:  fprintf (file, "eq"); break;
	case LT:  fprintf (file, "ge"); break;
	case LE:  fprintf (file, "gt"); break;
	case GT:  fprintf (file, "le"); break;
	case GE:  fprintf (file, "lt"); break;
	case LTU: fprintf (file, "nc"); break;
	case LEU: fprintf (file, "hi"); break;
	case GTU: fprintf (file, "ls"); break;
	case GEU: fprintf (file, "c"); break;
	default:
	  output_operand_lossage ("fr30_print_operand: unrecognised %B code");
	  break;
	}
      return;

    case 'A':
      /* Print a signed byte value as an unsigned value.  */
      if (GET_CODE (x) != CONST_INT)
	output_operand_lossage ("fr30_print_operand: invalid operand to %A code");
      else
	{
	  HOST_WIDE_INT val;
	  
	  val = INTVAL (x);

	  val &= 0xff;

	  fprintf (file, "%d", val);
	}
      return;
      
    case 'x':
      if (GET_CODE (x) != CONST_INT
	  || INTVAL (x) < 16
	  || INTVAL (x) > 32)
	output_operand_lossage ("fr30_print_operand: invalid %x code");
      else
	fprintf (file, "%d", INTVAL (x) - 16);
      return;

    case 'F':
      if (GET_CODE (x) != CONST_DOUBLE)
	output_operand_lossage ("fr30_print_operand: invalid %F code");
      else
	{
	  REAL_VALUE_TYPE d;

	  REAL_VALUE_FROM_CONST_DOUBLE (d, x);
	  fprintf (file, "%.8f", d);
	}
      return;
      
    case 0:
      /* Handled below.  */
      break;
      
    default:
      fprintf (stderr, "unknown code = %x\n", code);
      output_operand_lossage ("fr30_print_operand: unknown code");
      return;
    }

  switch (GET_CODE (x))
    {
    case REG:
      fputs (reg_names [REGNO (x)], file);
      break;

    case MEM:
      x0 = XEXP (x,0);
      
      switch (GET_CODE (x0))
	{
	case REG:
	  if (REGNO (x0) >= (sizeof (reg_names) / sizeof (reg_names[0])))
	    abort ();
	  fprintf (file, "@%s", reg_names [REGNO (x0)]);
	  break;

	case PLUS:
	  if (GET_CODE (XEXP (x0, 0)) != REG
	      || REGNO (XEXP (x0, 0)) < FRAME_POINTER_REGNUM
	      || REGNO (XEXP (x0, 0)) > STACK_POINTER_REGNUM
	      || GET_CODE (XEXP (x0, 1)) != CONST_INT)
	    {
	      fprintf (stderr, "bad INDEXed address:");
	      debug_rtx (x);
	      output_operand_lossage ("fr30_print_operand: unhandled MEM");
	    }
	  else if (REGNO (XEXP (x0, 0)) == FRAME_POINTER_REGNUM)
	    {
	      HOST_WIDE_INT val = INTVAL (XEXP (x0, 1));
	      if (val < -(1 << 9) || val > ((1 << 9) - 4))
		{
		  fprintf (stderr, "frame INDEX out of range:");
		  debug_rtx (x);
		  output_operand_lossage ("fr30_print_operand: unhandled MEM");
		}
	      fprintf (file, "@(r14, #%d)", val);
	    }
	  else
	    {
	      HOST_WIDE_INT val = INTVAL (XEXP (x0, 1));
	      if (val < 0 || val > ((1 << 6) - 4))
		{
		  fprintf (stderr, "stack INDEX out of range:");
		  debug_rtx (x);
		  output_operand_lossage ("fr30_print_operand: unhandled MEM");
		}
	      fprintf (file, "@(r15, #%d)", val);
	    }
	  break;
	  
	case SYMBOL_REF:
	  output_address (x0);
	  break;
	  
	default:
	  fprintf (stderr, "bad MEM code = %x\n", GET_CODE (x0));
	  debug_rtx (x);
	  output_operand_lossage ("fr30_print_operand: unhandled MEM");
	  break;
	}
      break;
      
    case CONST_DOUBLE :
      /* We handle SFmode constants here as output_addr_const doesn't.  */
      if (GET_MODE (x) == SFmode)
	{
	  REAL_VALUE_TYPE d;
	  long l;

	  REAL_VALUE_FROM_CONST_DOUBLE (d, x);
	  REAL_VALUE_TO_TARGET_SINGLE (d, l);
	  fprintf (file, "0x%08lx", l);
	  break;
	}

      /* Fall through.  Let output_addr_const deal with it.  */
    default:
      output_addr_const (file, x);
      break;
    }

  return;
}

/*}}}*/
/*{{{  Function arguments */ 

/* Compute the number of word sized registers needed to hold a
   function argument of mode INT_MODE and tree type TYPE.  */
int
fr30_num_arg_regs (int_mode, type)
     int int_mode;
     tree type;
{
  enum machine_mode mode = (enum machine_mode) int_mode;
  int size;

  if (MUST_PASS_IN_STACK (mode, type))
    return 0;

  if (type && mode == BLKmode)
    size = int_size_in_bytes (type);
  else
    size = GET_MODE_SIZE (mode);

  return (size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
}

/* Implements the FUNCTION_ARG_PARTIAL_NREGS macro.
   Returns the number of argument registers required to hold *part* of
   a parameter of machine mode MODE and tree type TYPE (which may be
   NULL if the type is not known).  If the argument fits entirly in
   the argument registers, or entirely on the stack, then 0 is returned.
   CUM is the number of argument registers already used by earlier
   parameters to the function.  */

int
fr30_function_arg_partial_nregs (cum, int_mode, type, named)
     CUMULATIVE_ARGS cum;
     int int_mode;
     tree type;
     int named;
{
  /* Unnamed arguments, ie those that are prototyped as ...
     are always passed on the stack.
     Also check here to see if all the argument registers are full.  */
  if (named == 0 || cum >= FR30_NUM_ARG_REGS)
    return 0;

  /* Work out how many argument registers would be needed if this
     parameter were to be passed entirely in registers.  If there
     are sufficient argument registers available (or if no registers
     are needed because the parameter must be passed on the stack)
     then return zero, as this parameter does not require partial
     register, partial stack stack space.  */
  if (cum + fr30_num_arg_regs (int_mode, type) <= FR30_NUM_ARG_REGS)
    return 0;
  
  /* Otherwise return the number of registers that would be used.  */
  return FR30_NUM_ARG_REGS - cum;
}

/*}}}*/
/*{{{  Operand predicates */ 

/* Returns true if OPERAND is an integer value suitable for use in
   an ADDSP instruction.  */
int
stack_add_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return
    (GET_CODE (operand) == CONST_INT
     && INTVAL (operand) >= -512
     && INTVAL (operand) <=  508
     && ((INTVAL (operand) & 3) == 0));
}

/* Returns true if OPERAND is an integer value suitable for use in
   an ADD por ADD2 instruction, or if it is a register.  */
int
add_immediate_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return
    (GET_CODE (operand) == REG
     || (GET_CODE (operand) == CONST_INT
	 && INTVAL (operand) >= -16
	 && INTVAL (operand) <=  15));
}

/* Returns true if OPERAND is hard register in the range 8 - 15.  */
int
high_register_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return
    (GET_CODE (operand) == REG
     && REGNO (operand) <= 15
     && REGNO (operand) >= 8);
}

/* Returns true if OPERAND is hard register in the range 0 - 7.  */
int
low_register_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return
    (GET_CODE (operand) == REG
     && REGNO (operand) <= 7
     && REGNO (operand) >= 0);
}

/* Returns true if OPERAND is suitable for use in a CALL insn.  */
int
call_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  enum machine_mode mode = (enum machine_mode) int_mode;
  
  return nonimmediate_operand (operand, mode)
    || (GET_CODE (operand) == MEM
	&& GET_CODE (XEXP (operand, 0)) == SYMBOL_REF);
}

/* Returns true if OPERAND is a displacement suitable for use in a
   frame pointer based load or store.  */
int
fp_displacement_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return (GET_CODE (operand) == CONST_INT
	  && ((INTVAL (operand) & 0x3) == 0)
	  && (INTVAL (operand) <= ((1 << 9) - 1))
	  && (INTVAL (operand) >= (- (1 << 9))));
}

/* Returns true if OPERAND is a displacement suitable for use in a
   stack pointer based load or store.  */
int
sp_displacement_operand (operand, int_mode)
     rtx operand;
     int int_mode;
{
  return (GET_CODE (operand) == CONST_INT
	  && ((INTVAL (operand) & 0x3) == 0)
	  && (INTVAL (operand) <= ((1 << 6) - 1))
	  && (INTVAL (operand) >= 0));
}

/* Returns true iff all the registers in the operands array
   are in descending or ascending order.  */
int
fr30_check_multiple_regs (operands, num_operands, descending)
     rtx * operands;
     int   num_operands;
     int   descending;
{
  if (descending)
    {
      int prev_regno = -1;
      
      while (num_operands--)
	{
	  if (GET_CODE (operands [num_operands]) != REG)
	    return 0;
	  
	  if (REGNO (operands [num_operands]) < prev_regno)
	    return 0;
	  
	  prev_regno = REGNO (operands [num_operands]);
	}
    }
  else
    {
      int prev_regno = CONDITION_CODE_REGNUM;
      
      while (num_operands--)
	{
	  if (GET_CODE (operands [num_operands]) != REG)
	    return 0;
	  
	  if (REGNO (operands [num_operands]) > prev_regno)
	    return 0;
	  
	  prev_regno = REGNO (operands [num_operands]);
	}
    }

  return 1;
}

/*}}}*/

/* Local Variables: */
/* folded-file: t   */
/* End:		    */

/* END CYGNUS LOCAL -- nickc/fr30 */

