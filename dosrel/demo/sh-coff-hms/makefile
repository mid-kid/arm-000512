.SUFFIXES : .x .o .c .s
CC=gcc
RUN=run
GDB=gdb
ASYNC=asynctsr
COM=1
BAUD=9600

all : hello.x flash.x


flash.o : flash.c
	$(CC) -c -O -g flash.c

flash.x : flash.o
	$(LD) -o flash.x flash.o -Tflash.cmd

# demo of gdb running and talking to eval board
# on serial port

gdbdemo : flash.x
	- mode com$(COM):9600,n,8,1,p
	- $(ASYNC) $(COM)
	$(GDB) -command flash.gsc

hello.s : hello.c
	$(CC) -O -g -S hello.c

hello.o : hello.s
	$(AS) -ahld -o hello.o hello.s


hello.x: hello.o
	$(LD)  -o hello.x ../lib/crt0.o hello.o  -lgcc -lc -lgcc
	- $(RUN) hello.x

