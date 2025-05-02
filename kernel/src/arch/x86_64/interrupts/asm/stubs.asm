
%macro isr_err_stub 1
isr_stub_%+%1:
    cli
    cmp byte [rsp + 8],0x08
    jz .cont
    swapgs
.cont:

    push qword (%+%1)
    jmp asmException
    cli
    hlt 
%endmacro

%macro isr_no_err_stub 1
isr_stub_%+%1:
    cli
    cmp byte [rsp + 8],0x08
    jz .cont
    swapgs
.cont:

    push qword 0
    push qword (%+%1)
    jmp asmException
    cli
    hlt
%endmacro

global ignoreStub
ignoreStub:
    iretq

extern keyHandler

global keyStub
keyStub:
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
    call keyHandler
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
    jz .cont2
    swapgs


.cont2:
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