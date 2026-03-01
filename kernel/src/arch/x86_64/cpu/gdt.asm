global load_gdt
load_gdt:
    lgdt [rdi]
    mov ax,0
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov ss,ax
    pop rdi
    mov rax,0x08
    push rax
    push rdi
    retfq

global load_tss
load_tss:
    mov ax,0x28
    ltr ax
    ret