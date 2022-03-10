# Sed commands that finish translating the BYACC Unix Makefile to MPW syntax.

# Comment out default flag settings.
/^CFLAGS/s/^/#/
/^LDFLAGS/s/^/#/

# Add the return signal type.
/@DEFS@/s/@DEFS@/-d RETSIGTYPE=void/

# Edit the linking compiler line.
/-o/s/^\([ 	]*\){LINKER} \(.*\){\([A-Z_]*\)CFLAGS} \(.*\){LDFLAGS} \(.*\)-o \([^ ]*\) \(.*\)$/\1{CC_LD} \2 {CC_LD_TOOL_FLAGS} \4 {LDFLAGS} \5 -o \6{PROG_EXT} \7\
\1{MAKEPEF} \6{PROG_EXT} -o \6 {MAKEPEF_TOOL_FLAGS} {MAKEPEF_FLAGS}\
\1{REZ} "{s}"mac-byacc.r -o \6 -append -d PROG_NAME='"'\6'"' -d VERSION_STRING='"'{version}'"'/

# Fix the variable name for libraries.
s/{LOADLIBES}/{EXTRALIBS}/

# Remove an unhelpful dependency.
/^depend/d

# Remove the Makefile rebuild rule.
/^Makefile /,/--recheck/d
