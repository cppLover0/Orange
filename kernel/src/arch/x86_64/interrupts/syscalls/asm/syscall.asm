
extern c_syscall_handler

global syscall_handler
syscall_handler:
    swapgs
    mov qword [gs:24],rax

    mov rax,cr3
    mov qword [gs:32], rax

    mov rax, qword [gs:16]
    mov cr3,rax
    mov rax, qword [gs:24]
    mov qword [gs:0],rsp ;save stack
    mov rsp, qword [gs:8]
    push qword (0x20 | 3)
    push qword [gs:0]
    push qword r11
    push qword (0x18 | 3)
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
    mov rax, qword [gs:32]
    push rax
    xor rbp,rbp
    mov rdi,rsp
    call c_syscall_handler
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
    mov rsp,[gs:0] 
    mov qword [gs:24],rax

    mov rax,qword [gs:32]
    mov cr3,rax
    mov rax, qword [gs:24]

    swapgs
    o64 sysret

global syscall_end
syscall_end:
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
    add rsp,40
    pop rsp
    swapgs
    o64 sysret

global syscall_read_stage_2_asm
extern syscall_read_stage_2

syscall_read_stage_2_asm:
    mov rsp,rdi
    mov rdi,rsi
    mov rsi,rdx
    jmp syscall_read_stage_2

global syscall_waitpid_stage2_asm
extern syscall_waitpid_stage2 
syscall_waitpid_stage2_asm:
    mov rsp,rdi
    mov rdi,rsi
    mov rsi,rdx
    jmp syscall_waitpid_stage2
    
global syscall_write_stage2_asm
extern syscall_write_stage2
syscall_write_stage2_asm:
    mov rsp,rdi
    mov rdi,rsi
    mov rsi,rdx
    mov rdx,rcx
    jmp syscall_write_stage2
