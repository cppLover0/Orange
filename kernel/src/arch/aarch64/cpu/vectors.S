
.extern C_fault_handler
.extern C_irq_handler
.extern C_userspace_fault_handler
.extern C_userspace_irq_handler

_aarch64_irq_handler:
_aarch64_fault_handler:
    msr daifset, #2 
    sub sp, sp, #280
    str x0, [sp, #24]
    str x1, [sp, #32]
    str x2, [sp, #40]
    str x3, [sp, #48]
    str x4, [sp, #56]
    str x5, [sp, #64]
    str x6, [sp, #72]
    str x7, [sp, #80]
    str x8, [sp, #88]
    str x9, [sp, #96]
    str x10, [sp, #104]
    str x11, [sp, #112]
    str x12, [sp, #120]
    str x13, [sp, #128]
    str x14, [sp, #136]
    str x15, [sp, #144]
    str x16, [sp, #152]
    str x17, [sp, #160]
    str x18, [sp, #168]
    str x19, [sp, #176]
    str x20, [sp, #184]
    str x21, [sp, #192]
    str x22, [sp, #200]
    str x23, [sp, #208]
    str x24, [sp, #216]
    str x25, [sp, #224]
    str x26, [sp, #232]
    str x27, [sp, #240]
    str x28, [sp, #248]
    str x29, [sp, #256]
    str x30, [sp, #264]
    mov x0, sp
    add x0, x0, #280
    str x0, [sp, #8]
    mrs x0, elr_el1
    str x0, [sp, #0]
    mrs x0, spsr_el1
    str x0, [sp, #16]
    mov x0, sp
    bl C_fault_handler
    ldr x0, [sp, #0]        
    msr elr_el1, x0
    ldr x0, [sp, #16]       
    msr spsr_el1, x0
    ldr x0, [sp, #24]
    ldr x1, [sp, #32]
    ldr x2, [sp, #40]
    ldr x3, [sp, #48]
    ldr x4, [sp, #56]
    ldr x5, [sp, #64]
    ldr x6, [sp, #72]
    ldr x7, [sp, #80]
    ldr x8, [sp, #88]
    ldr x9, [sp, #96]
    ldr x10, [sp, #104]
    ldr x11, [sp, #112]
    ldr x12, [sp, #120]
    ldr x13, [sp, #128]
    ldr x14, [sp, #136]
    ldr x15, [sp, #144]
    ldr x16, [sp, #152]
    ldr x17, [sp, #160]
    ldr x18, [sp, #168]
    ldr x19, [sp, #176]
    ldr x20, [sp, #184]
    ldr x21, [sp, #192]
    ldr x22, [sp, #200]
    ldr x23, [sp, #208]
    ldr x24, [sp, #216]
    ldr x25, [sp, #224]
    ldr x26, [sp, #232]
    ldr x27, [sp, #240]
    ldr x28, [sp, #248]
    ldr x29, [sp, #256]
    ldr x30, [sp, #264]
    add sp, sp, #280
    msr daifclr, #2
    eret

_aarch64_userspace_irq_handler:
_aarch64_userspace_fault_handler:
    msr daifset, #2 
    sub sp, sp, #280
    str x0, [sp, #24]
    str x1, [sp, #32]
    str x2, [sp, #40]
    str x3, [sp, #48]
    str x4, [sp, #56]
    str x5, [sp, #64]
    str x6, [sp, #72]
    str x7, [sp, #80]
    str x8, [sp, #88]
    str x9, [sp, #96]
    str x10, [sp, #104]
    str x11, [sp, #112]
    str x12, [sp, #120]
    str x13, [sp, #128]
    str x14, [sp, #136]
    str x15, [sp, #144]
    str x16, [sp, #152]
    str x17, [sp, #160]
    str x18, [sp, #168]
    str x19, [sp, #176]
    str x20, [sp, #184]
    str x21, [sp, #192]
    str x22, [sp, #200]
    str x23, [sp, #208]
    str x24, [sp, #216]
    str x25, [sp, #224]
    str x26, [sp, #232]
    str x27, [sp, #240]
    str x28, [sp, #248]
    str x29, [sp, #256]
    str x30, [sp, #264]
    mrs x0, sp_el0
    str x0, [sp, #8]
    mrs x0, elr_el1
    str x0, [sp, #0]
    mrs x0, spsr_el1
    str x0, [sp, #16]
    mov x0, sp
    bl C_fault_handler
    ldr x0, [sp, #0]        
    msr elr_el1, x0
    ldr x0, [sp, #16]       
    msr spsr_el1, x0
    ldr x0, [sp, #8]        
    msr sp_el0, x0
    ldr x0, [sp, #24]
    ldr x1, [sp, #32]
    ldr x2, [sp, #40]
    ldr x3, [sp, #48]
    ldr x4, [sp, #56]
    ldr x5, [sp, #64]
    ldr x6, [sp, #72]
    ldr x7, [sp, #80]
    ldr x8, [sp, #88]
    ldr x9, [sp, #96]
    ldr x10, [sp, #104]
    ldr x11, [sp, #112]
    ldr x12, [sp, #120]
    ldr x13, [sp, #128]
    ldr x14, [sp, #136]
    ldr x15, [sp, #144]
    ldr x16, [sp, #152]
    ldr x17, [sp, #160]
    ldr x18, [sp, #168]
    ldr x19, [sp, #176]
    ldr x20, [sp, #184]
    ldr x21, [sp, #192]
    ldr x22, [sp, #200]
    ldr x23, [sp, #208]
    ldr x24, [sp, #216]
    ldr x25, [sp, #224]
    ldr x26, [sp, #232]
    ldr x27, [sp, #240]
    ldr x28, [sp, #248]
    ldr x29, [sp, #256]
    ldr x30, [sp, #264]
    add sp, sp, #280
    msr daifclr, #2
    eret

.globl int_table
.balign 0x800

int_table:
sync:
    b _aarch64_fault_handler
.balign 0x80
irq:
    b _aarch64_irq_handler
.balign 0x80
fiq:
    b .
.balign 0x80
.sp0:
    b .

.balign 0x80
sync_ker:
    b _aarch64_fault_handler
.balign 0x80
irq_ker:
    b _aarch64_irq_handler
.balign 0x80
fiq_ker:
    b .
.balign 0x80
ker_sp:
    b .

.balign 0x80
sync_user:
    b _aarch64_userspace_fault_handler
.balign 0x80
irq_user:
    b _aarch64_userspace_irq_handler
.balign 0x80
fiq_user:
    b .
.balign 0x80
sp_user:
    b .

.balign 0x80
_32_sync:
    b .
.balign 0x80
_32_irq:
    b .
.balign 0x80
_32_fiq:
    b .
.balign 0x80
_32_sp:
    b .