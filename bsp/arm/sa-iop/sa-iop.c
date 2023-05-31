/*
 * sa-iop.c -- Support for Intel(R) SA-IOP Evaluation Board
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
 * ARM is a Registered Trademark of Advanced RISC Machines Limited.
 * Other Brands and Trademarks are the property of their respective owners.
 */

#include __BOARD_HEADER__
#include <bsp_if.h>

/*
 * Array of memory region descriptors. We just list RAM and FLASH.
 */
struct bsp_mem_info _bsp_memory_list[] = 
{
    { (void *)0x00000000, (void *)0x00000000, 0, 0, BSP_MEM_RAM }
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
    int led;

    if ((which < 0) || (which > 6))
        return;

    /*
     * Select the right LED
     */
    led = SA_IOP_SOFT_IO_LED(which);

    while (n--)
    {
        int i;

        /*
         * Turn on the LED
         */
        *SA_IOP_SOFT_IO_REGISTER |= led;

        i = 0xffff; while (--i);

        /*
         * Turn of the LED
         */
        *SA_IOP_SOFT_IO_REGISTER &= ~led;

        i = 0xffff; while (--i);
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
    extern void _bsp_init_sa110_comm(void);
    _bsp_init_sa110_comm();
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
    _bsp_platform_info.board = "Intel(R) SA-IOP Evaluation Board";

    /*
     * Finish setup of RAM description. Early initialization put a
     * pointer to the top of RAM in _bsp_ram_info_ptr.
     */
    _bsp_memory_list[0].nbytes = (long)_bsp_ram_info_ptr;
}

/*
 * Initialize the mmu and Page Tables
 * The MMU is actually turned on by the caller of this function.
 */
void
_bsp_mmu_init(int sdram_size)
{
    unsigned long ttb_base = (unsigned long)&page1;
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
     * We only do direct mapping for the IOP board. That is, all
     * virt_addr == phys_addr.
     */

    /*
     * Actual Base = 0x000(00000)
     * Virtual Base = 0x000(00000)
     * Size = Max SDRAM
     * SDRAM
     */
    for (i = 0x000; i < (sdram_size >> 20); i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x400(00000)
     * Virtual Base = 0x400(00000)
     * Size = 1M
     * 21285 Registers
     */
    ARM_MMU_SECTION(ttb_base, 0x400, 0x400,
		    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
		    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x410(00000) - 0x413(FFFFF)
     * Virtual Base = 0x410(00000) - 0x413(FFFFF)
     * Size = 4M
     * FLASH ROM
     */
    for (i = 0x410; i <= 0x413; i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_CACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x420(00000)
     * Virtual Base = 0x420(00000)
     * Size = 1M
     * 21285 CSR Space
     */
    ARM_MMU_SECTION(ttb_base, 0x420, 0x420, 
                    ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                    ARM_ACCESS_PERM_RW_RW);

    /*
     * Actual Base = 0x500(00000)-0x50F(FFFFF)
     * Virtual Base = 0x500(00000)-0x50F(FFFFF)
     * Size = 16M
     * Zeros (Cache Clean) Bank
     */
    for (i = 0x500; i <= 0x50F; i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_CACHEABLE, ARM_BUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x780(00000)-0x78F(FFFFF)
     * Virtual Base = 0x780(00000)-0x78F(FFFFF)
     * Size = 16M
     * Outbound Write Flush
     */
    for (i = 0x780; i <= 0x78F; i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x790(00000)-0x7C0(FFFFF)
     * Virtual Base = 0x790(00000)-0x7C0(FFFFF)
     * Size = 65M
     * PCI IACK/Config/IO Space
     */
    for (i = 0x790; i <= 0x7C0; i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }

    /*
     * Actual Base = 0x800(00000) - 0xFFF(FFFFF)
     * Virtual Base = 0x800(00000) - 0xFFF(FFFFF)
     * Size = 2G
     * PCI Memory Space
     */
    for (i = 0x800; i <= 0xFFF; i++) {
        ARM_MMU_SECTION(ttb_base, i, i,
                        ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
                        ARM_ACCESS_PERM_RW_RW);
    }
}
