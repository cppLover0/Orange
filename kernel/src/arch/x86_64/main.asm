
extern main
global kmain

kmain:
    cli
    mov rsp, stack_top
    jmp main

section .bss
stack_bottom:
resb 1024 * 128
stack_top: