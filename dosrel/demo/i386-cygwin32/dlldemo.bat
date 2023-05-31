rem build the dll
gcc -c -O2 -o dll.o dll.c 
ld -s  -e _dll_entry@12 --base-file dll.base -o dll.jnk dll.o  --dll 
dlltool --dllname dll.dll --def dll.def --base-file dll.base --output-exp dll.exp --output-lib dll.a 
ld -s -e _dll_entry@12 -o dll.dll dll.exp dll.o  --dll 

rem add the executable
gcc -o exe.exe exe.c dll.a





