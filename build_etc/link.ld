ENTRY(_entry)
SECTIONS
 {
   . = 1M;
   .boot : { *(.multiboot) }
   .text : { *(.text) }
   .data : { *(.data) }
   .bss  : { *(.bss)  }
 }