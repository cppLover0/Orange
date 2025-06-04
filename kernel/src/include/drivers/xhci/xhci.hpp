
#include <stdint.h>

#pragma once

typedef struct {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparms2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    uint32_t usbcmd;      
    uint32_t usbsts;     
    uint32_t pagesize;    
    uint32_t reserved1[3]; 
    uint32_t dnctrl;     
    uint32_t crcr;    
    uint32_t reserved2[6];
    uint32_t dcbaap;     
    uint32_t config;     
} __attribute__((packed)) xhci_op_regs_t;

#define XHCI_USBCMD_RS (1 << 0)
#define XHCI_USBCMD_HOSTCONTROLLERREST (1 << 1)


typedef struct {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
} __attribute__((packed)) xhci_port_regs_t;

typedef struct {
    uint32_t mfindex;     
    uint32_t reserved1[7]; 

    struct {
        uint32_t iman;   
        uint32_t imod;    
        uint32_t erstsz; 
        uint32_t erstba;   
        uint32_t erdp;  
    } __attribute__((packed)) int_regs[1024]; 
} __attribute__((packed)) xhci_runtime_regs_t;


typedef struct xhci_device {
    uint64_t xhci_phys_base;
    uint64_t xhci_virt_base;
    xhci_cap_regs_t* cap;
    xhci_op_regs_t* op;
    xhci_port_regs_t* port;
    xhci_runtime_regs_t* runtime;
    struct xhci_device* next;
} __attribute__((packed)) xhci_device_t;


class XHCI {
public:
    static void Init();
};