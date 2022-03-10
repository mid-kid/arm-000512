ENTRY(_main)

SECTIONS 
{
.text 0x0a004000:  { *(.text); }
.data . : { *(.data); }
.bss . : { *(.bss); }
}
