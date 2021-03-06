/* Simulator tracing support for Cpu tools GENerated simulators.
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

#ifndef CGEN_TRACE_H
#define CGEN_TRACE_H

void trace_insn_init (SIM_CPU *, int);
void trace_insn_fini (SIM_CPU *, const struct argbuf *, int);
void trace_insn (SIM_CPU *, const struct cgen_insn *,
		 const struct argbuf *, IADDR);
void trace_extract (SIM_CPU *, IADDR, char *, ...);
void trace_result (SIM_CPU *, char *, int, ...);
void cgen_trace_printf (SIM_CPU *, char *fmt, ...);

/* Trace instruction results.  */
#define TRACE_RESULT_P(cpu, abuf) (TRACE_INSN_P (cpu) && ARGBUF_TRACE_P (abuf))

#define TRACE_INSN_INIT(cpu, abuf, first_p) \
do { \
  if (TRACE_INSN_P (cpu)) \
    trace_insn_init ((cpu), (first_p)); \
} while (0)
#define TRACE_INSN_FINI(cpu, abuf, last_p) \
do { \
  if (TRACE_INSN_P (cpu)) \
    trace_insn_fini ((cpu), (abuf), (last_p)); \
} while (0)
#define TRACE_PRINTF(cpu, what, args) \
do { \
  if (TRACE_P ((cpu), (what))) \
    cgen_trace_printf args ; \
} while (0)
#define TRACE_INSN(cpu, opcode, abuf, pc) \
do { \
  if (TRACE_INSN_P (cpu) && ARGBUF_TRACE_P (abuf)) \
    trace_insn ((cpu), (opcode), (abuf), (pc)) ; \
} while (0)
#define TRACE_EXTRACT(cpu, abuf, args) \
do { \
  if (TRACE_EXTRACT_P (cpu)) \
    trace_extract args ; \
} while (0)
#define TRACE_RESULT(cpu, abuf, name, type, val) \
do { \
  if (TRACE_RESULT_P ((cpu), (abuf))) \
    trace_result ((cpu), (name), (type), (val)) ; \
} while (0)

#endif /* CGEN_TRACE_H */
