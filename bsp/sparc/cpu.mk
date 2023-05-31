#
# sparc/cpu.mk -- Makefile fragment for SPARC family.
#
# Copyright (c) 1999 Cygnus Solutions
#
# The authors hereby grant permission to use, copy, modify, distribute,
# and license this software and its documentation for any purpose, provided
# that existing copyright notices are retained in all copies and that this
# notice is included verbatim in any distributions. No written agreement,
# license, or royalty fee is required for any of the authorized uses.
# Modifications to this software may be copyrighted by their authors
# and need not follow the licensing terms described here, provided that
# the new terms are clearly indicated on the first page of each file where
# they apply.
#
BUILD_OPTIONS	   := $(BUILD_OPTIONS)
CPU_SRCDIR	    = @top_srcdir@/@archdir@
CPU_INCDIR	    = $(CPU_SRCDIR)/include
CPU_HEADERS	    = bsp/cpu.h
CPU_OBJS	    = cpu.o gdb-cpu.o singlestep.o irq-cpu.o trap.o \
		      mem.o mem-asm.o reg.o exceptions.o stublow.o
CPU_CFLAGS	    = 
CPU_LDFLAGS	    =
CPU_DEFINES	    = -DNO_INIT_FUNCTION
