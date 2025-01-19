ENTRY(_entry)
SECTIONS
 {
   . = 1M;
   .text : { *(.text) }
   .data : { *(.data) }
   .bss  : { *(.bss)  }
 }