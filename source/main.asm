bits 32
section .text
    align 4
    dd 0x1BADB002
    dd 0
    dd - (0x1BADB002 + 0)

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