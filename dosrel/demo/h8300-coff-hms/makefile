.SUFFIXES : .x .o .c .s
CC=gcc

all : hello.x flash.x
	run hello.x
	
hello.s : hello.c
	$(CC) -O -g -S hello.c

hello.o : hello.s
	$(AS) -ahld -o hello.o hello.s


hello.x: hello.o
	$(LD)  -o hello.x ../lib/crt0.o hello.o -lc -lgcc


flash.x: flash.c
	$(CC) -O -g -c flash.c
	$(LD) -o flash.x -oformat srec ../lib/crt0.o flash.o

