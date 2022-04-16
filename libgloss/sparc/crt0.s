! Copyright (c) 1995 Cygnus Support
!
! The authors hereby grant permission to use, copy, modify, distribute,
! and license this software and its documentation for any purpose, provided
! that existing copyright notices are retained in all copies and that this
! notice is included verbatim in any distributions. No written agreement,
! license, or royalty fee is required for any of the authorized uses.
! Modifications to this software may be copyrighted by their authors
! and need not follow the licensing terms described here, provided that
! the new terms are clearly indicated on the first page of each file where
! they apply.
!
! C startup code for the Fujitsu SPARClite demo board

	.text
	.align 8

win_ovf_trap:
	sethi %hi(win_ovf), %l3
	jmpl %lo(win_ovf)+%l3, %g0
	mov %wim, %l0
	nop

win_unf_trap:
	sethi %hi(win_unf), %l3
	jmpl %lo(win_unf)+%l3, %g0
	mov %wim, %l0
	nop

no_fpu_trap:		! Come here when no fpu exists.  This just skips the
	jmpl %l2, %g0	! offending instruction.
	rett %l2+4

	.globl start
	.globl _start

start:
_start:

! First, copy prom & trap vectors to sram

	set 0x30000000, %l0
	set 0xfff8, %l1
	tst %l1				! Set condition codes

copyloop:
	ldd [%l1], %l2
	std %l2, [%l0 + %l1]
	bg copyloop
	deccc 8, %l1

	set 0x30000000, %l0		! Base of new trap vector
	mov %l0, %tbr			! Install the new tbr

	set win_ovf_trap, %l1		! Setup window overflow trap
	ldd [%l1], %l2
	std %l2, [%l0 + 5 * 16]
	ldd [%l1 + 8], %l2
	std %l2, [%l0 + 5 * 16 + 8]

	set win_unf_trap, %l1		! Setup window underflow trap
	ldd [%l1], %l2
	std %l2, [%l0 + 6 * 16]
	ldd [%l1 + 8], %l2
	std %l2, [%l0 + 6 * 16 + 8]

! Try enabling the FPU by setting EF.  If that causes a trap, then we probably
! don't have an FPU.

	ldd [%l0 + 2 * 16], %l4		! Save original trap routine
	set no_fpu_trap, %l1		! Install new one
	ldd [%l1], %l2
	std %l2, [%l0 + 2 * 16]

	mov %psr, %l0
	sethi %hi(0x1000), %l1
	bset %l1, %l0
!	mov %l0, %psr

	std %l4, [2 * 16]		! Restore original trap routine

	call _main
	nop
