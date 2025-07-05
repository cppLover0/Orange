
extern schedulingSchedule

global schedulingScheduleStack
schedulingScheduleStack:
    mov rsp,qword [gs:8]
    jmp schedulingSchedule

global schedulingStub
schedulingStub:

    cli
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
    mov rbp,0
    jmp schedulingSchedule