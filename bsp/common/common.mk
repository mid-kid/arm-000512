# Common makefile fragment for Cygnus BSP.  This file goes at the bottom
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

COMMON_SRCDIR        = @top_srcdir@/common
COMMON_INCDIR        = @top_srcdir@/include
COMMON_SRCDIR_EXTRAS = @top_srcdir@/syslib:@top_srcdir@/net

vpath %c      $(BOARD_SRCDIR):$(BOARD_SRCDIR_EXTRAS):$(CPU_SRCDIR):$(COMMON_SRCDIR):$(COMMON_SRCDIR_EXTRAS)
vpath %S      $(BOARD_SRCDIR):$(BOARD_SRCDIR_EXTRAS):$(CPU_SRCDIR)
vpath %h      $(BOARD_INCDIR):$(CPU_INCDIR):$(COMMON_INCDIR)
vpath %ld     $(COMMON_SRCDIR):$(CPU_SRCDIR)
vpath %specs  $(BOARD_SRCDIR):$(BOARD_SRCDIR_EXTRAS)

prefix             = @prefix@
exec_prefix	   = @exec_prefix@
host_alias	   = @host_alias@
target_alias	   = @target_alias@
bindir		   = @bindir@
libdir		   = @libdir@
tooldir		   = $(exec_prefix)/$(target_alias)

CC                 = @CC@
AR                 = @AR@
ARFLAGS            = rcs
OBJCOPY            = @OBJCOPY@
OBJDUMP            = @OBJDUMP@
INSTALL		   = @INSTALL@
INSTALL_PROGRAM	   = @INSTALL_PROGRAM@
INSTALL_DATA	   = @INSTALL_DATA@
MKINSTALLDIRS	   = @top_srcdir@/mkinstalldirs

SPECS_FLAGS	   = `if [ -d $(BOARD_SRCDIR)/ ] ; then echo -B$(BOARD_SRCDIR)/ ; fi` -specs=$(BOARD_SPECS_STRIP)$(BOARD_SPECS)
CFLAGS             = -g -O2 -Wall -fno-exceptions
ALL_CFLAGS         = $(INCLUDES) $(CPU_CFLAGS) $(BOARD_CFLAGS) $(CFLAGS) $(SPECS_FLAGS)
ALL_BIG_CFLAGS     = $(INCLUDES) $(CPU_CFLAGS) $(BOARD_BIG_CFLAGS) $(CFLAGS)

#
# remove some sections because COFF assumes there is contents
# here and the section must be written when doing a binary dump
# This causes problems if the base address of this section
# is large.
#
OBJCOPY_FLAGS      = --remove-section=.bss --remove-section=.arm_pag

DEFINES            = $(CPU_DEFINES) $(BOARD_DEFINES)
LDFLAGS	           = -Wl,-Map,bsp.map -B./ -L./ \
                     `if [ -d $(COMMON_SRCDIR)/ ]; then echo -B$(COMMON_SRCDIR)/ ; fi` \
                     `if [ -d $(CPU_SRCDIR)/ ]; then echo -B$(CPU_SRCDIR)/ ; fi` \
                     $(ALL_CFLAGS) $(CPU_LDFLAGS) $(BOARD_LDFLAGS)
INCLUDES           = -I$(COMMON_INCDIR) -I$(CPU_INCDIR) -I$(BOARD_INCDIR)
LIBBSP		   = $(BOARD_LIBBSP)
BSP_HEADERS	   = bsp/bsp.h bsp/hex-utils.h bsp/gdb-data.h bsp/defs.h
LDSCRIPT           = bsp.ld

STANDALONE_OBJS    = main.o
COMMON_OBJS        = hex-utils.o bsp.o shared-data.o bsp_if.o breakpoint.o irq.o \
		     irq-rom.o syscall.o queue.o sysinfo.o debug-io.o console-io.o \
		     bsp_cache.o printf.o vprintf.o sprintf.o bsp_reset.o
ifeq (BUILD_TCP,$(findstring BUILD_TCP,$(BUILD_OPTIONS)))
COMMON_OBJS        := $(COMMON_OBJS) arp.o bootp.o cksum.o enet.o icmp.o ip.o net.o \
		      pktbuf.o socket.o tcp.o timers.o udp.o
endif
ifeq (BUILD_GDBSTUB,$(findstring BUILD_GDBSTUB,$(BUILD_OPTIONS)))
COMMON_OBJS	   := $(COMMON_OBJS) gdb.o gdb-data.o
endif
ifeq (BUILD_THREADS,$(findstring BUILD_THREADS,$(BUILD_OPTIONS)))
COMMON_OBJS        := $(COMMON_OBJS) gdb-threads.o threads-syscall.o
endif
ifeq (BUILD_SYSLIB,$(findstring BUILD_SYSLIB,$(BUILD_OPTIONS)))
COMMON_OBJS        := $(COMMON_OBJS) open.o close.o exit.o lseek.o print.o read.o \
		      write.o sbrk.o getpid.o fstat.o isatty.o kill.o unlink.o raise.o \
		      gettimeofday.o times.o
endif

ALL_OBJS = $(BOARD_OBJS) $(CPU_OBJS) $(COMMON_OBJS) $(STANDALONE_OBJS) \
	   $(BOARD_RAM_STARTUP) $(BOARD_ROM_STARTUP) $(BOARD_EXTRA_OBJS)

#
# Makefile rules
#
%.o: %.c
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) -o $@ $<

$(BOARD_NAME)-%.o: %.c
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) -o $@ $<

%.o: %.S
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) -o $@ $<

$(BOARD_NAME)-%.o: %.S
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) -o $@ $<

$(BOARD_NAME)-%-big.o: %.S
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_BIG_CFLAGS) -o $@ $<

%-big.o: %.S
	$(CC) -c $(CPPFLAGS) $(DEFINES) $(ALL_BIG_CFLAGS) -o $@ $<

%.lst: %.rom
	$(OBJDUMP) $(OBJDUMP_FLAGS) -D $< > $@

%.srec: %.rom
	$(OBJCOPY) $(OBJCOPY_FLAGS) -O srec $< $@

%.bin: %.rom
	$(OBJCOPY) $(OBJCOPY_FLAGS) -O binary $< $@

.deps/%.d: %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) $< | sed '\''s/\($*\.o\)\([ :]*\)/\1 .deps\/$(notdir $@)\2/g'\'' > $@'

.deps/$(BOARD_NAME)-%.d: %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) $< | sed '\''s/\($*\.o\)\([ :]*\)/$(BOARD_NAME)-\1 .deps\/$(notdir $@)\2/g'\'' > $@'

.deps/%.d: %.S
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) $< | sed '\''s/\($*\.o\)\([ :]*\)/\1 .deps\/$(notdir $@)\2/g'\'' > $@'

.deps/$(BOARD_NAME)-%.d: %.S
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_CFLAGS) $< | sed '\''s/\($*\.o\)\([ :]*\)/$(BOARD_NAME)-\1 .deps\/$(notdir $@)\2/g'\'' > $@'

.deps/$(BOARD_NAME)-%-big.d: %.S
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_BIG_CFLAGS) $< | sed '\''s/\($*\)\.o\([ :]*\)/$(BOARD_NAME)-\1-big.o .deps\/$(notdir $@)\2/g'\'' > $@'

.deps/%-big.d: %.S
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $(DEFINES) $(ALL_BIG_CFLAGS) $< | sed '\''s/\($*\)\.o\([ :]*\)/\1-big.o .deps\/$(notdir $@)\2/g'\'' > $@'

#
# Dependencies
#
ALL_DEPEND_OBJS=$(patsubst %.o,.deps/%.d,$(ALL_OBJS))

$(ALL_DEPEND_OBJS): Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS) .deps/stamp

.deps/stamp:
	$(MKINSTALLDIRS) .deps
	touch .deps/stamp

sinclude $(ALL_DEPEND_OBJS)

#
# Main dependencies
#
all: bsp.srec bsp.bin bsp.lst Makefile $(BOARD_EXTRAS) $(BOARD_SPECS_STRIP)$(BOARD_SPECS) $(ALL_DEPEND_OBJS)

bsp.lst: bsp.rom Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS) $(ALL_DEPEND_OBJS)

bsp.srec: bsp.rom Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS) $(ALL_DEPEND_OBJS)

bsp.bin: bsp.rom Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS) $(ALL_DEPEND_OBJS)

bsp.rom: $(BOARD_ROM_STARTUP) $(BOARD_RAM_STARTUP) $(BOARD_SPECS_STRIP)$(BOARD_SPECS) \
	 $(STANDALONE_OBJS) $(LIBBSP) $(LDSCRIPT) $(CPU_LDSCRIPT) \
	 Makefile
	$(CC) $(LDFLAGS) $(STANDALONE_OBJS) -o bsp.rom

$(LIBBSP): $(COMMON_OBJS) $(CPU_OBJS) $(BOARD_OBJS) Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS)
	$(AR) $(ARFLAGS) $(LIBBSP) $(COMMON_OBJS) $(CPU_OBJS) $(BOARD_OBJS)

$(ALL_OBJS): Makefile $(BOARD_SPECS_STRIP)$(BOARD_SPECS)

clean:
	rm -f *~ core *.o a.out bsp.rom bsp.srec bsp.bin bsp.lst bsp.map 
	rm -f $(LIBBSP) $(BOARD_CLEAN)

distclean: clean
	rm -f Makefile .deps/stamp .deps/*.d
	rmdir .deps

install: install_libs install_specs install_ram_specs install_headers

install_libs: $(LDSCRIPT) $(CPU_LDSCRIPT) $(LIBBSP) \
	      $(BOARD_ROM_STARTUP) $(BOARD_RAM_STARTUP) $(BOARD_EXTRA_LIBS)
	$(SHELL) $(MKINSTALLDIRS) $(tooldir)/lib
	set -e; \
	for x in $^; do \
		$(INSTALL_DATA) $$x $(tooldir)/lib/`basename $$x`; \
	done

install_specs: $(BOARD_SPECS_STRIP)$(BOARD_SPECS)
	$(SHELL) $(MKINSTALLDIRS) $(tooldir)/lib
	set -e; \
	$(INSTALL_DATA) $^ $(tooldir)/lib/$(BOARD_SPECS)

install_ram_specs: $(BOARD_SPECS_STRIP)$(BOARD_RAM_SPECS)
	$(SHELL) $(MKINSTALLDIRS) $(tooldir)/lib
	set -e; \
	$(INSTALL_DATA) $^ $(tooldir)/lib/$(BOARD_RAM_SPECS)

install_headers: $(BSP_HEADERS) $(CPU_HEADERS) $(BOARD_HEADERS)
	$(SHELL) $(MKINSTALLDIRS) $(tooldir)/include/bsp
	set -e; \
	for x in $^; do \
		$(INSTALL_DATA) $$x $(tooldir)/include/bsp/`basename $$x`; \
	done
