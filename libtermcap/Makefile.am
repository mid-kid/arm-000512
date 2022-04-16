## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = cygnus

noinst_LIBRARIES = libtermcap.a
libtermcap_a_SOURCES = getcap.c termcap.c tgoto.c tospeed.c tparm.c tputs.c termcap.h pathnames.h

tooldir = $(exec_prefix)/$(host_alias)

INCLUDES = -I@srcdir@/..

$(OBJECTS): termcap.h pathnames.h

install-data-local:
	$(INSTALL_DATA) $(srcdir)/termcap $(prefix)/etc/termcap
	$(INSTALL_DATA) $(srcdir)/termcap.h $(tooldir)/include/termcap.h

install-exec-local: $(noinst_LIBRARIES)
	$(INSTALL) libtermcap.a $(tooldir)/lib/libtermcap.a
