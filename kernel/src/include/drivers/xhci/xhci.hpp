
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
} xhci_port_regs_t;

typedef struct {
    uint64_t erst_segment : 3;
    uint64_t event_busy : 1;
    uint64_t event_ring_pointer : 60;
} xhci_erdp_t;

typedef struct {
    uint32_t iman;   
    uint32_t imod;    
    uint32_t erstsz; 
    uint32_t reserved0;
    uint64_t erstba;   
    union {
        xhci_erdp_t erdp;
        uint64_t erdp_val;
    };
} IR_t;

typedef struct {
    uint32_t mfindex;     
    uint32_t reserved1[7]; 
    IR_t int_regs[1024];
} xhci_runtime_regs_t;

typedef struct {
    uint64_t base; 
    uint32_t size;         
    uint32_t reserved0;
} xhci_erst_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t nexttrb : 1;
    uint32_t interruptonshort : 1;
    uint32_t nosnoop : 1;
    uint32_t chain : 1;
    uint32_t intoncompletion : 1;
    uint32_t immediate : 1;
    uint32_t reserved1 : 2;
    uint32_t blockeventint : 1;
    uint32_t type : 6;
    uint32_t reserved2 : 16;
} xhci_trb_info_t;

typedef struct {
    uint64_t base; 
    uint32_t status;    
    union {
        xhci_trb_info_t info_s;
        uint32_t info;
    };
} xhci_trb_t;

typedef struct {
    uint8_t cycle;
    uint16_t trb_limit;
    uint64_t queue;
    xhci_trb_t* trb;
} xhci_command_ring_ctx_t;

typedef struct {
    uint16_t trb_limit;
    uint64_t queue;
    uint8_t cycle;
    IR_t* father;
    xhci_trb_t* trb;
    xhci_erst_t* table;
} xhci_event_ring_ctx_t;

typedef struct xhci_device {
    uint64_t xhci_phys_base;
    uint64_t xhci_virt_base;
    uint64_t dcbaa; // i can read from op->dcbaap but its will return 0
    uint16_t calculated_scratchpad_count;
    xhci_cap_regs_t* cap;
    xhci_op_regs_t* op;
    xhci_port_regs_t* port;
    xhci_runtime_regs_t* runtime;
    uint32_t* doorbell;
    xhci_command_ring_ctx_t* com_ring;
    xhci_event_ring_ctx_t* event_ring;
    struct xhci_device* next;
} __attribute__((packed)) xhci_device_t;

#define TRB_NORMAL_TYPE 1 
#define TRB_SETUPSTAGE_TYPE 2
#define TRB_DATASTAGE_TYPE 3
#define TRB_STATUSSTAGE_TYPE 4
#define TRB_ISOCH_TYPE 5
#define TRB_LINK_TYPE 6
#define TRB_EVENTDATA_TYPE 7
#define TRB_NOOP_TYPE 8
#define TRB_ENABLESLOTCOMMAND_TYPE 9
#define TRB_DISABLESLOTCOMMAND_TYPE 10
#define TRB_ADDRESSDEVICECOMMAND_TYPE 11
#define TRB_CONFIGUREENDPOINTCOMMAND_TYPE 12
#define TRB_EVALUATECONTEXTCOMMAND_TYPE 13
#define TRB_RESETENDPOINTCOMMAND_TYPE 14
#define TRB_STOPENDPOINTCOMMAND_TYPE 15
#define TRB_SETTRDEQPOINTERCOMMAND_TYPE 16
#define TRB_RESETDEVICECOMMAND_TYPE 17
#define TRB_FORCEEVENTCOMMAND_TYPE 18
#define TRB_NEGBANDWIDTHCOMMAND_TYPE 19
#define TRB_SETLATENCYCOMMAND_TYPE 20
#define TRB_GETPORTBANDWIDTHCOMMAND_TYPE 21
#define TRB_FORCEHEADERCOMMAND_TYPE 22
#define TRB_NOOPCOMMAND_TYPE 23
#define TRB_TRANSFEREVENT_TYPE 32
#define TRB_COMMANDCOMPLETIONEVENT_TYPE 33
#define TRB_PORTSTATUSCHANGEEVENT_TYPE 34
#define TRB_HOSTCONTROLLEREVENT_TYPE 37
#define TRB_DEVICENOTIFICATIONEVENT_TYPE 38
#define TRB_MFINDEXWARPEVENT_TYPE 39

class XHCI {
public:
    static void Init();
};