all: hello.exe dhry.exe cplus.exe win.exe
	hello.exe
	dhry.exe	
	cplus.exe
	ls -a hello.exe	
	du -a ..
	cp win.exe foo.exe
	foo.exe

hello.exe:hello.c
	gcc -o hello.exe hello.c

dhry.exe:dhry1.c dhry2.c
	gcc -o dhry.exe -O2 -DTIME dhry1.c dhry2.c

cplus.exe:cplus.cc
	gcc -o cplus.exe cplus.cc -lstdcxx

DLLS=-lkernel32  -luser32 -lgdi32 

win.exe:win.c
	gcc -c win.c
	gcc -o junk --Xlinker --base-file=win.base win.o $(DLLS)
	dlltool --base-file win.base --output-exp winexp.o
	gcc -o win.exe win.o winexp.o  $(DLLS)








