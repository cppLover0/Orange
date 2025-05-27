
extern kmain
global kmain_s
kmain_s:
    cli
    mov rsp,stack_end
    jmp kmain

section .bss

stack_start:
resb 1024*1024*128
stack_end: