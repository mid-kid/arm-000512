/*
 * sa1100mm.c -- Support for
 *               Intel(R) SA-1100 Multimedia Development Board
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
    {(void *)ROM_VIRTUAL_BASE,   (void *)ROM_VIRTUAL_BASE,   0, ROM_TOTAL_SIZE,   BSP_MEM_ROM  },
    {(void *)FLASH_VIRTUAL_BASE, (void *)FLASH_VIRTUAL_BASE, 0, FLASH_TOTAL_SIZE, BSP_MEM_FLASH},
    {(void *)RAM_VIRTUAL_BASE,   (void *)RAM_VIRTUAL_BASE,   0, RAM_TOTAL_SIZE,   BSP_MEM_RAM  },
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
    unsigned long led = 0;
    volatile unsigned long *led_reg = 0;

    /*
     * Select the right LED
     */
    if ((which >= 0) && (which <= 3))
    {
        /*
         * These led's show up on the Discrete LED register
         */
        led = 1 << which;
        led_reg = SA1100MM_DISCRETE_LED_REGISTER;
    } else if (which <= 7) {
        /*
         * These led's show up on the Keypad I/O register
         */
        led = 1 << (which - 4);
        led_reg = SA1100MM_KEYPAD_IO_REGISTER;
    } else if (which == 8) {
        /*
         * This is the Hex LED
         */
    } else {
        /*
         * Unknown LED value
         */
        return;
    }

    if (which != 8)
    {
        /*
         * Flash one of the discrete LED's
         */
        while (n--)
        {
            int i;

            /*
             * Turn on the LED
             */
            *led_reg = ~led;

            /*
             * Delay
             */
            i = 0x7fffff; while (--i);

            /*
             * Turn off the LED
             */
            *led_reg = 0xF;

            /*
             * Delay
             */
            i = 0x7fffff; while (--i);
        }
    } else {
        /*
         * Put N onto the Hex LED
         */
        {
            unsigned long unibble = (n >> 4) & SA1100MM_HEX_DATA_MASK;
            unsigned long lnibble = (n >> 0) & SA1100MM_HEX_DATA_MASK;

            /*
             * Display First 4 bits on the Right LEDs
             */
            unibble |= SA1100MM_HEX_LED_BOTH_STROBES;
            *SA1100MM_HEX_LED_REGISTER = unibble;
            
            unibble &= ~SA1100MM_HEX_LED_0_STROBE;
            *SA1100MM_HEX_LED_REGISTER = unibble;

            unibble |= SA1100MM_HEX_LED_BOTH_STROBES;
            *SA1100MM_HEX_LED_REGISTER = unibble;

            /*
             * Display Second 4 bits on the Left LEDs
             */
            lnibble |= SA1100MM_HEX_LED_BOTH_STROBES;
            *SA1100MM_HEX_LED_REGISTER = lnibble;

            lnibble &= ~SA1100MM_HEX_LED_1_STROBE;
            *SA1100MM_HEX_LED_REGISTER = lnibble;

            lnibble |= SA1100MM_HEX_LED_BOTH_STROBES;
            *SA1100MM_HEX_LED_REGISTER = lnibble;
        }
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
     _bsp_platform_info.board = "Intel(R) SA-1100 Multimedia Development Board";
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
    unsigned long ttb_base = ((unsigned long)&page1) + (RAM_ACTUAL_BASE - RAM_VIRTUAL_BASE);
    unsigned long i;

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
     * Size = 1M (Only 64K used)
     * Boot flash ROMspace
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
     * Actual Base = 0x100(00000) - 0x180(00000)
     * Virtual Base = 0x100(00000) - 0x180(00000)
     * Size = 128M
     * SA-1101 Development Board Registers
     */
    for (i = 0x100; i < 0x180; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x180(00000)
     * Virtual Base = 0x180(00000)
     * Size = 1M (Only 16 registers (8 bits wide) used)
     * Ct8020 DSP
     */
    ARM_MMU_SECTION(ttb_base, 0x180, 0x180, 
                    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x184(00000)
     * Virtual Base = 0x184(00000)
     * Size = 1M (Only 1 register (2 bits wide) used)
     * XBusReg
     */
    ARM_MMU_SECTION(ttb_base, 0x184, 0x184, 
                    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x188(00000)
     * Virtual Base = 0x188(00000)
     * Size = 1M (Only 4 registers (8 bits wide) used)
     * SysRegA
     */
    ARM_MMU_SECTION(ttb_base, 0x188, 0x188, 
                    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x18C(00000)
     * Virtual Base = 0x18C(00000)
     * Size = 1M (Only 4 registers (8 bits wide) used)
     * SysRegB
     */
    ARM_MMU_SECTION(ttb_base, 0x18C, 0x18C, 
                    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x190(00000) - 0x194(00000)
     * Virtual Base = 0x190(00000) - 0x194(00000)
     * Size = 4M
     * Spare CPLD A
     */
    for (i = 0x190; i < 0x194; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x194(00000) - 0x198(00000)
     * Virtual Base = 0x194(00000) - 0x198(00000)
     * Size = 4M
     * Spare CPLD B
     */
    for (i = 0x194; i < 0x198; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

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
     * Actual Base = 0xC00(00000) - 0xC08(00000)
     * Virtual Base = 0x000(00000) - 0x008(00000)
     * Size = 8M
     * DRAM Bank 0
     */
    for (i = 0xC00; i < 0xC08; i++)
    {
        ARM_MMU_SECTION(ttb_base, i, i - 0xC00,
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
{
    /*
     * Find out how big the RAM is
     */
#define MEMSTART 0xC0000000
#define MEMEND   0xE0000000
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
}
{
    maxmem = ptr;
    bsp_printf("maxmem = 0x%08lx\n", ptr);
}
#endif /* 0 */
