
#include <stdint.h>

#pragma once

typedef struct {
    uint32_t maxslots : 8;
    uint32_t maxintrs : 11;
    uint32_t reserved1 : 5;
    uint32_t maxports : 8;
} __attribute__((packed)) hcsparams1_t;

typedef struct {
    uint32_t ist : 4;
    uint32_t erstmax : 4;
    uint32_t reserved1 : 13;
    uint32_t max_scratchpad_hi : 5;
    uint32_t spr : 1;
    uint32_t max_scratchpad_lo : 5;
} __attribute__((packed)) hcsparams2_t;

typedef struct {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    hcsparams1_t hcsparams1;
    hcsparams2_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparms2;
} xhci_cap_regs_t;

typedef struct {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t pagesize;
    uint32_t reserved1[2];
    uint32_t dnctrl;
    uint64_t crcr;
    uint32_t reserved2[4];
    uint64_t dcbaap;
    uint32_t config;
} xhci_op_regs_t;

#define XHCI_USBCMD_RS (1 << 0)
#define XHCI_USBCMD_HOSTCONTROLLERREST (1 << 1)

typedef struct {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
} __attribute__((packed)) xhci_port_regs_t;

typedef struct {
    uint64_t erst_segment : 3;
    uint64_t event_busy : 1;
    uint64_t event_ring_pointer : 60;
} __attribute__((packed)) xhci_erdp_t;

typedef struct {
    uint32_t iman;   
    uint32_t imod;    
    uint32_t erstsz; 
    uint64_t erstba;   
    xhci_erdp_t erdp;
} __attribute__((packed)) IR_t;

typedef struct {
    uint32_t mfindex;     
    uint32_t reserved1[7]; 
    IR_t int_regs[1024];
} __attribute__((packed)) xhci_runtime_regs_t;

typedef struct {
    uint64_t base; 
    uint32_t size;         
    uint32_t reserved0;
} __attribute__((packed)) xhci_erst_t;

typedef struct {
    uint64_t base; 
    uint32_t status;    
    uint32_t info;
} xhci_trb_t;

typedef struct {
    uint8_t cycle;
    uint16_t trb_limit;
    uint64_t queue;
    xhci_trb_t* trb;
} __attribute__((packed)) xhci_command_ring_ctx_t;

typedef struct xhci_device {
    uint64_t xhci_phys_base;
    uint64_t xhci_virt_base;
    uint64_t dcbaa; // i can read from op->dcbaap but its will return 0
    uint16_t calculated_scratchpad_count;
    xhci_cap_regs_t* cap;
    xhci_op_regs_t* op;
    xhci_port_regs_t* port;
    xhci_runtime_regs_t* runtime;
    xhci_command_ring_ctx_t* com_ring;
    struct xhci_device* next;
} __attribute__((packed)) xhci_device_t;

class XHCI {
public:
    static void Init();
};