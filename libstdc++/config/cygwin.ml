# cygwin - shared libstdc++.

ARLIB   = libstdc++.a.$(VERSION)
MARLINK = libstdc++.a.$(LIBSTDCXX_INTERFACE)-$(VERSION)
ARLINK  = libstdc++.a
SHLIB   = libstdc++.$(VERSION).dll
MSHLINK = libstdc++.$(LIBSTDCXX_INTERFACE)-$(VERSION).dll
SHLINK  = libstdc++.dll
LIBS    = $(SHLIB)-dll $(SHLIB) $(ARLIB) $(ARLINK) mshlink

WINLIBTOOL = $(srcdir)/winlibtool.sh

CLEAN_JUNK = $(LIBS) $(SHLIB)-base $(SHLIB)-def $(SHLIB)-exp $(SHLIB)-ltdll.c $(SHLIB)-ltdll.o
