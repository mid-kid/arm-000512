
/cygnus/devo/S-NT/gcc/xgcc -B/cygnus/devo/S-NT/gcc/  -c -O2 -o dll.o dll.c 
/cygnus/devo/S-NT/ld/ld.new -s  -e _dll_entry@12 --base-file dll.base -o dll.jnk dll.o  --dll 
/cygnus/devo/S-NT/binutils/dlltool --dllname dll.dll --def dll.def --base-file dll.base --output-exp dll.exp --output-lib dll.a  --nodelete --nodelete
/cygnus/devo/S-NT/ld/ld.new -s -e _dll_entry@12 -o dll.dll dll.exp dll.o  --dll 

rem add the executable
/cygnus/devo/S-NT/gcc/xgcc -B/cygnus/devo/S-NT/gcc/ -o exe.exe exe.c dll.a





