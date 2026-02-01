

global syscall_handler
extern syscall_handler_c
syscall_handler:
    swapgs 
    mov qword [gs:0],rsp
    mov rsp, qword [gs:8]
    push qword (0x18 | 3)
    push qword [gs:0]
    push qword r11
    push qword (0x20 | 3)
    push qword rcx
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
    call syscall_handler_c
    pop rax
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
    cli
    mov rsp, qword [gs:0] 
    swapgs
    o64 sysret

section .rodata
