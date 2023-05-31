.SUFFIXES : .x .o .c .s
CC=gcc
AS=as
LD=ld
NM=nm
SIZE=size
OBJDUMP=objdump

all : hello.x
	$(NM) hello.x
	$(OBJDUMP) -d -j .text hello.o
	$(OBJDUMP) -s -j .rodata hello.o
	$(SIZE) hello.x

hello.s : hello.c
	$(CC) -O -g -S hello.c

hello.o : hello.s
	$(AS) -ahld -o hello.o hello.s

hello.x: hello.o
	$(CC) -o hello.x hello.o $(LDFLAGS)
