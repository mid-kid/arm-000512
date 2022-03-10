# Common makefile fragment for Cygnus BSP.  This file goes at the top
# of the generated Makefile.
#
# Copyright (c) 1998, 1999 Cygnus Solutions
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

#
# The following make variable is used to determine
# a series of build options that are processed by
# common.mk.  If a name (BUILD_<blah>) is in this
# list, it will enable a certain build-time option
# for the given board.  Generally that will include
# certain object files into the build.
#
# Valid values are:
#      BUILD_GDBSTUB
#      BUILD_THREADS
#      BUILD_SYSLIB
#      BUILD_TCP
#
BUILD_OPTIONS = BUILD_GDBSTUB BUILD_THREADS BUILD_SYSLIB

#
# Make sure all is the default target
#
all:
