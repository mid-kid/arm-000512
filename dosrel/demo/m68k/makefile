.SUFFIXES : .x .o .c .s
CC=gcc
AS=as
LD=ld
NM=nm
SIZE=size
OBJDUMP=objdump

# these are the flags needed to produce a fully linked binary. If there are
# undefined symbols from libc.a, then pass -nodefaultlibs as well.
IDP_LDFLAGS= -N -Wl,-Tidp.ld 
MVME_LDFLAGS=  -N -Wl,-Tmvme135.ld

all: hi-idp.x hi-mvme.x
	$(NM) hi-idp.x
	$(NM) hi-mvme.x
	$(OBJDUMP) -d -j .text hello.o
	$(OBJDUMP) -s -j .rodata hello.o
	$(SIZE) hi-idp.x
	$(SIZE) hi-mvme.x

hello.s : hello.c
	$(CC) -O -g -S hello.c

hello.o : hello.s
	$(AS) -ahld -o hello.o hello.s

hi-idp.x: hello.o
	$(CC) -o hi-idp.x hello.o $(IDP_LDFLAGS)

hi-mvme.x: hello.o
	$(CC)  -o hi-mvme.x hello.o $(MVME_LDFLAGS)

clean:
	rm -f *.[osx]
