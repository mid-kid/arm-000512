MEMORY
{
  DATAMEM (AW)  : ORIGIN = 0x20000, LENGTH = 32K
  TEXTMEM (ARX) : ORIGIN = 0x10000, LENGTH = 32K
}

/* Map should be:

           SIZE    VMA    LMA
   .bss1     10  20000  20000
   .bss2     30  20000  20010
   .bss3     20  20000  20040
   .mbss    230  20030  20060

   .mtext    20  10000  30000
   .text1    80  10020  30020
   .text2    40  10020  300a0
   .text3    20  10020  300e0

   .data1    30  20260  30100
   .data2    40  20260  30130
   .data3    50  20260  30170  */

SECTIONS
{
  OVERLAY : 
    {
      .bss1 { *(.bss1) }
      .bss2 { *(.bss2) }
      .bss3 { *(.bss3) }
    } > DATAMEM

  .mtext : AT (0x30000) { *(.mtext) } > TEXTMEM

  .mbss : AT (__load_stop_bss3)
    {
      *(.mbss)
      . += 0x200;
    } > DATAMEM

  OVERLAY : AT (LOADADDR (.mtext) + SIZEOF (.mtext))
    {
      .text1 { *(.text1) }
      .text2 { *(.text2) }
      .text3 { *(.text3) }
    } > TEXTMEM

  OVERLAY : AT (LOADADDR (.text3) + SIZEOF (.text3))
    {
      .data1 { *(.data1) }
      .data2 { *(.data2) }
      .data3 { *(.data3) }
    } > DATAMEM

  . = 0x8000;
}
