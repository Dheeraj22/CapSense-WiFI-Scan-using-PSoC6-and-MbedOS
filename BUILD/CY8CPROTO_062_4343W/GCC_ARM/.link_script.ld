OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)
ENTRY(Reset_Handler)
STACK_SIZE = 1024;
EXTERN(Reset_Handler)
MEMORY
{
    ram (rwx) : ORIGIN = 0x08002000, LENGTH = 0x000FD800
    flash (rx) : ORIGIN = 0x10002000, LENGTH = 0x001FE000
    em_eeprom (rx) : ORIGIN = 0x14000000, LENGTH = 0x8000
    sflash_user_data (rx) : ORIGIN = 0x16000800, LENGTH = 0x800
    sflash_nar (rx) : ORIGIN = 0x16001A00, LENGTH = 0x200
    sflash_public_key (rx) : ORIGIN = 0x16005A00, LENGTH = 0xC00
    sflash_toc_2 (rx) : ORIGIN = 0x16007C00, LENGTH = 0x200
    sflash_rtoc_2 (rx) : ORIGIN = 0x16007E00, LENGTH = 0x200
    xip (rx) : ORIGIN = 0x18000000, LENGTH = 0x8000000
    efuse (r) : ORIGIN = 0x90700000, LENGTH = 0x100000
}
GROUP(libgcc.a libc.a libm.a libnosys.a)
SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        __Vectors = . ;
        KEEP(*(.vectors))
        . = ALIGN(4);
        __Vectors_End = .;
        __Vectors_Size = __Vectors_End - __Vectors;
        __end__ = .;
        . = ALIGN(4);
        *(.text*)
        KEEP(*(.init))
        KEEP(*(.fini))
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)
        *(.rodata .rodata.* .constdata .constdata.* .conststring .conststring.*)
        KEEP(*(.eh_frame*))
    } > flash
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > flash
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > flash
    __exidx_end = .;
    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;
        LONG (__Vectors)
        LONG (__ram_vectors_start__)
        LONG (__Vectors_End - __Vectors)
        LONG (__etext)
        LONG (__data_start__)
        LONG (__data_end__ - __data_start__)
        __copy_table_end__ = .;
    } > flash
    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG (__bss_start__)
        LONG (__bss_end__ - __bss_start__)
        __zero_table_end__ = .;
    } > flash
    __etext = . ;
    .ramVectors (NOLOAD) : ALIGN(8)
    {
        __ram_vectors_start__ = .;
        KEEP(*(.ram_vectors))
        __ram_vectors_end__ = .;
    } > ram
    .data __ram_vectors_end__ : AT (__etext)
    {
        __data_start__ = .;
        *(vtable)
        *(.data*)
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);
        KEEP(*(.jcr*))
        . = ALIGN(4);
        KEEP(*(.cy_ramfunc*))
        . = ALIGN(4);
        __data_end__ = .;
    } > ram
    .noinit (NOLOAD) : ALIGN(8)
    {
      KEEP(*(.noinit))
    } > ram
    .bss (NOLOAD):
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > ram
    .heap (NOLOAD):
    {
        __HeapBase = .;
        __end__ = .;
        end = __end__;
        KEEP(*(.heap*))
        __HeapLimit = .;
    } > ram
    __StackTop = ORIGIN(ram) + LENGTH(ram);
    __StackLimit = __StackTop - STACK_SIZE;
    PROVIDE(__stack = __StackTop);
    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
    .cy_app_signature ORIGIN(flash) + LENGTH(flash) - 256 :
    {
        KEEP(*(.cy_app_signature))
    } > flash
    .cy_em_eeprom :
    {
        KEEP(*(.cy_em_eeprom))
    } > em_eeprom
    .cy_sflash_user_data :
    {
        KEEP(*(.cy_sflash_user_data))
    } > sflash_user_data
    .cy_sflash_nar :
    {
        KEEP(*(.cy_sflash_nar))
    } > sflash_nar
    .cy_sflash_public_key :
    {
        KEEP(*(.cy_sflash_public_key))
    } > sflash_public_key
    .cy_toc_part2 :
    {
        KEEP(*(.cy_toc_part2))
    } > sflash_toc_2
    .cy_rtoc_part2 :
    {
        KEEP(*(.cy_rtoc_part2))
    } > sflash_rtoc_2
    .cy_xip :
    {
        KEEP(*(.cy_xip))
    } > xip
    .cy_efuse :
    {
        KEEP(*(.cy_efuse))
    } > efuse
    .cymeta 0x90500000 : { KEEP(*(.cymeta)) } :NONE
}
__cy_memory_0_start = 0x10000000;
__cy_memory_0_length = 0x00200000;
__cy_memory_0_row_size = 0x200;
__cy_memory_1_start = 0x14000000;
__cy_memory_1_length = 0x8000;
__cy_memory_1_row_size = 0x200;
__cy_memory_2_start = 0x16000000;
__cy_memory_2_length = 0x8000;
__cy_memory_2_row_size = 0x200;
__cy_memory_3_start = 0x18000000;
__cy_memory_3_length = 0x08000000;
__cy_memory_3_row_size = 0x200;
__cy_memory_4_start = 0x90700000;
__cy_memory_4_length = 0x100000;
__cy_memory_4_row_size = 1;
