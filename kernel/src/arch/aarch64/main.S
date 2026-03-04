
.extern main
.global kmain

kmain:
    adrp x0, stack_top
    mov x1, 1
    msr SPSel, x1
    isb
    mov sp, x0
    isb
    bl main
    

.section .bss
.align 4
stack_base:
    .skip (4096 * 8)
stack_top: