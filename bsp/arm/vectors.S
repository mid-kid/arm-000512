/*
 * vectors.S -- Exception vector code for ARM(R) BSP
 *
 * Copyright (c) 1998, 1999 Cygnus Support
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * ARM is a Registered Trademark of Advanced RISC Machines Limited.
 * Other Brands and Trademarks are the property of their respective owners.
 */

#include <bsp/cpu.h>

	.file	"vectors.S"

        .text
	

/*
 *  Create exception frame and switch to SVC mode from a non-SVC mode
 *  exception .
 */
.macro SWITCH_TO_SVC_FROM_NONSVC
	/* save some working regs on mode-specific stack */
        stmfd   sp!, {r0-r1}
        mov     r0, lr       /* pre-exception pc     */
        mrs     r1, spsr     /* pre-exception  cpsr  */
        stmfd   sp!, {r0-r1}
	mov	r0, sp	     /* pointer to stuff we just saved */

	/* switch to SVC mode */
	ldr	r1, =(ARM_PSR_IRQ|ARM_PSR_MODE_SVC)
	msr	cpsr, r1
        stmfd   r0!, {sp}    /* save sp in temp area */

	/***************************************
	 * r0 points to our temp save area:    *
	 *                                     *
	 *  +16:  pre-exception r1             *
	 *  +12:  pre-exception r0             *
	 *   +8:  pre-exception cpsr           *
	 *   +4:  pre-exception pc             *
	 *   +0:  saved sp_svc                 *
	 **************************************/

	/* switch to exception stack */
	ldr	r1, =SYM_NAME(_ex_stack)
	cmp	sp, r1
	subhi	sp, r1, IMM(ARM_EX_REGS_T_SIZE)
	subls	sp, sp, IMM(ARM_EX_REGS_T_SIZE)
	
        stmia   sp, {r0-r7}

	/* fixup regs from temp save area */
	ldmfd	r0, {r2-r6}
	str	r2, [sp, IMM(spsvc_o)]
        str     r3, [sp, IMM(pc_o)]
        str     r4, [sp, IMM(cpsr_o)]
        str     r5, [sp, IMM(r0_o)]
        str     r6, [sp, IMM(r1_o)]

	/* switch to pre-exception mode to get banked regs */
	add	r0, sp, IMM(r8_o)
	mrs	r1, cpsr
	orr	r4, r4, IMM(ARM_PSR_IRQ|ARM_PSR_FIQ)
	msr	cpsr, r4
        stmia   r0, {r8-r12,sp,lr}
	/* back to svc mode */
	msr	cpsr, r1

	/* if pre-exception mode is svc mode, fixup sp in exception frame */
	eor	r1, r1, r4
	ands	r1, r1, IMM(ARM_PSR_MODE_MASK)
	ldreq   r0, [sp, IMM(spsvc_o)]
	streq   r0, [sp, IMM(sp_o)]
.endm


/*
 *  Create exception frame from a SVC mode exception.
 */
.macro SWITCH_TO_SVC_FROM_SVC
	/* save some working regs on current stack */
        stmfd   sp!, {r0-r1,sp}
	mov	r0, sp
	
	/* switch to exception stack and create frame */
	ldr	r1, =SYM_NAME(_ex_stack)
	cmp	sp, r1
	subhi	sp, r1, IMM(ARM_EX_REGS_T_SIZE)
	subls	sp, sp, IMM(ARM_EX_REGS_T_SIZE - 8)
	
	/*
	 * Fill in the exception frame
	 */
	ldr	r1, [r0, IMM(8)]
	str	r1, [sp, IMM(spsvc_o)]
	ldmia	r0, {r0-r1}	
        stmia   sp, {r0-r7}
        mrs     r4, spsr     
	str	r4, [sp, IMM(cpsr_o)] /* pre-exception  cpsr      */
	str	lr, [sp, IMM(pc_o)]   /* exception return address */

	/* switch to pre-exception mode to get banked regs */
	add	r0, sp, IMM(r8_o)
	mrs	r1, cpsr
	orr	r4, r4, IMM(ARM_PSR_IRQ|ARM_PSR_FIQ)
	msr	cpsr, r4
        stmia   r0, {r8-r12,sp,lr}
	/* back to svc mode */
	msr	cpsr, r1

	/* if pre-exception mode is svc mode, fixup sp in exception frame */
	eor	r1, r1, r4
	ands	r1, r1, IMM(ARM_PSR_MODE_MASK)
	ldreq   r0, [sp, IMM(spsvc_o)]
	streq   r0, [sp, IMM(sp_o)]
.endm


/*
 * This is the Undefined instruction interrupt handler
 */
FUNC_START undefined_isr_asm
        /*
         * Set up a temp stack to use
         */
        ldr     sp, =SYM_NAME(_undefined_stack)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_UNDEFINED_INSTRUCTION
	b	__exc_common
	
FUNC_END undefined_isr_asm


/*
 * This is the SWI interrupt handler
 */
FUNC_START swi_isr_asm
	SWITCH_TO_SVC_FROM_SVC
        ldr	r1, =BSP_CORE_EXC_SOFTWARE_INTERRUPT
	b	__exc_common
FUNC_END swi_isr_asm


/*
 * This is the Prefetch Abort interrupt handler
 */
FUNC_START prefetch_abort_isr_asm
        /*
         * Set up a temp stack to use
         */
	ldr     sp, =SYM_NAME(_abort_stack)
	sub	lr, lr, IMM(4)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_PREFETCH_ABORT
	b	__exc_common
FUNC_END prefetch_abort_isr_asm


/*
 * This is the Data Abort interrupt handler
 */
FUNC_START data_abort_isr_asm
        /*
         * Set up a temp stack to use
         */
        ldr     sp, =SYM_NAME(_abort_stack)
	sub	lr, lr, IMM(8)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_DATA_ABORT
	b	__exc_common
FUNC_END data_abort_isr_asm


/*
 * This is the 26-bit mode Address Error interrupt handler
 */
FUNC_START address_err_isr_asm
        /*
         * Set up a temp stack to use
         */
        ldr     sp, =SYM_NAME(_address_err_stack)
	sub	lr, lr, IMM(4)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_ADDRESS_ERROR_26_BIT
	b	__exc_common
FUNC_END address_err_isr_asm

/*
 * This is the IRQ interrupt handler
 */
FUNC_START irq_isr_asm
        /*
         * Set up a temp stack to use
         */
        ldr     sp, =SYM_NAME(_irq_stack)
	sub	lr, lr, IMM(4)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_IRQ
	b	__exc_common
FUNC_END irq_isr_asm

/*
 * This is the FIQ interrupt handler
 */
FUNC_START fiq_isr_asm
        /*
         * Set up a temp stack to use
         */
        ldr     sp, =SYM_NAME(_fiq_stack)
	sub	lr, lr, IMM(4)

	SWITCH_TO_SVC_FROM_NONSVC
        ldr	r1, =BSP_CORE_EXC_FIQ
	b	__exc_common
FUNC_END fiq_isr_asm


/*
 * This is the common exception code. It calls generic_exception_handler
 * then returns to the pre-exception state.
 *
 * On entry, R1 should contain exception type.
 */
FUNC_START __exc_common
        /*
         * void generic_exception_handler(ex_regs_t *regs,
	 *                                unsigned long vect_num)
         */
        mov     r0, sp
        bl      SYM_NAME(generic_exception_handler)

	/* restore pre-exception mode r8-12,sp,lr */
	add	r0, sp, IMM(r8_o)
	mrs	r1, cpsr
	ldr	r4, [sp, IMM(cpsr_o)]
	orr	r3, r4, IMM(ARM_PSR_IRQ|ARM_PSR_FIQ)
	msr	cpsr, r3	/* switch mode */
	ldmia	r0!, {r8-r12}
	ldr	lr, [r0, IMM(4)]

	/* if pre-exception mode is not svc mode, fix sp */
	eor	r3, r3, r1
	ands	r3, r3, IMM(ARM_PSR_MODE_MASK)
	ldrne	sp, [r0]
	msr	cpsr, r1	/* back to svc mode */
	
	ldr	r0, [sp, IMM(spsvc_o)]
	str	r0, [sp, IMM(pc_o) - 4]
	msr	spsr, r4
	ldmia	sp, {r0-r7}
	add	sp, sp, IMM(pc_o) - 4
	ldmia	sp, {sp,pc}^
FUNC_END __exc_common


/*
 * Assembly representing the exception vectors.
 */
        .section .ram_vectors

SYM_NAME(RESET_VECTOR):           b       SYM_NAME(RESET_VECTOR)
SYM_NAME(UNDEFINED_VECTOR):       ldr     pc, SYM_NAME(ROM_UNDEFINED_ISR)
SYM_NAME(SWI_VECTOR):             ldr     pc, SYM_NAME(ROM_SWI_ISR)
SYM_NAME(PREFETCH_ABORT_VECTOR):  ldr     pc, SYM_NAME(ROM_PREFETCH_ABORT_ISR)
SYM_NAME(DATA_ABORT_VECTOR):      ldr     pc, SYM_NAME(ROM_DATA_ABORT_ISR)
SYM_NAME(ADDR_ERROR_VECTOR):      ldr     pc, SYM_NAME(ROM_ADDR_ERROR_ISR)
SYM_NAME(IRQ_VECTOR):             ldr     pc, SYM_NAME(ROM_IRQ_ISR)
SYM_NAME(FIQ_VECTOR):             ldr     pc, SYM_NAME(ROM_FIQ_ISR)

SYM_NAME(ROM_UNDEFINED_ISR):      .word   SYM_NAME(undefined_isr_asm)
SYM_NAME(ROM_SWI_ISR):            .word   SYM_NAME(swi_isr_asm)
SYM_NAME(ROM_PREFETCH_ABORT_ISR): .word   SYM_NAME(prefetch_abort_isr_asm)
SYM_NAME(ROM_DATA_ABORT_ISR):     .word   SYM_NAME(data_abort_isr_asm)
SYM_NAME(ROM_ADDR_ERROR_ISR):     .word   SYM_NAME(address_err_isr_asm)
SYM_NAME(ROM_IRQ_ISR):            .word   SYM_NAME(irq_isr_asm)
SYM_NAME(ROM_FIQ_ISR):            .word   SYM_NAME(fiq_isr_asm)


#ifdef MMU
/*
 * This the the MMU page tables
 */
	.section .arm_page_table
        .global SYM_NAME(page1)
SYM_NAME(page1): .rept ARM_FIRST_LEVEL_PAGE_TABLE_SIZE
                 .byte 0
                 .endr
#endif /* MMU */

/*
 * Macro Definition for an EXCEPTION stack
 */
.macro EXCEPTION_STACK label, size
	.bss
        .global SYM_NAME(\label)
	.align 4
	.rept	\size
	.byte	0
	.endr
SYM_NAME(\label):
.endm


/*
 * Provide a stack for use by exception processing
 */
        EXCEPTION_STACK _ex_stack, 8*1024

/*
 * Provide a simple stack for use in each of the
 * exception modes.  These are pretty small since
 * all we do is immediately switch to the exception
 * stack
 */
#define TMP_STKSIZE 32
        EXCEPTION_STACK _abort_stack, TMP_STKSIZE
        EXCEPTION_STACK _address_err_stack, TMP_STKSIZE
        EXCEPTION_STACK _undefined_stack, TMP_STKSIZE
        EXCEPTION_STACK _irq_stack, TMP_STKSIZE
        EXCEPTION_STACK _fiq_stack, TMP_STKSIZE
