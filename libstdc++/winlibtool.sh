#! /bin/sh
# This scripts emulates libtool 1.3's building of a cygwin DLL.
# Derived from ltmain.sh.

set -e
set -x

CC="$1"
SRCDIR="$2"
DLL="$3"
LIB="$4"
PICFILES="$5"

# The following hack works for native and canadian-cross builds.
# It cannot support ordinary cross builds yet.

AS=`echo $CC | sed -e s,gcc,as,`
DLLTOOL=`echo $CC | sed -e s,gcc,dlltool,`


PICLIST=`cat $PICFILES`

#---- export_symbol_commands ----

LTDLL="${DLL}-ltdll.c"
cat > $LTDLL <<'EOF'
/* ltdll.c starts here */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <stdio.h>
#
#ifdef __cplusplus
extern "C" {
#endif
BOOL APIENTRY DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved);
#ifdef __cplusplus
}
#endif
#
#include <cygwin/cygwin_dll.h>
DECLARE_CYGWIN_DLL( DllMain );
HINSTANCE __hDllInstance_base;
#
BOOL APIENTRY
DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  __hDllInstance_base = hInst;
  return TRUE;
}
/* ltdll.c ends here */
EOF

$CC -c $LTDLL

$DLLTOOL --export-all --exclude-symbols  DllMain@12,_cygwin_dll_entry@12,_cygwin_noncygwin_dll_entry@12 --output-def ${DLL}-def ${DLL}-ltdll.o $PICLIST

sed -e "1,/EXPORTS/d" -e "s/ @ [0-9]* ;.*//" < ${DLL}-def > ${DLL}-exp

#---- archive_expsym_cmds ----

echo EXPORTS > ${DLL}-def
set +x
_lt_hint=1;
for symbol in `cat ${DLL}-exp`; do
   echo "  $symbol @ $_lt_hint ; " >> ${DLL}-def;
   _lt_hint=`expr 1 + $_lt_hint`;
done

set -x
$CC -Wl,--base-file,${DLL}-base -Wl,--dll -nostartfiles -Wl,-e,__cygwin_dll_entry@12 -o $DLL ${DLL}-ltdll.o $PICLIST

$DLLTOOL --as=$AS --dllname $DLL --exclude-symbols DllMain@12,_cygwin_dll_entry@12,_cygwin_noncygwin_dll_entry@12 --def ${DLL}-def --base-file ${DLL}-base --output-exp ${DLL}-exp

$CC -Wl,--base-file,${DLL}-base ${DLL}-exp -Wl,--dll -nostartfiles -Wl,-e,__cygwin_dll_entry@12 -o $DLL ${DLL}-ltdll.o $PICLIST

$DLLTOOL --as=$AS --dllname $DLL --exclude-symbols DllMain@12,_cygwin_dll_entry@12,_cygwin_noncygwin_dll_entry@12 --def ${DLL}-def --base-file ${DLL}-base --output-exp ${DLL}-exp

$CC ${DLL}-exp  -Wl,--base-file,${DLL}-base -Wl,--dll -Wl,--image-base,0x62000000 -nostartfiles -Wl,-e,__cygwin_dll_entry@12 -o $DLL ${DLL}-ltdll.o $PICLIST

#---- old_archive_from_new_cmds ----
$DLLTOOL --as=$AS --dllname $DLL --def ${DLL}-def --output-lib ${LIB}
