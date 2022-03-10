## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = cygnus
MULTI_ICE_INCLUDES = @MULTI_ICE_INCLUDES@
MULTI_ICE_LIBS = @MULTI_ICE_LIBS@
MULTI_ICE_PLATFORM_SOURCE = @MULTI_ICE_PLATFORM_SOURCE@

INCLUDES=-I./.. -I$(srcdir)/../../include \
         -I$(srcdir)/.. -I$(srcdir)/../config -I../../bfd \
         $(MULTI_ICE_INCLUDES)

LIBS=-L../../libiberty -liberty $(WIN32LIBS) $(MULTI_ICE_LIBS)

bin_PROGRAMS = multi-ice-gdb-server
multi_ice_gdb_server_SOURCES = server-main.c rdi150-low.c \
    remote-utils.c utils.c server-win.c arm-singlestep.c

## Dependencies
multi_ice_gdb_server_LDFLAGS = $(WIN32LDAPP)
multi_ice_gdb_server_DEPENDENCIES = $(WIN32RES)
multi_ice_gdb_server_LDADD = $(WIN32RES)

MICE-gdbserverres.o: MICE-gdbserver.rc
	$(WINDRES) --include $(srcdir) $(srcdir)/MICE-gdbserver.rc MICE-gdbserverres.o

