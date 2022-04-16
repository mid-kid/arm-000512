/* Simulator header for cgen cpus.
   Copyright (C) 1998 Free Software Foundation, Inc.
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

#ifndef CGEN_CPU_H
#define CGEN_CPU_H

/* Name of function that is ultimately called by sim_resume.  */
typedef void (ENGINE_FN) (SIM_CPU *);

/* Additional non-machine generated per-cpu data to go in SIM_CPU.
   The member's name must be `cgen_cpu'.  */

typedef struct {
  /* Non-zero while cpu is running.  */
  int running_p;
#define CPU_RUNNING_P(cpu) ((cpu)->cgen_cpu.running_p)

  /* Instruction count.  This is maintained even in fast mode to keep track
     of simulator speed.  */
  unsigned long insn_count;
#define CPU_INSN_COUNT(cpu) ((cpu)->cgen_cpu.insn_count)

  /* sim_resume handlers */
  ENGINE_FN *fast_engine_fn;
#define CPU_FAST_ENGINE_FN(cpu) ((cpu)->cgen_cpu.fast_engine_fn)
  ENGINE_FN *full_engine_fn;
#define CPU_FULL_ENGINE_FN(cpu) ((cpu)->cgen_cpu.full_engine_fn)

  /* Maximum number of instructions per time slice.
     When single stepping this is 1.  If using the pbb model, this can be
     more than 1.  0 means "as long as you want".  */
  unsigned int max_slice_insns;
#define CPU_MAX_SLICE_INSNS(cpu) ((cpu)->cgen_cpu.max_slice_insns)

  /* Simulator's execution cache.
     Allocate space for this even if not used as some simulators may have
     one machine variant that uses the scache and another that doesn't and
     we don't want members in this struct to move about.  */
  CPU_SCACHE scache;

  /* Instruction descriptor table.  */
  IDESC *idesc;
#define CPU_IDESC(cpu) ((cpu)->cgen_cpu.idesc)

  /* Whether the read,write,semantic entries have been initialized or not.
     These are computed goto labels.  */
  int idesc_read_init_p;
#define CPU_IDESC_READ_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_read_init_p)
  int idesc_write_init_p;
#define CPU_IDESC_WRITE_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_write_init_p)
  int idesc_sem_init_p;
#define CPU_IDESC_SEM_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_sem_init_p)

  /* Function to fetch the opcode table entry in the IDESC.  */
  const CGEN_INSN * (*opcode) (SIM_CPU *, int);
#define CPU_OPCODE(cpu) ((cpu)->cgen_cpu.opcode)

  /* Allow slop in size calcs for case where multiple cpu types are supported
     and space for the specified cpu is malloc'd at run time.  */
  double slop;
} CGEN_CPU;

/* Shorthand macro for fetching registers.
   CPU_CGEN_HW is defined in cpu.h.  */
#define CPU(x) (CPU_CGEN_HW (current_cpu)->x)

#endif /* CGEN_CPU_H */
