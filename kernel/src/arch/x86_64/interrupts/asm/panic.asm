extern CPanic

global Panic 
Panic:
    cli
    push qword 0
    push rsp
    pushfq
    push qword 0
    push qword 0
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
    mov rsi,rsp
    jmp CPanic