
extern schedulingSchedule

global schedulingStub
schedulingStub:

    cmp byte [rsp + 8],0x08
    jz .continue
    swapgs
.continue:

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
    cld
    jmp schedulingSchedule