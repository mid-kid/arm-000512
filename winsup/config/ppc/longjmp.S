/* longjmp.S

   Copyright 1996, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include "ppc-asm.h"

FUNC_START(longjmp)
	cmpwi	r4,0
	bne	notz
	li	r4,1
notz:	lfd	f14,0(r3)
	lfd	f15,8(r3)
	lfd	f16,16(r3)
	lfd	f17,24(r3)
	lfd	f18,32(r3)
	lfd	f19,40(r3)
	lfd	f20,48(r3)
	lfd	f21,56(r3)
	lfd	f22,64(r3)
	lfd	f23,72(r3)
	lfd	f24,80(r3)
	lfd	f25,88(r3)
	lfd	f26,96(r3)
	lfd	f27,104(r3)
	lfd	f28,112(r3)
	lfd	f29,120(r3)
	lfd	f30,128(r3)
	lfd	f31,136(r3)
	lwz	r13,152(r3)
	lwz	r14,156(r3)
	lwz	r15,160(r3)
	lwz	r16,164(r3)
	lwz	r17,168(r3)
	lwz	r18,172(r3)
	lwz	r19,176(r3)
	lwz	r20,180(r3)
	lwz	r21,184(r3)
	lwz	r22,188(r3)
	lwz	r23,192(r3)
	lwz	r24,196(r3)
	lwz	r25,200(r3)
	lwz	r26,204(r3)
	lwz	r27,208(r3)
	lwz	r28,212(r3)
	lwz	r29,216(r3)
	lwz	r30,220(r3)
	lwz	r31,224(r3)
	lwz	r5,232(r3)
	lwz	r6,228(r3)
	mtlr	r5
	lwz	r1,144(r3)
	lwz	r2,148(r3)
	mtcr	r6
	mr	r3,r4
	blr
FUNC_END(longjmp)
