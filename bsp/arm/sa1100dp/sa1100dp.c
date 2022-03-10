/*
 * sa1100dp.c -- Support for Intel(R) SA-1100 Microprocessor Evaluation Platform
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
 * Intel is a Registered Trademark of Intel Corporation.
 * StrongARM is a Registered Trademark of Advanced RISC Machines Limited.
 * ARM is a Registered Trademark of Advanced RISC Machines Limited.
 * Other Brands and Trademarks are the property of their respective owners.
 */

#include __BOARD_HEADER__
#include <bsp/defs.h>
#include <bsp_if.h>

/*
 * Array of memory region descriptors. We just list RAM and FLASH.
 */
struct bsp_mem_info _bsp_memory_list[] = 
{
    { (void *)ROM_VIRTUAL_BASE,   (void *)ROM_VIRTUAL_BASE,   0, ROM_TOTAL_SIZE,   BSP_MEM_ROM  },
    { (void *)FLASH_VIRTUAL_BASE, (void *)FLASH_VIRTUAL_BASE, 0, FLASH_TOTAL_SIZE, BSP_MEM_FLASH},
    { (void *)RAM_VIRTUAL_BASE,   (void *)RAM_VIRTUAL_BASE,   0, RAM_TOTAL_SIZE,   BSP_MEM_RAM  },
};

/*
 * Number of memory region descriptors.
 */
int _bsp_num_mem_regions = sizeof(_bsp_memory_list)/sizeof(_bsp_memory_list[0]);

/*
 * Toggle LED for debugging purposes.
 */
void flash_led(int n, int which)
{
    /*
     * Select the right LED
     */
    if ((which == GREEN_LED_0) || (which == GREEN_LED_1) || 
        (which == RED_LED)     || (which == ALL_LEDS))
    {
        /*
         * Make sure the port pin is set for output.
         */
        *SA1100DP_DISCRETE_LED_DIR_REGISTER = BIT27 | which;

        while (n--)
        {
            unsigned long i;
            
            /*
             * Turn on the LED
             */
            *SA1100DP_DISCRETE_LED_SET_REGISTER = BIT27 | which;

            /*
             * Delay
             */
            i = 0x7fffff; while (--i);

            /*
             * Turn off the LED
             */
            *SA1100DP_DISCRETE_LED_CLEAR_REGISTER = BIT27 | which;

            /*
             * Delay
             */
            i = 0x7fffff; while (--i);
        }
    } else if (which == HEX_LED) {
        /*
         * This is the Hex LED
         */
        *SA1100DP_HEX_LED_REGISTER = (0x38010 | (~n & 0xF));
    } else {
        /*
         * Unknown LED value
         */
        return;
    }
}

/*
 * Early initialization of comm channels. Must not rely
 * on interrupts, yet. Interrupt operation can be enabled
 * in _bsp_board_init().
 */
void
_bsp_init_board_comm(void)
{
    extern void _bsp_init_sa1100_comm(void);
    _bsp_init_sa1100_comm();
}


/*
 * Set any board specific debug traps.
 */
void
_bsp_install_board_debug_traps(void)
{
}


/*
 * Install any board specific interrupt controllers.
 */
void
_bsp_install_board_irq_controllers(void)
{
}

/*
 *  Board specific BSP initialization.
 */
void
_bsp_board_init(void)
{
    /*
     * Override default platform info.
     */
     _bsp_platform_info.board = "Intel(R) SA-1100 Microprocessor Evaluation Platform";
}

/*
 * Initialize the mmu and Page Tables
 * The MMU is actually turned on by the caller of this function.
 *
 * Returns: top of remapped memory.
 */
unsigned long *
_bsp_mmu_init(void)
{
    unsigned long ttb_base = ~0;
    unsigned long i, j;

    if ((unsigned long)&page1 < RAM2_VIRTUAL_BASE)
    {
        /*
         * page1 is physically located in RAM Bank 1
         */
        ttb_base = ((unsigned long)&page1) - RAM1_VIRTUAL_BASE + RAM1_ACTUAL_BASE;
    } else if ((unsigned long)&page1 < RAM3_VIRTUAL_BASE) {
        /*
         * page1 is physically located in RAM Bank 2
         */
        ttb_base = ((unsigned long)&page1) - RAM2_VIRTUAL_BASE + RAM2_ACTUAL_BASE;
    } else if ((unsigned long)&page1 < RAM4_VIRTUAL_BASE) {
        /*
         * page1 is physically located in RAM Bank 3
         */
        ttb_base = ((unsigned long)&page1) - RAM3_VIRTUAL_BASE + RAM3_ACTUAL_BASE;
    } else {
        /*
         * page1 is physically located in RAM Bank 4
         */
        ttb_base = ((unsigned long)&page1) - RAM4_VIRTUAL_BASE + RAM4_ACTUAL_BASE;
    }
    BSP_ASSERT((ttb_base & ARM_TRANSLATION_TABLE_MASK) == ttb_base);

    /*
     * Set the TTB register
     */
    __mcr(ARM_CACHE_COPROCESSOR_NUM,
          ARM_COPROCESSOR_OPCODE_DONT_CARE,
          ttb_base,
          ARM_TRANSLATION_TABLE_BASE_REGISTER,
          ARM_COPROCESSOR_RM_DONT_CARE,
          ARM_COPROCESSOR_OPCODE_DONT_CARE);

    /*
     * Set the Domain Access Control Register
     */
    i = ARM_ACCESS_TYPE_MANAGER(0)    | 
        ARM_ACCESS_TYPE_NO_ACCESS(1)  |
        ARM_ACCESS_TYPE_NO_ACCESS(2)  |
        ARM_ACCESS_TYPE_NO_ACCESS(3)  |
        ARM_ACCESS_TYPE_NO_ACCESS(4)  |
        ARM_ACCESS_TYPE_NO_ACCESS(5)  |
        ARM_ACCESS_TYPE_NO_ACCESS(6)  |
        ARM_ACCESS_TYPE_NO_ACCESS(7)  |
        ARM_ACCESS_TYPE_NO_ACCESS(8)  |
        ARM_ACCESS_TYPE_NO_ACCESS(9)  |
        ARM_ACCESS_TYPE_NO_ACCESS(10) |
        ARM_ACCESS_TYPE_NO_ACCESS(11) |
        ARM_ACCESS_TYPE_NO_ACCESS(12) |
        ARM_ACCESS_TYPE_NO_ACCESS(13) |
        ARM_ACCESS_TYPE_NO_ACCESS(14) |
        ARM_ACCESS_TYPE_NO_ACCESS(15);
    __mcr(ARM_CACHE_COPROCESSOR_NUM,
          ARM_COPROCESSOR_OPCODE_DONT_CARE,
          i,
          ARM_DOMAIN_ACCESS_CONTROL_REGISTER,
          ARM_COPROCESSOR_RM_DONT_CARE,
          ARM_COPROCESSOR_OPCODE_DONT_CARE);

    /*
     * First clear all TT entries - ie Set them to Faulting
     */
    memset((void *)ttb_base, 0, ARM_FIRST_LEVEL_PAGE_TABLE_SIZE);

    /*
     * Actual Base = 0x000(00000)
     * Virtual Base = 0x500(00000)
     * Size = 1M (Only 128K used)
     * Boot ROM
     */
    ARM_MMU_SECTION(ttb_base, 0x000, 0x500, 
                    ARM_CACHEABLE, ARM_BUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x080(00000) - 0x084(00000)
     * Virtual Base = 0x080(00000) - 0x084(00000)
     * Size = 4M
     * Application flash ROM
     */
    for (i = 0x080; i < 0x084; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x100(00000)
     * Virtual Base = 0x100(00000)
     * Size = 1M (Only 512K used)
     * SRAM
     */
    ARM_MMU_SECTION(ttb_base, 0x100, 0x100, 
                    ARM_CACHEABLE, ARM_BUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x200(00000) - 0x400(00000)
     * Virtual Base = 0x200(00000) - 0x400(00000)
     * PCMCIA Sockets 0 and 1
     */
    for (i = 0x200; i < 0x400; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0xC00(00000) - 0xC04(00000)
     * Virtual Base = 0x000(00000) - 0x004(00000)
     * Size = 4M
     * DRAM Bank 0
     */
    for (i = 0xC00, j = 0x000; i < 0xC04; i++, j++)
    {
        ARM_MMU_SECTION(ttb_base, i, j,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0xC80(00000) - 0xC84(00000)
     * Virtual Base = 0x004(00000) - 0x008(00000)
     * Size = 4M
     * DRAM Bank 1
     */
    for (i = 0xC80, j = 0x004; i < 0xC84; i++, j++)
    {
        ARM_MMU_SECTION(ttb_base, i, j,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0xD00(00000) - 0xD04(00000)
     * Virtual Base = 0x008(00000) - 0x00C(00000)
     * Size = 4M
     * DRAM Bank 2
     */
    for (i = 0xD00, j = 0x008; i < 0xD04; i++, j++)
    {
        ARM_MMU_SECTION(ttb_base, i, j,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0xD80(00000) - 0xD84(00000)
     * Virtual Base = 0x00C(00000) - 0x010(00000)
     * Size = 4M
     * DRAM Bank 3
     */
    for (i = 0xD80, j = 0x00C; i < 0xD84; i++, j++)
    {
        ARM_MMU_SECTION(ttb_base, i, j,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0xE00(00000)-0xE80(00000)
     * Virtual Base = 0xE00(00000)-0xE80(00000)
     * Size = 128M
     * Zeros (Cache Clean) Bank
     */
    for (i = 0xE00; i < 0xE80; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x800(00000) - 0xC00(00000)
     * Virtual Base = 0x800(00000) - 0xC00(00000)
     * StrongARM(R) Registers
     */
    for (i = 0x800; i < 0xC00; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    return (unsigned long *)(RAM_VIRTUAL_BASE + RAM_TOTAL_SIZE);
}


#if 0
unsigned long maxmem;
void size_mem(void)
{
    /*
     * Find out how big the RAM is
     */
#define MEMSTART 0x100
#define MEMEND   SZ_16M
    register unsigned long *ptr=(unsigned long *)MEMSTART, *end = (unsigned long *)MEMEND;
    register unsigned long data1=0xCAFEBABE, data2=0xDEADBEEF;

    /*
     * Fill all of suspected memory w/ 0xCAFEBABE and 0xDEADBEEF
     */
    while (ptr < end)
    {
        *(ptr+0) = data1;
        *(ptr+1) = data2;
        ptr = (unsigned long *)((unsigned long)ptr + SZ_4K);
    }

    /*
     * Now test all of suspected memory for expected pattern
     */
    ptr = (unsigned long *)MEMSTART;

    while (ptr < end)
    {
        /*
         * Check for 0xCAFEBABE
         */
        if (*(ptr+0) != data1) break;
        *(ptr+0) = 0;

        /*
         * Check for 0xDEADBEEF
         */
        if (*(ptr+1) != data2) break;
        *(ptr+1) = 0;

        ptr = (unsigned long *)((unsigned long)ptr + SZ_4K);
    }

    maxmem = (unsigned long) ptr;
}
#endif /* 0 */
