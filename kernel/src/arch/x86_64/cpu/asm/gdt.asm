global loadgdt
loadgdt:
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

global loadtss
loadtss:
    mov ax,0x28
    ltr ax
    ret