
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
    uint32_t _64bitcap : 1;
    uint32_t bnc : 1;
    uint32_t contextsize : 1;
    uint32_t portpowercontrol : 1;
    uint32_t portindicator : 1;
    uint32_t lhrc : 1;
    uint32_t ltc : 1;
    uint32_t nss : 1;
    uint32_t pae : 1;
    uint32_t spc : 1;
    uint32_t sec : 1;
    uint32_t cfc : 1;
    uint32_t max_psa_size : 4;
    uint32_t xECP : 16;
} __attribute__((packed)) hccparams1_t;

typedef struct {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    hcsparams1_t hcsparams1;
    hcsparams2_t hcsparams2;
    uint32_t hcsparams3;
    hccparams1_t hccparams1;
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
#define XHCI_USB_SPEED_FULL_SPEED           1 
#define XHCI_USB_SPEED_LOW_SPEED            2 
#define XHCI_USB_SPEED_HIGH_SPEED           3 
#define XHCI_USB_SPEED_SUPER_SPEED          4 
#define XHCI_USB_SPEED_SUPER_SPEED_PLUS     5 

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
    union {
        struct {
            uint32_t reserved0 : 24;
            uint32_t ret_code : 8;
        };
        uint32_t status; 
    };   
    union {
        xhci_trb_info_t info_s;
        uint32_t info;
    };
} xhci_trb_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t reserved1 : 9;
    uint32_t type : 6;
    uint32_t vfid : 8;
    uint32_t slotid : 8;
} xhci_slot_info_trb_t;

typedef struct {
    uint64_t base;
    uint32_t CCP : 24;
    uint32_t ret_code : 8;
    union {
        xhci_slot_info_trb_t info_s;
        uint32_t info;
    };
} xhci_slot_trb_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t reserved0 : 8;
    uint32_t bsr : 1;
    uint32_t type : 6;
    uint8_t reserved1;
    uint8_t slotid;
} xhci_set_addr_info_trb_t;

typedef struct {
    uint64_t base;
    uint32_t status;
    union {
        xhci_set_addr_info_trb_t info_s;
        uint32_t info;
    };
} xhci_set_addr_trb_t;

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
    volatile IR_t* father;
    xhci_trb_t* trb;
    xhci_erst_t* table;
} xhci_event_ring_ctx_t;

typedef struct {
    uint16_t trb_limit;
    uint64_t queue;
    uint8_t cycle;
    uint32_t slot;
    xhci_trb_t* trb;
} xhci_port_ring_ctx_t;

typedef struct {
    uint32_t D;
    uint32_t A;
    uint32_t reserved0[5];
    uint8_t configvalue;
    uint8_t interfacenum;
    uint8_t altsetting;
    uint8_t reserved1;
} __attribute__((packed)) xhci_input_control_ctx_t;

typedef struct {
    uint32_t D;
    uint32_t A;
    uint32_t reserved0[5];
    uint8_t configvalue;
    uint8_t interfacenum;
    uint8_t altsetting;
    uint8_t reserved1;
    uint32_t align[8];
} __attribute__((packed)) xhci_input_control_ctx64_t;

typedef struct {
    uint32_t routestring : 20;
    uint8_t speed : 4;
    uint8_t reserved0 : 1;
    uint8_t multitt : 1;
    uint8_t hub : 1;
    uint8_t contextentries : 5;
    uint16_t maxexitlat;
    uint8_t porthubnum;
    uint8_t numofports;
    uint8_t parenthubslot;
    uint8_t parentportnum;
    uint8_t thinktime : 2;
    uint8_t reserved1 : 4;
    uint16_t irtarget : 10;
    uint8_t address;
    uint32_t reserved2 : 19;
    uint8_t slotstate : 5;
    uint32_t reserved3[4];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t routestring : 20;
    uint8_t speed : 4;
    uint8_t reserved0 : 1;
    uint8_t multitt : 1;
    uint8_t hub : 1;
    uint8_t contextentries : 5;
    uint16_t maxexitlat;
    uint8_t porthubnum;
    uint8_t numofports;
    uint8_t parenthubslot;
    uint8_t parentportnum;
    uint8_t thinktime : 2;
    uint8_t reserved1 : 4;
    uint16_t irtarget : 10;
    uint8_t address;
    uint32_t reserved2 : 19;
    uint8_t slotstate : 5;
    uint32_t reserved3[4];
    uint32_t align[8];
} __attribute__((packed)) xhci_slot_ctx64_t;

typedef struct {
    uint32_t state : 3;
    uint32_t reserved0 : 5;
    uint32_t mult : 2;
    uint32_t maxprimarystreams : 5;
    uint32_t linear : 1;
    uint32_t interval : 8;
    uint32_t some_shit_with_long_name : 8;
    uint32_t reserved1 : 1;
    uint32_t cerr : 2;
    uint32_t endpointtype : 3;
    uint32_t reserved2 : 1;
    uint32_t hid : 1;
    uint32_t maxburstsize : 8;
    uint32_t maxpacketsize : 16;
    uint64_t base;
    uint16_t averagetrblen;
    uint16_t some_shit_with_long_name_lo;
    uint32_t align[3];
} __attribute__((packed)) xhci_endpoint_ctx_t;

typedef struct {
    uint32_t state : 3;
    uint32_t reserved0 : 5;
    uint32_t mult : 2;
    uint32_t maxprimarystreams : 5;
    uint32_t linear : 1;
    uint32_t interval : 8;
    uint32_t some_shit_with_long_name : 8;
    uint32_t reserved1 : 1;
    uint32_t cerr : 2;
    uint32_t endpointtype : 3;
    uint32_t reserved2 : 1;
    uint32_t hid : 1;
    uint32_t maxburstsize : 8;
    uint32_t maxpacketsize : 16;
    uint64_t base;
    uint16_t averagetrblen;
    uint16_t some_shit_with_long_name_lo;
    uint32_t align[11];
} __attribute__((packed)) xhci_endpoint_ctx64_t;

typedef struct {
    xhci_input_control_ctx_t input_ctx;
    xhci_slot_ctx_t slot;
    xhci_endpoint_ctx_t ep0;
    xhci_endpoint_ctx_t ep[30];
} __attribute__((packed)) xhci_input_ctx_t;

typedef struct {
    xhci_input_control_ctx64_t input_ctx;
    xhci_slot_ctx64_t slot;
    xhci_endpoint_ctx64_t ep0;
    xhci_endpoint_ctx64_t ep[30];
} __attribute__((packed)) xhci_input_ctx64_t;

typedef struct {
    uint8_t len;
    uint8_t type;
} xhci_usb_descriptor_header;

typedef struct {
    xhci_usb_descriptor_header head;
    uint16_t usb;
    uint8_t deviceclass;
    uint8_t devicesubclass;
    uint8_t protocol;
    uint8_t maxpacketsize;
    uint16_t vendor;
    uint16_t product0;
    uint16_t device;
    uint8_t manufacter;
    uint8_t product1;
    uint8_t serialnum;
    uint8_t numconfigs;
} xhci_usb_descriptor_t;

typedef struct {
    xhci_usb_descriptor_header head;
    uint16_t lang[128];
} xhci_lang_descriptor_t;

typedef struct {
    xhci_usb_descriptor_header head;
    uint16_t str[128];
} xhci_string_descriptor_t;

typedef struct {
    xhci_usb_descriptor_header head;
    uint16_t len;
    uint8_t numinterfaces;
    uint8_t configval;
    uint8_t config;
    uint8_t attributes;
    uint8_t maxpower;
    uint8_t data[1024];
} xhci_config_descriptor_t;

typedef struct {
    uint8_t desctype;
    uint8_t desclen;
} xhci_hid_sub_desc;

typedef struct {
    xhci_usb_descriptor_header head;
    uint16_t hid;
    uint8_t country;
    uint8_t numdesc;
    xhci_hid_sub_desc desc[1024];
} xhci_hid_descriptor_t;

typedef struct {
    xhci_usb_descriptor_header head;
    uint8_t endpointaddr;
    uint8_t attributes;
    uint16_t maxpacketsize;
    uint8_t interval;
} xhci_endpoint_descriptor_t;

typedef struct {
    xhci_usb_descriptor_header head;
    uint8_t num;
    uint8_t altsetting;
    uint8_t numendpoints;
    uint8_t interclass;
    uint8_t intersubclass;
    uint8_t protocol;
    uint8_t interface;
} xhci_interface_descriptor_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t reserved0 : 8;
    uint32_t deconfigure : 1;
    uint32_t type : 6;
    uint32_t reserved1 : 8;
    uint32_t slot : 8;
} xhci_configure_endpoints_info_trb_t;

typedef struct {
    uint64_t base;
    uint32_t status;
    union {
        xhci_configure_endpoints_info_trb_t info_s;
        uint32_t info;
    };
} xhci_configure_endpoints_trb_t;

typedef struct xhci_device {
    uint64_t xhci_phys_base;
    uint64_t xhci_virt_base;
    uint64_t* dcbaa;
    uint16_t calculated_scratchpad_count;
    volatile xhci_cap_regs_t* cap;
    volatile xhci_op_regs_t* op;
    volatile xhci_port_regs_t* port;
    volatile xhci_runtime_regs_t* runtime;
    uint32_t* doorbell;
    xhci_command_ring_ctx_t* com_ring;
    xhci_event_ring_ctx_t* event_ring;

    uint32_t max_ports; 

    uint8_t usb3ports[64];

    struct xhci_device* next;
} __attribute__((packed)) xhci_device_t;

typedef struct xhci_interface {
    uint8_t type;
    void* data;

    uint64_t len;
    void* buffer;
    
    xhci_interface* next;
} xhci_interface_t;

#define USB_TYPE_KEYBOARD 4
#define USB_TYPE_MOUSE 3

typedef struct xhci_usb_device {
    xhci_port_ring_ctx_t* transfer_ring;
    xhci_usb_descriptor_t* desc;
    xhci_config_descriptor_t* config;
    xhci_device_t* dev;
    xhci_input_ctx_t* input_ctx;
    xhci_port_ring_ctx_t* ep_ctx[30];
    uint8_t* buffers[30];
    uint8_t main_ep;
    uint16_t buffers_need_size[30];
    uint8_t doorbell_values[30];
    xhci_interface_t* interface;
    struct xhci_usb_device* next;
    uint64_t phys_input_ctx;
    uint32_t slotid;
    uint32_t portnum;
    uint32_t type;
    uint8_t _is64byte;
    uint8_t add_buffer[8];
} xhci_usb_device_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t reserved0 : 1;
    uint32_t eventdata : 1;
    uint32_t reserved1 : 7;
    uint32_t type : 6;
    uint32_t ep_id : 5;
    uint32_t reserved2 : 3;
    uint32_t slot : 8;
} xhci_done_info_trb_t;

typedef struct {
    uint64_t base;
    struct {
        uint32_t seek : 24;
        uint32_t ret_code : 8;
    };
    union {
        xhci_done_info_trb_t info_s;
        uint32_t info;
    };
} xhci_done_trb_t;

typedef struct {
    union {
        struct {
            uint8_t id;
            uint8_t nextcap;
            uint16_t info;
        };
        uint32_t full;
    };
} xhci_ext_cap_t;

typedef struct {
    union {
        struct {
            uint8_t id;
            uint8_t nextcap;
            uint8_t minor;
            uint8_t major;
        };
        uint32_t firsthalf;
    };

    uint32_t name;

    union {
        struct {
            uint8_t portoffset;
            uint8_t portcount;
            uint8_t protocoldefine;
            uint8_t protocolspeedid;
        };
        uint32_t thirdhalf;
    };

    union {
        struct {
            uint8_t slottype : 4;
            uint32_t reserved0 : 28;
        };
        uint32_t fourhalf;
    };
} xhci_usb_cap_t;

typedef struct {
    uint8_t type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t len;
} xhci_usb_command_t;

typedef struct {
    xhci_usb_command_t command;
    uint32_t len : 17;
    uint32_t reserved0 : 5;
    uint32_t target : 10;
    uint32_t cycle : 1;
    uint32_t reserved1 : 4;
    uint32_t intoncomp : 1;
    uint32_t imdata : 1;
    uint32_t reserved2 : 3;
    uint32_t type : 6;
    uint32_t trt : 2;
    uint32_t reserved3 : 14;
} xhci_setupstage_trb_t;

typedef struct {
    uint64_t data;
    uint32_t len : 17;
    uint32_t tdsize : 5;
    uint32_t target : 10;
    uint32_t cycle : 1;
    uint32_t evnexttrb : 1;
    uint32_t intrshortpacket : 1;
    uint32_t nosnoop : 1;
    uint32_t chain : 1;
    uint32_t introncomp : 1;
    uint32_t imdata : 1;
    uint32_t reserved0 : 3;
    uint32_t type : 6;
    uint32_t direction : 1;
    uint32_t reserved1 : 15;
} xhci_datastage_trb_t;

typedef struct {
    uint64_t base;
    uint32_t reserved0 : 22;
    uint32_t target : 10;
    uint32_t cycle : 1;
    uint32_t evnext : 1;
    uint32_t reserved1 : 2;
    uint32_t chain : 1;
    uint32_t intoncomp : 1;
    uint32_t reserved2 : 3;
    uint32_t blockevent : 1;
    uint32_t type : 6;
    uint32_t reserved3 : 16;
} xhci_eventdata_trb_t;

typedef struct {
    uint64_t reserved0;
    uint32_t reserved1 : 22;
    uint32_t target : 10;
    uint32_t cycle : 1;
    uint32_t evnext : 1;
    uint32_t reserved2 : 2;
    uint32_t chain : 1;
    uint32_t intoncomp : 1;
    uint32_t reserved3 : 4;
    uint32_t type : 6;
    uint32_t direction : 1;
    uint32_t reserved4 : 15;
} xhci_statusstage_trb_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t ent : 1;
    uint32_t isp : 1;
    uint32_t nosnoop : 1;
    uint32_t chain : 1;
    uint32_t ioc : 1;
    uint32_t imdata : 1;
    uint32_t reserved0 : 2;
    uint32_t bei : 1;
    uint32_t type : 6;
    uint32_t dir : 1;
    uint32_t reserved1 : 15;
} xhci_normal_info_trb_t;

typedef struct {
    uint64_t base;
    uint32_t trbtransferlen : 17;
    uint32_t tdsize : 5;
    uint32_t target : 10;
    union {
        xhci_normal_info_trb_t info_s;
        uint32_t info;
    };
} xhci_normal_trb_t;

typedef struct {
    uint32_t cycle : 1;
    uint32_t reserved0 : 9;
    uint32_t type : 6;
    uint32_t reserved1 : 16;
} xhci_port_change_info_t;

typedef struct {
    struct {
        uint32_t reserved0 : 24;
        uint32_t port : 8;
    };
    uint32_t status;
    union {
        xhci_port_change_info_t info_s;
        uint32_t info;
    };
} xhci_port_change_trb_t;

typedef struct xhci_hid_driver {
    void (*func)(xhci_usb_device_t* usbdev,xhci_done_trb_t* trb);
    int type;
    struct xhci_hid_driver* next;
} xhci_hid_driver_t;

#define XHCI_ENDPOINTTYPE_ISOCHRONOUS_OUT  1
#define XHCI_ENDPOINTTYPE_BULK_OUT         2
#define XHCI_ENDPOINTTYPE_INTERRUPT_OUT    3
#define XHCI_ENDPOINTTYPE_ISOCHRONOUS_IN   5
#define XHCI_ENDPOINTTYPE_BULK_IN          6
#define XHCI_ENDPOINTTYPE_INTERRUPT_IN     7
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
    static void HIDRegister(void (*func)(xhci_usb_device_t* usbdev,xhci_done_trb_t* trb),int type);
};