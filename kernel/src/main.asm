
section .text

extern main
global kmain
kmain:
    mov rsp,stack_top
    call main
    hlt

section .data
stack_bottom:
resb 1024*128
stack_top: