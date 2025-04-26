

global schedulingEnd
schedulingEnd:
    cli
    mov rsp,rdi

    mov cr3,rsi

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
    add rsp,16

    cmp byte [rsp + 8],0x08
    jz .end
    swapgs
.end:
    iretq
