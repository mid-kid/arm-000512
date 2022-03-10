/* setjmp.S

   Copyright 1996, 1998 Cygnus Solutions.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include "ppc-asm.h"

FUNC_START(setjmp)
	mflr	r0
	mfcr	r4
	stfd	f14,0(r3)
	stfd	f15,8(r3)
	stfd	f16,16(r3)
	stfd	f17,24(r3)
	stfd	f18,32(r3)
	stfd	f19,40(r3)
	stfd	f20,48(r3)
	stfd	f21,56(r3)
	stfd	f22,64(r3)
	stfd	f23,72(r3)
	stfd	f24,80(r3)
	stfd	f25,88(r3)
	stfd	f26,96(r3)
	stfd	f27,104(r3)
	stfd	f28,112(r3)
	stfd	f29,120(r3)
	stfd	f30,128(r3)
	stfd	f31,136(r3)
	stw	r13,152(r3)
	stw	r14,156(r3)
	stw	r15,160(r3)
	stw	r16,164(r3)
	stw	r17,168(r3)
	stw	r18,172(r3)
	stw	r19,176(r3)
	stw	r20,180(r3)
	stw	r21,184(r3)
	stw	r22,188(r3)
	stw	r23,192(r3)
	stw	r24,196(r3)
	stw	r25,200(r3)
	stw	r26,204(r3)
	stw	r27,208(r3)
	stw	r28,212(r3)
	stw	r29,216(r3)
	stw	r30,220(r3)
	stw	r31,224(r3)
	stw	r0,232(r3)
	stw	r4,228(r3)
	stw	r1,144(r3)
	stw	r2,148(r3)
	li	r0,0
	stw	r0,236(r3)
	li	r3,0
	blr
FUNC_END(setjmp)
