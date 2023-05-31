/*
 * breakpoint.c -- Breakpoint generation.
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
 */

#include <bsp/cpu.h>
#include <bsp/bsp.h>

#ifndef DEBUG_BREAKPOINT
#define DEBUG_BREAKPOINT 0
#endif

/*
 *  Trigger a breakpoint exception.
 */
void
bsp_breakpoint(void)
{
#if DEBUG_BREAKPOINT
    bsp_printf("Before BP\n");
#endif
#ifdef __NEED_UNDERSCORE__
    asm volatile (".globl _bsp_breakinsn\n"
		  "_bsp_breakinsn:\n");
#else
    asm volatile (".globl bsp_breakinsn\n"
		  "bsp_breakinsn:\n");
#endif
    BREAKPOINT();
#if DEBUG_BREAKPOINT
    bsp_printf("After BP\n");
#endif
}

