
section .text

extern main
global kmain
kmain:
    mov rsp,stack_top
    call main
    hlt

global setwp
setwp:
    mov rax,cr0
    or rax, 0x10000
    mov cr0,rax
    ret
    

section .data
stack_bottom:
resb 1024*128
stack_top: