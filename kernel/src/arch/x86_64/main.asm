
extern kmain
global kmain_s
kmain_s:
    cli
    mov rsp,stack_end
    jmp kmain

global enable_sse
enable_sse:

    push rax
    push rcx
    push rdx

    mov eax, 0x1
    cpuid
    test edx, 1<<25
    jz .no_sse
    mov rax, cr0
    and ax, 0xFFFB	
    or ax, 0x2			
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9		
    mov cr4, rax
 .no_sse:
    pop rdx
    pop rcx
    pop rax
    ret

section .bss

stack_start:
resb 1024*1024*128
stack_end: