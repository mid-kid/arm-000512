#! /bin/sh
# Generate CGEN opcode files: arch-opc.[ch], arch-asm.c, arch-asm.c.
#
# Usage: /bin/sh cgen.sh "opcodes" srcdir cgen cgendir cgenflags \
#	arch prefix options
#
# OPTIONS is comma separated list of options:
#	opinst - generate operand instance tables
#
# We store the generated files in the source directory until we decide to
# ship a Scheme interpreter (or other implementation) with gdb/binutils.
# Maybe we never will.

# We want to behave like make, any error forces us to stop.
set -e

# `action' is currently always "opcodes".
# It exists to be consistent with the simulator.

action=$1
srcdir=$2
cgen=$3
cgendir=$4
cgenflags=$5
arch=$6
prefix=$7
options=$8

rootdir=${srcdir}/..

lowercase='abcdefghijklmnopqrstuvwxyz'
uppercase='ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ARCH=`echo ${arch} | tr "${lowercase}" "${uppercase}"`

case $action in
opcodes)
	rm -f tmp-opc.h tmp-opc.h1
	rm -f tmp-opc.c tmp-opc.in1
	rm -f tmp-asm.c tmp-asm.in1
	rm -f tmp-dis.c tmp-dis.in1

	${cgen} -s ${cgendir}/cgen-opc.scm \
		-s ${cgendir} \
		${cgenflags} \
		-f "${options}" \
		-m all \
		-a ${arch} \
		-h tmp-opc.h1 \
		-t tmp-opc.in1 \
		-A tmp-asm.in1 \
		-D tmp-dis.in1

	sed -e "s/@ARCH@/${ARCH}/g" -e "s/@arch@/${arch}/g" < tmp-opc.h1 > tmp-opc.h
	${rootdir}/move-if-change tmp-opc.h ${srcdir}/${prefix}-opc.h

	cat ${srcdir}/cgen-opc.in tmp-opc.in1 | \
	  sed -e "s/@ARCH@/${ARCH}/g" -e "s/@arch@/${arch}/g" \
		-e "s/@prefix@/${prefix}/" > tmp-opc.c
	${rootdir}/move-if-change tmp-opc.c ${srcdir}/${prefix}-opc.c

	sed -e "/ -- assembler routines/ r tmp-asm.in1" ${srcdir}/cgen-asm.in \
	  | sed -e "s/@ARCH@/${ARCH}/g" -e "s/@arch@/${arch}/g" \
		-e "s/@prefix@/${prefix}/" > tmp-asm.c
	${rootdir}/move-if-change tmp-asm.c ${srcdir}/${prefix}-asm.c

	sed -e "/ -- disassembler routines/ r tmp-dis.in1" ${srcdir}/cgen-dis.in \
	  | sed -e "s/@ARCH@/${ARCH}/g" -e "s/@arch@/${arch}/g" \
		-e "s/@prefix@/${prefix}/" > tmp-dis.c
	${rootdir}/move-if-change tmp-dis.c ${srcdir}/${prefix}-dis.c

	rm -f tmp-opc.h1 tmp-opc.in1
	rm -f tmp-asm.in1 tmp-dis.in1
	;;

*)
	echo "cgen.sh: bad action: ${action}" >&2
	exit 1
	;;

esac

exit 0
