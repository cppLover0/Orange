OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x100000; 

    .text : {
        *(.text)          
    }

    .rodata : {
        *(.rodata)      
    }

    .data : {
        *(.data)       
    }

    .bss : {
        *(.bss)          
        *(COMMON)       
    }

    . = ALIGN(8);
    _end = .;    
}