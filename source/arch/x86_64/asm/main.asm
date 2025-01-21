bits 32
section .multiboot
multiboot_start:
    dd 0xE85250D6                
    dd 0                       
    dd multiboot_end - multiboot_start
    dd 0x100000000 - (0xE85250D6 + 0 + (multiboot_end - multiboot_start))

    dw 5
    dw 1
    dd 20
    dd 0
    dd 0
    dd 0

    dw 0
    dw 0
    dd 0

multiboot_end:

section .text

global _entry
extern main

extern setupTempPaging
extern setupEarlyGDT

_entry:
    cli
    mov esp,stack
    call setupTempPaging ; map first 4GB
    call setupEarlyGDT
    jmp 0x08:_kernel_entry ; perform far jump to _kernel_entry with long mode

_kernel_entry:
    call main
    hlt

section .bss
resb (1024 * 8)
stack: