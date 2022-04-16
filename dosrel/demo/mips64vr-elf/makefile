.SUFFIXES: .x .o .c .s .xl .xb
CC=gcc
AS=as
RUN=run
SRC =  testme.c hello.c
EL  =  testme.xl hello.xl
EB  =  testme.xb hello.xb

all : $(EL) $(EB)

.c.xl:
	$(CC) -g -O2 -EL -Tddb.ld -o $@ $< -lm 

.c.xb:
	$(CC) -g -O2 -EB -Tddb.ld -o $@ $< -lm 

# verify assembler, compiler, simulator, binutils are
# installed properly and run OK
hello.s : hello.c
	$(CC) -O2 -g -S hello.c

hello.o : hello.s
	$(AS) -ahld -o hello.o hello.s

hello.xb: hello.o
	$(CC) -g -O2 -EB -Tddb.ld -o hello.xb hello.o
	nm hello.o
	objdump -d -j .text hello.o
	objdump -s -j .rodata hello.o
	size hello.o
	$(RUN) hello.xb
