# Sed commands to finish translating the flex Unix makefile into MPW syntax.

# Whatever this is for, it's not useful.
/^SET_MAKE@/d

# Comment out a redundant definition.
/^CPPFLAGS = @CPPFLAGS@/s/^/#/

# Add a path to MPW-specific include files.
/^CPPFLAGS =/s/$/ -i "{srcroot}"include:mpw: -i ::extra-include:/

# Change a dot to an underscore.
/\.bootstrap/s/\.bootstrap/_bootstrap/g

/Duplicate/s/Duplicate -d -y \(.*initscan.c\) \(.*scan.c\)/Catenate \1 > \2/

# Replace libiberty trickiness with requirement for it.
/^LIBIBERTY =/s/LIBIBERTY = .*$/LIBIBERTY = ::libiberty:libiberty.o/

/@LIBS@/s/@LIBS@/{EXTRALIBS}/

# Fix pathnames to various files.
/config.h/s/"{s}"config\.h/"{o}"config.h/g
/config.h/s/^config\.h/"{o}"config.h/

/y.tab.c/s/"{s}"y\.tab\.c/"{o}"y.tab.c/g
/y.tab.c/s/^y\.tab\.c/"{o}"y.tab.c/
/parse.c/s/"{s}"parse\.c/"{o}"parse.c/g
/parse.c/s/^parse\.c/"{o}"parse.c/
/y.tab.h/s/"{s}"y\.tab\.h/"{o}"y.tab.h/g
/y.tab.h/s/^y\.tab\.h/"{o}"y.tab.h/
/parse.h/s/"{s}"parse\.h/"{o}"parse.h/g
/parse.h/s/^parse\.h/"{o}"parse.h/

/scan.c/s/"{s}"scan\.c/"{o}"scan.c/g
/scan.c/s/^scan\.c/"{o}"scan.c/

/{FLEX}\.r/s/{FLEX}\.r/mac-flex.r/g

# Rename a target that differs only in case.
/TAGS/s/TAGS/tags2/g

# Remove un-useful targets.
/^Makefile \\Option-f/,/^$/d
/^"{o}"config.h \\Option-f/,/^$/d
/^config.status \\Option-f/,/^$/d
/^skel.c \\Option-f/,/^$/d

