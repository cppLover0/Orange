
extern schedulingSchedule
global schedulingEnter
global schedulingEnd

schedulingEnter:
    cli
    cmp byte [rsp + 8],0x08
    jz .cont
    swapgs
.cont:
    push qword 0
    push qword 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    mov rax,cr3
    push rax
    mov rdi,rsp
    jmp schedulingSchedule

schedulingEnd:
    mov rsp,rdi
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

    cmp byte [rsp + 8],0x08
    jz .end
    swapgs
.end:
    iretq