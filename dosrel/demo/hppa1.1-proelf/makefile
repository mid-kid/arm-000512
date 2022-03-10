.SUFFIXES : .x .o .c .s
CC=gcc
AS=as
LD=ld
NM=nm
SIZE=size
OBJDUMP=objdump

# these are the flags needed to produce a fully linked binary. If there are
# undefined symbols from libc.a, then pass -nodefaultlibs as well.
OKI_LDFLAGS= -Wl,-Top50n.ld -msoft-float
WEC_LDFLAGS= -Wl,-Tw89k.ld -msoft-float

all: hi-oki.x hi-wec.x
	$(NM) hi-wec.x
	$(NM) hi-oki.x
	$(OBJDUMP) -d -j .text hello.o
	$(OBJDUMP) -s -j .rodata hello.o
	$(SIZE) hi-wec.x
	$(SIZE) hi-oki.x

hello.s: hello.c
	$(CC) $(CFLAGS) -S hello.c

hello.o: hello.s
	$(AS) -ahld -o hello.o hello.s

hi-oki.x: hello.o
	$(CC)  -o hi-oki.x hello.o $(OKI_LDFLAGS)

hi-wec.x: hello.c
	$(CC) -o hi-wec.x hello.o $(WEC_LDFLAGS)

clean:
	rm -f *.[osx] *.nm *.dis *.sre
