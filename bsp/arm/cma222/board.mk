#
# cma222/board.mk -- Makefile fragment for the Cogent CMA222 DB w/CMA110 MB
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
# ARM is a Registered Trademark of Advanced RISC Machines Limited.
# Other Brands and Trademarks are the property of their respective owners.
#
BUILD_OPTIONS	   := $(BUILD_OPTIONS)
BOARD_NAME	    = cma222

BOARD_SRCDIR	    = @top_srcdir@/@archdir@/$(BOARD_NAME)
BOARD_INCDIR	    = $(BOARD_SRCDIR)/include

BOARD_HEADERS	    = bsp/$(BOARD_NAME).h
BOARD_LIBBSP	    = lib$(BOARD_NAME).a
BOARD_OBJS	    = init_$(BOARD_NAME).o $(BOARD_NAME).o
BOARD_CFLAGS	    = -msoft-float
BOARD_LDFLAGS	    = -L../../newlib/
BOARD_DEFINES	    = 

#
# This contains the string to strip off of the front of the
# specs file name before installing it.  This will allow us
# to have multiple specs files for different target variants
# (ie elf vs. coff) that will all get installed w/ the same
# name
#
BOARD_SPECS_STRIP   = @target_os@-

BOARD_SPECS	    = $(BOARD_NAME)-rom.specs
BOARD_RAM_SPECS	    = $(BOARD_NAME).specs
BOARD_ROM_STARTUP   = $(BOARD_NAME)-start.o $(BOARD_NAME)-c_start.o $(BOARD_NAME)-vectors.o
BOARD_RAM_STARTUP   = $(BOARD_NAME)-bsp-crt0.o $(BOARD_NAME)-c_bsp-crt0.o
BOARD_EXTRAS	    = 
