
%macro isr_err_stub 1
isr_stub_%+%1:
    cli
    mov rax,%+%1
    push rax
    jmp asmException
    cli
    hlt 
%endmacro

%macro isr_no_err_stub 1
isr_stub_%+%1:
    cli
    push qword 0
    mov rax,%+%1
    push rax
    jmp asmException
    cli
    hlt
%endmacro

global ignoreStub
ignoreStub:
    iretq

extern CPUKernelPanic
asmException:
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
    jmp CPUKernelPanic

isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31

global isrTable
isrTable:
%assign i 0 
%rep    32 
    dq isr_stub_%+i 
%assign i i+1 
%endrep