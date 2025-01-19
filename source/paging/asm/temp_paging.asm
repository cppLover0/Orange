
bits 32

tempPagingMap1G:
    mov eax,ebx
    mul ecx
    or eax,0b10000011
    mov [edi + (ecx * 8)],eax
    inc ecx
    cmp ecx,512
    jne tempPagingMap1G
    jz .end
 .end:
    ret

global setupTempPaging
setupTempPaging:

    mov eax,p3_table
    or eax,0b11
    mov [p4_table],eax

    mov eax,p2_table
    or eax,0b11
    mov [p3_table],eax

    mov eax,p2_table2
    or eax,0b11
    mov [p3_table + 8],eax

    mov eax,p2_table3
    or eax,0b11
    mov [p3_table + 16],eax

    mov eax,p2_table4
    or eax,0b11
    mov [p3_table + 24],eax

    mov ebx,0x200000
    mov edi,p2_table
    mov ecx,0
    call tempPagingMap1G

    mov ebx,0x400000
    mov edx,p2_table2
    mov ecx,0
    call tempPagingMap1G

    mov ebx,0x600000
    mov edx,p2_table3
    mov ecx,0
    call tempPagingMap1G

    mov ebx,0x800000
    mov edx,p2_table4
    mov ecx,0
    call tempPagingMap1G

    mov eax,p4_table
    mov cr3,eax

    mov eax,cr4
    or eax,1 << 5
    mov cr4,eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
p2_table2:
    resb 4096
p2_table3:
    resb 4096
p2_table4:
    resb 4096