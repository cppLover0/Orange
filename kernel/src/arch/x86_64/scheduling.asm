
global context_switch
context_switch:
    mov rsp, rdi 
    pop rax
    mov cr3,rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11 
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp,16
    iretq

extern scheduler_timer_asm
global yield
yield:
    pop rax
    mov rdx, rsp
    mov rsp,[gs:16]
    push qword 0
    push rdx
    pushfq
    push qword 0x08
    push qword rax
    jmp scheduler_timer_asm