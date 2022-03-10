.SUFFIXES : .x .o .c .s
CC=gcc
AS=as
LD=ld
NM=nm
SIZE=size
OBJDUMP=objdump

# these are the flags needed to produce a fully linked binary. If there are
# undefined symbols from libc.a, then pass -nodefaultlibs as well.
# we pass -nostartfiles, cause gcc keeps trying to link in a second crt0.o.
LDFLAGS= -N -Wl,-Tex930.ld -nostartfiles

all: hi-ex930.x
	$(NM) hi-ex930.x
	$(OBJDUMP) -d -j .text hello.o
	$(OBJDUMP) -s -j .rodata hello.o
	$(SIZE) hi-ex930.x

hello.s: hello.c
	$(CC) -O -g -S hello.c

hello.o: hello.s
	$(AS) -ahld -o hello.o hello.s

hi-ex930.x: hello.o
	$(CC) -o hi-ex930.x hello.o $(LDFLAGS)

clean:
	rm -f *.[osx]
