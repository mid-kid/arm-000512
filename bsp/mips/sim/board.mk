#
# sim/board.mk -- Makefile fragment for simulator bsp.
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
BOARD_NAME          = simstub

BOARD_SRCDIR	    = @top_srcdir@/@archdir@/sim
BOARD_INCDIR	    = sim

#BOARD_HEADERS	    = bsp/$(BOARD_NAME).h
BOARD_LIBBSP	    = lib$(BOARD_NAME).a
BOARD_OBJS	    = init_sim.o sim.o
BOARD_CFLAGS	    = -msoft-float
#BOARD_CFLAGS	    =

BOARD_LDFLAGS	    = -L../../soft-float/newlib/
#BOARD_LDFLAGS	    = -L../../newlib/
BOARD_DEFINES	    = 

BOARD_SPECS	    = $(BOARD_NAME)-rom.specs
BOARD_RAM_SPECS	    = $(BOARD_NAME).specs
BOARD_ROM_STARTUP   = $(BOARD_NAME)-start.o $(BOARD_NAME)-c_start.o
BOARD_RAM_STARTUP   = $(BOARD_NAME)-bsp-crt0.o $(BOARD_NAME)-c_bsp-crt0.o
BOARD_EXTRAS	    = 
