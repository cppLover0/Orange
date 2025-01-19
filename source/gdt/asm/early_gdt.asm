
global setupEarlyGDT
setupEarlyGDT:
    lgdt [earlyGDT.p]
    ret

section .data
earlyGDT:
    dq 0
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
 .p:
    dw $ - earlyGDT  - 1
    dq earlyGDT
