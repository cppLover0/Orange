#include <drivers/xhci.hpp>
#include <klibc/stdio.hpp>
#include <klibc/string.hpp>
#include <klibc/stdlib.hpp>
#include <generic/hhdm.hpp>
#include <generic/pmm.hpp>
#include <generic/time.hpp>
#include <generic/scheduling.hpp>
#include <generic/paging.hpp>
#include <utils/align.hpp>
#include <generic/evdev.hpp>
#include <utils/gobject.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/assembly.hpp>
#include <arch/x86_64/drivers/pci.hpp>
#endif

xhci_device_t* xhci_list;

void __xhci_put_new_device(xhci_device_t* dev) {
    if(!dev) {
        return;
    }

    if(!xhci_list) {
        xhci_list = dev;
    } else {
        dev->next = xhci_list;
        xhci_list = dev;
    }

}

void __xhci_doorbell(xhci_device_t* dev,uint32_t value) {
    arch::memory_barrier();
    *dev->doorbell = value;
    arch::memory_barrier();
}

void __xhci_doorbell_id(xhci_device_t* dev,uint32_t idx,uint32_t val) {
    arch::memory_barrier();
    dev->doorbell[idx] = val;
    arch::memory_barrier();
}

void __xhci_reset(xhci_device_t* dev) {

    time::timer->sleep(50*1000); // wait some time

    dev->op->usbcmd |= (1 << 1);
    uint16_t timeout = XHCI_RESET_TIMEOUT;
    while(dev->op->usbsts & (1 << 11)) {
        if(!timeout) {
            log("xhci","Can't reset XHCI Controller, ignoring");
            break;
        }
        time::timer->sleep(5 * 1000);
        timeout = timeout - 1;
    }

    time::timer->sleep(5000);

}

void __xhci_enable(xhci_device_t* dev) {
    time::timer->sleep(50*1000);
    dev->op->usbcmd |= (1 << 0) | (1 << 2);
    uint16_t timeout = XHCI_RESET_TIMEOUT;
    while(dev->op->usbsts & (1 << 0)) {
        if(!timeout) {
            log("xhci","Can't start XHCI Controller, crying");
            break;
        }
        time::timer->sleep(5 * 1000);
        timeout = timeout - 1;
    }

    time::timer->sleep(5000);

}

void __xhci_reset_intr(xhci_device_t* dev, uint16_t intr) {

    if(intr > dev->cap->hcsparams1.maxintrs) {
        log("xhci","Intr is higher than maxintrs! Skipping");
        return;
    }

    dev->op->usbsts = (1 << 3);
    dev->runtime->int_regs[intr].iman |= (1 << 0);
}

void __xhci_punch_intr(xhci_device_t* dev,uint16_t intr) {
    dev->runtime->int_regs[intr].erdp.event_busy = 1;
}

void __xhci_update_stepfather(xhci_device_t* dev,xhci_event_ring_ctx_t* grandpa) {
    (void)dev;
    grandpa->father->erdp_val = grandpa->table[0].base + (grandpa->queue * sizeof(xhci_trb_t));
}

xhci_trb_t* __xhci_move_father(xhci_device_t* dev,xhci_event_ring_ctx_t* ctx) {
    (void)dev;
    xhci_trb_t* trb = &ctx->trb[ctx->queue++];
    if(ctx->queue == ctx->trb_limit) {
        ctx->cycle = !ctx->cycle;
        ctx->queue = 0;
    }
    return trb;
}

xhci_event_ring_ctx_t* __xhci_create_event_ring(xhci_device_t* dev,uint16_t trb_size,volatile IR_t* stepfather,uint16_t sons) {
    xhci_event_ring_ctx_t* ring_info = new xhci_event_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->father = stepfather; // :pensive:
    ring_info->trb_limit = trb_size;
    ring_info->trb = 0;

    uint64_t new_table = pmm::buddy::alloc(ALIGNPAGEUP(sons * sizeof(xhci_erst_t))).phys;
    void* virt_table = (void*)(new_table + etc::hhdm());
    
    ring_info->table = (xhci_erst_t*)virt_table;

    uint64_t phys_trb = pmm::buddy::alloc(16 * PAGE_SIZE).phys;
    ring_info->table[0].base = phys_trb;
    ring_info->table[0].size = trb_size;

    ring_info->trb = (xhci_trb_t*)(phys_trb + etc::hhdm());

    stepfather->erstsz = sons;
    __xhci_update_stepfather(dev,ring_info);
    stepfather->erstba = new_table;

    return ring_info;

}

xhci_trb_t get_trb(xhci_event_ring_ctx_t* father,uint16_t idx) {
    return father->trb[idx];
}

int __xhci_event_receive(xhci_device_t* dev,xhci_event_ring_ctx_t* father,xhci_trb_t** out) { // it will return trb virtual addresses 
    int len = 0;
    arch::memory_barrier();
    while(get_trb(father,father->queue).info_s.cycle == father->cycle)
        out[len++] = __xhci_move_father(dev,father);
    __xhci_punch_intr(dev,0);
    __xhci_update_stepfather(dev,father);
    arch::memory_barrier();
    return len;
}

xhci_command_ring_ctx_t* __xhci_create_command_ring(xhci_device_t* dev,uint16_t trb_size) {
    (void)dev;
    xhci_command_ring_ctx_t* ring_info = new xhci_command_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->trb_limit = trb_size;

    uint64_t phys_trb = pmm::buddy::alloc(ALIGNPAGEUP(trb_size * sizeof(xhci_trb_t))).phys;

    ring_info->phys = phys_trb;

    ring_info->trb = (xhci_trb_t*)(phys_trb + etc::hhdm());
    ring_info->trb[trb_size - 1].base = phys_trb;
    ring_info->trb[trb_size - 1].info = (6 << 10) | (1 << 1) | (1 << 0);
    return ring_info;
}


void __xhci_command_ring_queue(xhci_device_t* dev,xhci_command_ring_ctx_t* ctx,xhci_trb_t* trb) {
    (void)dev;
    trb->info |= ctx->cycle;
    klibc::memcpy(&ctx->trb[ctx->queue],trb,sizeof(xhci_trb_t));
    if(++ctx->queue == ctx->trb_limit - 1) {
        ctx->queue = 0;
        ctx->trb[ctx->trb_limit - 1].info = (6 << 10) | (1 << 1) | (ctx->cycle << 0); 
        ctx->cycle = !ctx->cycle;
    }
    arch::memory_barrier();
}

void __xhci_setup_run(xhci_device_t* dev) {
    volatile IR_t* head = (volatile IR_t*)&dev->runtime->int_regs[0];
    head->iman |= (1 << 1);
    
    dev->event_ring = __xhci_create_event_ring(dev,256,head,1);

    __xhci_reset_intr(dev,0);
} 

void __xhci_fill_dcbaa(xhci_device_t* dev) {

    uint64_t phys_dcbaa = pmm::buddy::alloc(ALIGNPAGEUP(8*dev->cap->hcsparams1.maxslots)).phys;

    uint64_t* dcbaa = (uint64_t*)(phys_dcbaa  + etc::hhdm());

    if(dev->calculated_scratchpad_count) {

        uint64_t phys_list = pmm::buddy::alloc(ALIGNPAGEUP(8*dev->calculated_scratchpad_count)).phys;

        uint64_t* list = (uint64_t*)(phys_list  + etc::hhdm());
        for(uint16_t i = 0;i < dev->calculated_scratchpad_count;i++) {
            uint64_t hi = pmm::freelist::alloc_4k();
            list[i] = hi;
        }
        dcbaa[0] = phys_list;
    }

    dev->dcbaa = (uint64_t*)dcbaa;
    dev->op->dcbaap = phys_dcbaa;
    time::timer->sleep(5000);
}

void __xhci_setup_op(xhci_device_t* dev) {
    __xhci_fill_dcbaa(dev);
    dev->com_ring = __xhci_create_command_ring(dev,128); // i dont think what command ring will be fat 
    dev->op->crcr = dev->com_ring->phys | dev->com_ring->cycle; 
    dev->op->config = dev->cap->hcsparams1.maxslots;
    dev->op->dnctrl = 0xFFFF;
} 

volatile uint32_t* __xhci_portsc(xhci_device_t* dev,uint32_t portnum) {
    return ((volatile uint32_t*)(dev->xhci_virt_base + 0x400 + dev->cap->caplength + (0x10 * portnum)));
}

const char* trb_type_to_str(int type) {
    switch(type) {
        case TRB_COMMANDCOMPLETIONEVENT_TYPE:
            return "TRB_COMMAND_COMPLETION_EVENT";
        case TRB_TRANSFEREVENT_TYPE:
            return "TRB_TRANSFER_EVENT";
        default:
            return "Unknown";
    }
}

xhci_trb_t __xhci_event_wait(xhci_device_t* dev,int type) {
    xhci_event_ring_ctx_t* father = dev->event_ring;
    xhci_trb_t* buffer[1024];
    int timeout = 100;
    while(1) {
        arch::memory_barrier();
        int count = 0;
        klibc::memset(buffer,0,sizeof(xhci_trb_t*) * 1024);
        if(get_trb(father,father->queue).info_s.cycle == father->cycle)
            count = __xhci_event_receive(dev,father,buffer);

        if(count) {
            xhci_trb_t* current = 0;
            for(int i = 0;i < count;i++) {
                current = buffer[i];
                if(current->info_s.type == type) {
                    return *current;
                } else {
                    log("xhci","Wait for wrong type %d",current->info_s.type);
                }
            }
        }

        if(--timeout == 0) {
            xhci_trb_t t;
            t.base = 0xDEAD;
            t.ret_code = 0;
            return t;
        }

        arch::memory_barrier();
        time::timer->sleep(5000);
        
    }
}

void __xhci_clear_event(xhci_device_t* dev) {
    xhci_trb_t* buffer[1024];
    if(get_trb(dev->event_ring,dev->event_ring->queue).info_s.cycle == dev->event_ring->cycle)
        __xhci_event_receive(dev,dev->event_ring,buffer);
}


xhci_port_ring_ctx_t* __xhci_setup_port_ring(uint16_t trb_count,uint16_t slot) {
    xhci_port_ring_ctx_t* ring_info = new xhci_port_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->slot = slot;
    ring_info->trb_limit = trb_count;

    uint64_t phys_trb = pmm::buddy::alloc(16 * 4096).phys;
    ring_info->trb = (xhci_trb_t*)(phys_trb  + etc::hhdm());

    ring_info->phys = phys_trb;
    ring_info->trb[trb_count - 1].base = phys_trb;
    ring_info->trb[trb_count - 1].info = (6 << 10) | (1 << 1) | (1 << 0);
    return ring_info;
}

// its same as command ring
void __xhci_port_ring_queue(xhci_port_ring_ctx_t* ctx,xhci_trb_t* trb) {
    trb->info |= ctx->cycle;
    klibc::memcpy(&ctx->trb[ctx->queue],trb,sizeof(xhci_trb_t));
    if(++ctx->queue == ctx->trb_limit - 1) {
        ctx->queue = 0;
        ctx->trb[ctx->trb_limit - 1].info = (6 << 10) | (1 << 1) | (ctx->cycle << 0); 
        ctx->cycle = !ctx->cycle;
    }
}

void __xhci_create_dcbaa(xhci_device_t* dev,uint32_t slotid,uint64_t addr) {
    dev->dcbaa[slotid] = addr;
}

xhci_usb_device_t* usbdevs = 0;

xhci_usb_device_t* __xhci_alloc_dev(uint32_t portnum,uint32_t slotid) {
    xhci_usb_device_t* dev = new xhci_usb_device_t;
    dev->next = 0;
    dev->portnum = portnum;
    dev->slotid = slotid;
    dev->transfer_ring = __xhci_setup_port_ring(256,slotid);
    dev->phys_input_ctx = pmm::freelist::alloc_4k();
    if(!usbdevs) {
        usbdevs = dev;
    } else {
        dev->next = usbdevs;
        usbdevs = dev;
    }
    return dev;
}

int __xhci_enable_slot(xhci_device_t* dev, int portnum) {
    xhci_trb_t trb;

    klibc::memset(&trb,0,sizeof(xhci_trb_t));

    trb.info_s.intoncompletion = 1;
    trb.info_s.type = TRB_ENABLESLOTCOMMAND_TYPE;

    __xhci_clear_event(dev);

    __xhci_command_ring_queue(dev,dev->com_ring,&trb);
    __xhci_doorbell(dev,0);
    time::timer->sleep(25000);
    
    xhci_trb_t ret = __xhci_event_wait(dev,TRB_COMMANDCOMPLETIONEVENT_TYPE);

    if(ret.base == 0xDEAD) {
        log("xhci","Timeout for port %d in slot enabling",portnum);
        return 0;
    }

    xhci_slot_trb_t* slot_ret = (xhci_slot_trb_t*)&ret;

    if(ret.ret_code != 1)
        log("xhci","Can't allocate slot for port %d (ret %d)",portnum,ret.ret_code);

    return slot_ret->info_s.slotid;

}

int __xhci_set_addr(xhci_device_t* dev,uint64_t addr,uint32_t id,char bsr) {
    (void)bsr;
    xhci_set_addr_trb_t trb;
    klibc::memset(&trb,0,sizeof(trb));
    trb.base = addr;
    trb.info_s.bsr = 0;
    trb.info_s.type = TRB_ADDRESSDEVICECOMMAND_TYPE;
    trb.info_s.slotid = id;
    __xhci_clear_event(dev);

    time::timer->sleep(5000);

    __xhci_clear_event(dev);

    __xhci_command_ring_queue(dev,dev->com_ring,(xhci_trb_t*)&trb);
    __xhci_doorbell(dev,0);
    time::timer->sleep(5000);
    
    xhci_trb_t ret = __xhci_event_wait(dev,TRB_COMMANDCOMPLETIONEVENT_TYPE);

    if(ret.ret_code != 1 && ret.ret_code != 0)
        log("xhci","Can't set XHCI port address (ret %d)",ret.ret_code);

    return ret.ret_code;

}

xhci_trb_t __xhci_send_usb_request_packet(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_usb_command_t usbcommand,void* out,uint64_t len) {

    uint64_t phys_status_buf = pmm::freelist::alloc_4k();
    uint64_t phys_desc_buf = pmm::freelist::alloc_4k();

    void* desc_buf = (void*)(phys_desc_buf + etc::hhdm());

    klibc::memset(out,0,len);

    xhci_setupstage_trb_t setup;
    xhci_datastage_trb_t data;
    xhci_eventdata_trb_t event0;
    xhci_statusstage_trb_t status;
    xhci_eventdata_trb_t event1;

    klibc::memset(&setup,0,sizeof(xhci_trb_t));
    klibc::memset(&data,0, sizeof(xhci_trb_t));
    klibc::memset(&event0,0,sizeof(xhci_trb_t));
    klibc::memset(&status,0,sizeof(xhci_trb_t));
    klibc::memset(&event1,0,sizeof(xhci_trb_t));

    klibc::memcpy(&setup.command,&usbcommand,sizeof(xhci_usb_command_t));

    setup.type = TRB_SETUPSTAGE_TYPE;
    setup.trt = 3;
    setup.imdata = 1;
    setup.len = 8;

    data.type = TRB_DATASTAGE_TYPE;
    data.data = phys_desc_buf;
    data.len = len;
    data.chain = 1;
    data.direction = 1;

    event0.type = TRB_EVENTDATA_TYPE;
    event0.base = phys_status_buf;
    event0.chain = 0;
    event0.intoncomp = 1;

    status.type = TRB_STATUSSTAGE_TYPE;
    status.chain = 1;

    event1.type = TRB_EVENTDATA_TYPE;
    event1.intoncomp = 1;

    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&setup);
    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&data);
    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&event0);
    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&status);
    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&event1);


    arch::memory_barrier();

    __xhci_clear_event(dev);

    __xhci_doorbell_id(dev,usbdev->slotid,1);
    time::timer->sleep(5000);

    xhci_trb_t ret = __xhci_event_wait(dev,TRB_TRANSFEREVENT_TYPE);

    if(ret.base == 0xDEAD) {
        ret.ret_code = 0;
        log("xhci", "timeout !");
        return ret;
    }

    if(ret.ret_code != 1) {
        log("xhci","Failed to request xhci device, idx: %p, val: %p, type: %p, len: %p, request: %d ret_code %d",usbcommand.index,usbcommand.value,usbcommand.type,usbcommand.len,usbcommand.request,ret.ret_code);
        ret.ret_code = 0;
        return ret;
    }

    time::timer->sleep(5000);

    klibc::memcpy(out,desc_buf,len);

    pmm::freelist::free(phys_desc_buf);
    pmm::freelist::free(phys_status_buf);

    return ret;

}

xhci_trb_t __xhci_send_usb_packet(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_usb_command_t com) {

    xhci_setupstage_trb_t setup;
    xhci_statusstage_trb_t status;

    klibc::memset(&setup,0,sizeof(xhci_setupstage_trb_t));
    klibc::memset(&status,0,sizeof(xhci_statusstage_trb_t));

    klibc::memcpy(&setup.command,&com,sizeof(xhci_usb_command_t));

    setup.imdata = 1;
    setup.len = 8;
    setup.type = TRB_SETUPSTAGE_TYPE;

    status.direction = 1;
    status.intoncomp = 1;
    status.type = TRB_STATUSSTAGE_TYPE;

    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&setup);
    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&status);

    __xhci_clear_event(dev);

    __xhci_doorbell_id(dev,usbdev->slotid,1);
    time::timer->sleep(5000);

    xhci_trb_t ret = __xhci_event_wait(dev,TRB_TRANSFEREVENT_TYPE);

    return ret;

}

int __xhci_get_usb_descriptor(xhci_device_t* dev,xhci_usb_device_t* usbdev,void* out,uint64_t len) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = 1 << 8;
    usbcommand.index = 0;
    usbcommand.len = len;

    xhci_trb_t ret = __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,len);
    if(ret.ret_code != 1) {
        log("xhci", "failed to get usb descriptior ret code is %d\r\n",ret.ret_code);
        return 0;
    }

    return 1;

}

uint16_t __xhci_get_speed(xhci_device_t* dev,uint32_t portsc) {
    (void)dev;
    uint8_t extracted_speed = (portsc & 0x3C00) >> 10;
    switch (extracted_speed) {
        case XHCI_USB_SPEED_LOW_SPEED: 
            return 8; 

        case XHCI_USB_SPEED_FULL_SPEED:
            return 64;

        case XHCI_USB_SPEED_HIGH_SPEED: 
            return 64;

        case XHCI_USB_SPEED_SUPER_SPEED:
            return 512;

        case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
            return 512;

        default: 
            return 0;
    }
}

int __xhci_reset_dev(xhci_device_t* dev, uint32_t portnum) {
    volatile uint32_t* portsc = (volatile uint32_t*)__xhci_portsc(dev, portnum);
    uint32_t load_portsc = *portsc;

    if (!(load_portsc & (1 << 9))) {
        *portsc = (load_portsc & 0x0E0003E0) | (1 << 9); 
        time::timer->sleep(5000);
        if (!(*portsc & (1 << 9))) return 0;
    }

    if (dev->usb3ports[portnum]) {

        uint32_t val = *portsc & 0x0E0003E0; 
        *portsc = val | (1 << 31);
    } else {

        uint32_t val = *portsc & 0x0E0003E0;
        *portsc = val | (1 << 4);
    }

    uint16_t timeout = 50;
    if (dev->usb3ports[portnum]) {

        while (!(*portsc & (1 << 19))) {
            if (timeout-- == 0) {
                log("xhci", "Can't reset USB 3.0 device on port %d", portnum);
                return 0;
            }
            time::timer->sleep(5000);
        }

        *portsc = (*portsc & 0x0E0003E0) | (1 << 19);
    } else {
        while (*portsc & (1 << 4)) {
            if (timeout-- == 0) {
                log("xhci", "Can't reset USB 2.0 device on port %d", portnum);
                return 0;
            }
            time::timer->sleep(5000);
        }
    }

    time::timer->sleep(50000);
    return 1;
}


void __xhci_unicode_to_ascii(uint16_t* src,char* dest) {
    uint16_t src_ptr = 0;
    uint16_t dest_ptr = 0;

    uint16_t* ptr = (uint16_t*)src;
    while(ptr[src_ptr]) {

        if(ptr[src_ptr] < 128)
            dest[dest_ptr++] = ptr[src_ptr++];
        else
            dest[dest_ptr++] = '?';
    }
    dest[dest_ptr] = '\0';
}

int __xhci_read_usb_string(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_string_descriptor_t* out,uint8_t index,uint8_t lang) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = (3 << 8) | index;
    usbcommand.index = lang;
    usbcommand.len = sizeof(xhci_usb_descriptor_header);

    xhci_trb_t ret = __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
    if(ret.ret_code != 1)
        return 0;

    usbcommand.len = out->head.len;

    if(usbcommand.len < 8) {
        return 1;
    }

    ret = __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
    if(ret.ret_code != 1)
        return 0;

    return 1;

}



int __xhci_read_usb_lang_string(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_lang_descriptor_t* out) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = (3 << 8);
    usbcommand.index = 0;
    usbcommand.len = sizeof(xhci_usb_descriptor_header);

    xhci_trb_t ret = __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
    if(ret.ret_code != 1)
        return 0;

    usbcommand.len = out->head.len;

    ret = __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
    if(ret.ret_code != 1)
        return 0;

    return 1;

}

int __xhci_print_device_info(xhci_device_t* dev,xhci_usb_device_t* usb_dev,char* product0,char* manufacter0) {
    xhci_lang_descriptor_t lang;

    int status = __xhci_read_usb_lang_string(dev,usb_dev,&lang);

    if(!status)
        return 0;

    uint16_t lang0 = lang.lang[0];

    xhci_string_descriptor_t product;
    xhci_string_descriptor_t manufacter;

    klibc::memset(&product,0,sizeof(product));
    klibc::memset(&manufacter,0,sizeof(manufacter));

    status = __xhci_read_usb_string(dev,usb_dev,&product,usb_dev->desc->product1,lang0);

    if(!status)
        return 0;

    status = __xhci_read_usb_string(dev,usb_dev,&manufacter,usb_dev->desc->manufacter,lang0);

    if(!status)
        return 0;

    klibc::memset(product0,0,256);
    klibc::memset(manufacter0,0,256);

    __xhci_unicode_to_ascii(product.str,product0);
    __xhci_unicode_to_ascii(manufacter.str,manufacter0);

    return 1;

}

void __xhci_setup_config(xhci_device_t* dev,xhci_usb_device_t* usbdev,uint16_t value) {
    xhci_usb_command_t command;
    command.type = 0;
    command.index = 0;
    command.len = 0;
    command.request = 9;
    command.value = value;

    int ret = __xhci_send_usb_packet(dev,usbdev,command).ret_code;

    if(ret != 1) 
        log("xhci","Can't set configuration on port %d with val %d (ret %d)",usbdev->portnum,value,ret);
}

void __xhci_get_config_descriptor(xhci_device_t* dev,xhci_usb_device_t* usbdev, xhci_config_descriptor_t* out) {
    xhci_usb_command_t com;
    com.type = 0x80;
    com.value = 2 << 8;
    com.index = 0;
    com.len = sizeof(xhci_usb_descriptor_header);
    com.request = 6;

    __xhci_send_usb_request_packet(dev,usbdev,com,out,sizeof(xhci_usb_descriptor_header));
    com.len = out->head.len;
    __xhci_send_usb_request_packet(dev,usbdev,com,out,out->head.len);
    com.len = out->len;
    __xhci_send_usb_request_packet(dev,usbdev,com,out,out->len);

}

void __xhci_get_hid_report(xhci_device_t* dev,xhci_usb_device_t* usbdev,uint8_t idx,uint8_t internum,void* data,uint32_t len) {
    xhci_usb_command_t cmd;
    cmd.index = internum;
    cmd.request = 6;
    cmd.type = 0x81;
    cmd.len = len;
    cmd.value = (0x22 << 8) | idx;
    __xhci_send_usb_request_packet(dev,usbdev,cmd,data,len);
}

void __xhci_set_boot_protocol(xhci_device_t* dev, xhci_usb_device_t* usbdev, int internum) {
    xhci_usb_command_t cmd;
    cmd.index = internum;
    cmd.request = 0x0B;
    cmd.type = 0x21;
    cmd.len = 0;
    cmd.value = 0;

    char data[4096];
    __xhci_send_usb_request_packet(dev,usbdev,cmd,data,0);
}

int __xhci_ep_to_type(xhci_endpoint_descriptor_t* desc) {
    uint8_t direction = (desc->endpointaddr & 0x80) ? 1 : 0;
    uint8_t type = desc->attributes & 3;

    switch (type) {
        case 1:
            return direction == 1 ? XHCI_ENDPOINTTYPE_ISOCHRONOUS_IN : XHCI_ENDPOINTTYPE_ISOCHRONOUS_OUT;
        
        case 2: 
            return direction == 1 ? XHCI_ENDPOINTTYPE_BULK_IN : XHCI_ENDPOINTTYPE_BULK_OUT;
        
        case 3:
            return direction == 1 ? XHCI_ENDPOINTTYPE_INTERRUPT_IN : XHCI_ENDPOINTTYPE_INTERRUPT_OUT;
    }

    return 0; // without it i will got UB
}

void __xhci_ask_for_help_hid(xhci_device_t* dev,xhci_usb_device_t* usbdev) {
    
    for(int i = 0;i < 30;i++) {

        if(!usbdev->doorbell_values[i])
            continue;

        xhci_port_ring_ctx_t* transfer_ring = usbdev->ep_ctx[i];
        xhci_normal_trb_t trb;
        klibc::memset(&trb,0,sizeof(xhci_normal_trb_t));
        trb.info_s.type = 1;
        trb.info_s.ioc = 1;
        trb.base = (uint64_t)usbdev->phys_buffers[i];
        trb.trbtransferlen = usbdev->buffers_need_size[i];

        __xhci_port_ring_queue(transfer_ring,(xhci_trb_t*)&trb);
        __xhci_doorbell_id(dev,usbdev->slotid,usbdev->doorbell_values[i]);
    }

}

const char* __xhci_usb_type_to_str(int type) {
    switch(type) {
        case USB_TYPE_KEYBOARD:
            return "USB Keyboard";
        case USB_TYPE_MOUSE:
            return "USB Mouse";
        default:
            return "Unsupported";
    };
}

int log2(unsigned int x) {
    int result = -1;
    while (x) {
        x >>= 1;
        result++;
    }
    return result;
}

bool usb_device_self_test(xhci_device_t* dev, xhci_usb_device_t* usbdev) {
    xhci_normal_trb_t trb = {};
    trb.info_s.type = 8;
    trb.info_s.ioc = 1;

    __xhci_clear_event(dev);

    __xhci_port_ring_queue(usbdev->transfer_ring,(xhci_trb_t*)&trb);

    arch::memory_barrier();

    __xhci_doorbell_id(dev,usbdev->slotid,1);
    time::timer->sleep(5000);

    xhci_trb_t ret1 = __xhci_event_wait(dev,TRB_TRANSFEREVENT_TYPE);

    if(ret1.base == 0xDEAD) {
        log("xhci", "xhci usb device self test timeouted");
        return false;
    }

    if(ret1.ret_code != 1) {
        log("xhci", "xhci usb device self test is not ok (%d)",ret1.ret_code);
        return false;
    }

    log("xhci", "xhci usb device self test is ok");
    return true;
}

void __xhci_init_dev(xhci_device_t* dev,int portnum) {

    volatile uint32_t* portsc = __xhci_portsc(dev,portnum);
    uint32_t load_portsc = *portsc;
    
    if(!(load_portsc & 1)) 
        return; 
    
    int status = __xhci_reset_dev(dev,portnum);
    if(!status)
        return;

    int id = __xhci_enable_slot(dev,portnum);
    xhci_usb_device_t* usb_dev = __xhci_alloc_dev(portnum,id);
    //String::memset(usb_dev,0,sizeof(xhci_usb_device_t));

    klibc::memset(usb_dev->doorbell_values,0,30);
    
    usb_dev->interface = 0;

    uint64_t addr = pmm::freelist::alloc_4k();
    __xhci_create_dcbaa(dev,usb_dev->slotid,addr);

    uint32_t* hccparams = (uint32_t*)(&dev->cap->hccparams1);
    char context_size = ((hccparams1_t*)hccparams)->contextsize;


    if(!context_size)
        dev->dcbaa[id] += 64;
    else
        dev->dcbaa[id] += 128; 

    if(!context_size)
        usb_dev->_is64byte = 0;
    else 
        usb_dev->_is64byte = 1;

    usb_dev->dev = dev;

    load_portsc = *portsc; // load it again

    uint16_t speed = __xhci_get_speed(dev,load_portsc);

    const char* speed_to_str[7] = {
        "(0 MB/s - USB ?)",
        "(12 MB/s - USB 2.0)",
        "(1.5 Mb/s - USB 2.0)",
        "(480 Mb/s - USB 2.0)",
        "(5 Gb/s - USB3.0)",
        "(10 Gb/s - USB 3.1)"
    };

    assert((addr % 64) == 0, "addr is not aligned (kernel bug)");
    assert((usb_dev->transfer_ring->phys % 16) == 0, "sfaf");

    if(!context_size) {
        xhci_input_ctx_t* input_ctx = (xhci_input_ctx_t*)(addr + etc::hhdm());

        klibc::memset(input_ctx,0,4096);
        input_ctx->input_ctx.A = (1 << 0) | (1 << 1);
        input_ctx->slot.contextentries = 1;
        input_ctx->slot.speed = (load_portsc & 0x3C00) >> 10;
        input_ctx->slot.porthubnum = portnum + 1;
        input_ctx->ep0.endpointtype = 4;
        input_ctx->ep0.cerr = 3;
        input_ctx->ep0.maxpacketsize = speed;
        input_ctx->ep0.base = usb_dev->transfer_ring->phys | usb_dev->transfer_ring->cycle;
        input_ctx->ep0.averagetrblen = 0x8;
        usb_dev->input_ctx = (xhci_input_ctx_t*)input_ctx;

    } else {
        xhci_input_ctx64_t* input_ctx = (xhci_input_ctx64_t*)(addr  + etc::hhdm());

        klibc::memset(input_ctx,0,4096);
        input_ctx->input_ctx.A = (1 << 0) | (1 << 1);
        input_ctx->slot.contextentries = 1;
        input_ctx->slot.speed = (load_portsc & 0x3C00) >> 10;
        input_ctx->slot.porthubnum = portnum + 1;
        input_ctx->ep0.endpointtype = 4;
        input_ctx->ep0.cerr = 3;
        input_ctx->ep0.maxpacketsize = speed;
        input_ctx->ep0.base = usb_dev->transfer_ring->phys | usb_dev->transfer_ring->cycle;
        input_ctx->ep0.averagetrblen = 8;

        usb_dev->input_ctx = (xhci_input_ctx_t*)input_ctx;

    }

    int status_addr = __xhci_set_addr(dev,addr,id,0);

    if(status_addr == 0) {
        log("xhci","zero ret from xhci_set_addr (broken xhci ?)");
    } else if(status_addr != 1) 
        return;

    // bool self_test = usb_device_self_test(dev, usb_dev);
    // if(self_test == false)
    //     return;

    xhci_usb_descriptor_t* descriptor = (xhci_usb_descriptor_t*)klibc::malloc(4096);
    int status2 = __xhci_get_usb_descriptor(dev,usb_dev,(void*)descriptor,8);

    if(!status2)
        return;

    ((xhci_input_ctx_t*)(dev->dcbaa[id]  + etc::hhdm()))->ep0.maxpacketsize = descriptor->maxpacketsize;

    status2 = __xhci_get_usb_descriptor(dev,usb_dev,(void*)descriptor,descriptor->head.len);
    if(!status2)
        return;
    usb_dev->desc = descriptor;

    usb_dev->type = 0;

    if(!speed) {
        log("xhci","Broken USB Device/Firmware, can't continue work. skipping");
        return; 
    }

    char product[1024];
    char manufacter[1024];

    klibc::memset(product,0,1024);
    klibc::memset(manufacter,0,1024);

    int status4 = __xhci_print_device_info(dev,usb_dev,product,manufacter);
    if(!status4)
        return;

    xhci_config_descriptor_t* cfg = (xhci_config_descriptor_t*)klibc::malloc(4096);
    usb_dev->config = cfg;

    __xhci_get_config_descriptor(dev,usb_dev,cfg);

    __xhci_setup_config(dev,usb_dev,cfg->configval);

    if(!context_size) {
        usb_dev->input_ctx->input_ctx.A = (1 << 0);
    } else {
        xhci_input_ctx64_t* input = (xhci_input_ctx64_t*)(addr + etc::hhdm());
        input->input_ctx.A = (1 << 0);
    }

    uint16_t i = 0;
    while(i < (cfg->len - cfg->head.len)) {
        xhci_usb_descriptor_header* current = (xhci_usb_descriptor_header*)(&cfg->data[i]);
        //INFO("Found descriptor with type %d (0x%p) and len %d (i %d)!",current->type,current->type,current->len,i);
        
        xhci_interface_t* inter = new xhci_interface_t;
        inter->type = current->type;
        inter->data = current;
        klibc::memset(inter,0,sizeof(xhci_interface_t));
        if(!usb_dev->interface) 
            usb_dev->interface = inter;
        else {
            inter->next = usb_dev->interface;
            usb_dev->interface = inter;
        }

        switch(current->type) {

            case 0x04: {
                xhci_interface_descriptor_t* interface = (xhci_interface_descriptor_t*)current;
                //INFO("interface interclass %d, interface intersubclass %d, protocol %d",interface->interclass,interface->intersubclass,interface->protocol);
                if(interface->interclass == 3 && interface->intersubclass == 1 && !usb_dev->type) {
                    switch(interface->protocol) {
                        case 2: {
                            usb_dev->type = USB_TYPE_MOUSE;
                            break;
                        }

                        case 1: {
                            usb_dev->type = USB_TYPE_KEYBOARD;
                            break;
                        }
                    }
                }
                break;
            }

            case 0x21: {
                xhci_hid_descriptor_t* hid = (xhci_hid_descriptor_t*)current;
                for(int i = 0; i < hid->numdesc;i++) {
                    xhci_hid_sub_desc* desc = &hid->desc[i];
                    if(desc->desctype == 0x22) {
                        
                        xhci_interface_t* need_interface = usb_dev->interface;
                        while(need_interface) {
                            if(need_interface->type == 0x04)
                                break;
                            need_interface = need_interface->next;
                        }
                        
                        if(!need_interface)
                            continue;

                        need_interface->buffer = klibc::malloc(desc->desclen + 1);
                        klibc::memset(need_interface->buffer,0,desc->desclen + 1);
                        need_interface->len = desc->desclen;

                        xhci_interface_descriptor_t* interface = (xhci_interface_descriptor_t*)need_interface;

                        __xhci_set_boot_protocol(dev,usb_dev,interface->num);
                        __xhci_get_hid_report(dev,usb_dev,0,interface->num,need_interface->buffer,need_interface->len);

                    }
                }
                break;
            }
            // __xhci_setup_port_ring(256,slotid);
            case 0x05: {
                xhci_endpoint_descriptor_t* ep = (xhci_endpoint_descriptor_t*)current;

                int idx = ((ep->endpointaddr >> 7) & 1) + ((ep->endpointaddr & 0x0F) << 1);
                usb_dev->main_ep = idx;
                idx -= 2;
                usb_dev->ep_ctx[idx] = __xhci_setup_port_ring(256,usb_dev->slotid);
                
                usb_dev->buffers_need_size[idx] = ep->maxpacketsize;

                usb_dev->phys_buffers[idx] = pmm::freelist::alloc_4k();
                usb_dev->buffers[idx] = (uint8_t*)(usb_dev->phys_buffers[idx]  + etc::hhdm());
                usb_dev->doorbell_values[idx] = idx + 2;

                if(!context_size) { 
                    xhci_input_ctx_t* input = (xhci_input_ctx_t*)(addr  + etc::hhdm());
                    xhci_endpoint_ctx_t* ep1 = &input->ep[idx];
                    klibc::memset(ep1,0,sizeof(xhci_endpoint_ctx_t));
                    usb_dev->input_ctx->input_ctx.A |= (1 << (idx + 2));
                    
                    input->slot.contextentries += 2;
                    ep1->state = 0;
                    ep1->endpointtype = __xhci_ep_to_type(ep);
                    ep1->maxpacketsize = ep->maxpacketsize;
                    ep1->some_shit_with_long_name_lo = ep->maxpacketsize;
                    ep1->cerr = 3;
                    ep1->maxburstsize = 0;
                    ep1->averagetrblen = ep->maxpacketsize;
                    ep1->base = usb_dev->ep_ctx[idx]->phys | usb_dev->ep_ctx[idx]->cycle;
                    uint8_t port_speed = (load_portsc & 0x3C00) >> 10;
                    uint8_t interval = ep->interval;
                    uint8_t epty = ep1->endpointtype;

                    /* thanks n00byedge for tips */

                    if(port_speed == XHCI_USB_SPEED_HIGH_SPEED || port_speed == XHCI_USB_SPEED_SUPER_SPEED || port_speed == XHCI_USB_SPEED_SUPER_SPEED_PLUS)
                        ep1->interval = interval - 1;
                    else if(port_speed == XHCI_USB_SPEED_FULL_SPEED && (epty == XHCI_ENDPOINTTYPE_ISOCHRONOUS_IN || epty == XHCI_ENDPOINTTYPE_ISOCHRONOUS_OUT))
                        ep1->interval = interval + 2;
                    else if((port_speed == XHCI_USB_SPEED_FULL_SPEED || port_speed == XHCI_USB_SPEED_LOW_SPEED) && (epty == XHCI_ENDPOINTTYPE_INTERRUPT_IN || epty == XHCI_ENDPOINTTYPE_INTERRUPT_OUT)) {
                        ep1->interval = log2(interval) + 4;
                    }

                    
                } else {
                    xhci_input_ctx64_t* input = (xhci_input_ctx64_t*)(addr  + etc::hhdm());
                    xhci_endpoint_ctx_t* ep1 = (xhci_endpoint_ctx_t*)(&input->ep[idx]);
                    klibc::memset(ep1,0,sizeof(xhci_endpoint_ctx_t));
                    
                    input->slot.contextentries += 2;
                    input->input_ctx.A |= (1 << (idx + 2));
                    ep1->state = 0;
                    ep1->endpointtype = __xhci_ep_to_type(ep);
                    ep1->maxpacketsize = ep->maxpacketsize;
                    ep1->some_shit_with_long_name_lo = ep->maxpacketsize;
                    ep1->cerr = 3;
                    ep1->maxburstsize = 0;
                    ep1->averagetrblen = ep->maxpacketsize;
                    ep1->base = (uint64_t)usb_dev->ep_ctx[idx]->phys | usb_dev->ep_ctx[idx]->cycle;
                    uint8_t port_speed = (load_portsc & 0x3C00) >> 10;
                    uint8_t interval = ep->interval;
                    uint8_t epty = ep1->endpointtype;

                    if(port_speed == XHCI_USB_SPEED_HIGH_SPEED || port_speed == XHCI_USB_SPEED_SUPER_SPEED || port_speed == XHCI_USB_SPEED_SUPER_SPEED_PLUS)
                        ep1->interval = interval - 1;
                    else if(port_speed == XHCI_USB_SPEED_FULL_SPEED && (epty == XHCI_ENDPOINTTYPE_ISOCHRONOUS_IN || epty == XHCI_ENDPOINTTYPE_ISOCHRONOUS_OUT))
                        ep1->interval = interval + 2;
                    else if((port_speed == XHCI_USB_SPEED_FULL_SPEED || port_speed == XHCI_USB_SPEED_LOW_SPEED) && (epty == XHCI_ENDPOINTTYPE_INTERRUPT_IN || epty == XHCI_ENDPOINTTYPE_INTERRUPT_OUT)) {
                        ep1->interval = log2(interval) + 4;
                    }
                }

                break;
            }
        }
        
        i += current->len;
    }

    xhci_configure_endpoints_trb_t ep_trb;
    klibc::memset(&ep_trb,0,sizeof(xhci_trb_t));
    
    ep_trb.info_s.type = 12;
    ep_trb.base = addr;
    ep_trb.info_s.slot = usb_dev->slotid;

    __xhci_clear_event(dev);

    __xhci_command_ring_queue(dev,dev->com_ring,(xhci_trb_t*)&ep_trb);
    __xhci_doorbell(dev,0);
    time::timer->sleep(5000);

    xhci_trb_t ret = __xhci_event_wait(dev,TRB_COMMANDCOMPLETIONEVENT_TYPE);

    if(ret.ret_code > 1) {
        log("xhci","Can't configure endpoints for port %d (ret %d base %p), context_size: %d, given addr %p",portnum,ret.ret_code,ret.base,usb_dev->_is64byte,ep_trb.base);
        return;
    } else if(ret.ret_code == 0) {
        log("xhci","Endpoint configure TRB ret_code is 0 (it shouldn't)");
    }
    
    __xhci_ask_for_help_hid(dev,usb_dev);

    time::timer->sleep(1000);

    log("xhci","Found USB%s Device %s (%d, %d, %d) %s %s on port %d and slot %d",dev->usb3ports[portnum] == 1 ? "3.0" : "2.0",((load_portsc & 0x3C00) >> 10) < 7 ? speed_to_str[(load_portsc & 0x3C00) >> 10] : "Broken (0 MB/S - USB ?)",(load_portsc & 0x3C00) >> 10,speed,descriptor->maxpacketsize,manufacter,product,portnum,usb_dev->slotid);
    
    char buffer[2048];
    klibc::memset(buffer,0,2048);
    klibc::__printfbuf(buffer,2048,"%s %s",manufacter, product);
    
    usb_dev->evdev_num = evdev::create(buffer,usb_dev->type == USB_TYPE_KEYBOARD ? EVDEV_TYPE_KEYBOARD : EVDEV_TYPE_MOUSE);

}

void __xhci_init_ports(xhci_device_t* dev) {
    for(std::uint32_t i = 0;i < dev->max_ports;i++) {
        __xhci_init_dev(dev,i);
    }
}

void __xhci_iterate_usb_ports(xhci_device_t* dev) {

    volatile uint32_t* cap = (volatile uint32_t*)(dev->xhci_virt_base + dev->cap->hccparams1.xECP * 4);
    
    xhci_ext_cap_t load_cap;
    load_cap.full = *cap;
    //INFO("0x%p",cap);
    while(1) {
        //INFO("Found cap with id %d",load_cap.id);

        if(load_cap.id == 2) {

            xhci_usb_cap_t usb_cap;

            usb_cap.firsthalf = cap[0];
            usb_cap.name = cap[1];
            usb_cap.thirdhalf = cap[2];
            usb_cap.fourhalf = cap[3];

            if(usb_cap.major == 3) {
                for(uint8_t i = usb_cap.portoffset - 1;i <= (usb_cap.portoffset - 1) + usb_cap.portcount - 1;i++) {
                    dev->usb3ports[i] = 1;
                    //log("xhci","Found USB 3.0 Port %d !",i);
                }
            } else {
                for(uint8_t i = usb_cap.portoffset - 1;i <= (usb_cap.portoffset - 1) + usb_cap.portcount - 1;i++) {
                    dev->usb3ports[i] = 0;
                    //log("xhci","Found USB 2.0 Port %d !",i);
                }
            }

        } else if(load_cap.id == 1) {
            
            log("xhci","disabling xhci hand off");
            *((volatile uint32_t*)((uint64_t)cap + 4)) &= ~((1 << 0) | (1 << 13) | (1 << 4) | (1 << 14) | (1 << 15));
            *cap |= (1 << 24);

            time::timer->sleep(500 * 1000);

        }

        

        if(!load_cap.nextcap)
            break;


        cap = (uint32_t*)((uint64_t)cap + (load_cap.nextcap * 4));
        load_cap.full = *cap;

    }
}

xhci_hid_driver_t* hid_drv = 0;

void xhci_hid_register(void (*func)(xhci_usb_device_t* usbdev,xhci_done_trb_t* trb),int type) {
    xhci_hid_driver_t* drv = new xhci_hid_driver_t;
    drv->func = func;
    drv->type = type;

    if(!hid_drv)
        hid_drv = drv;
    else {
        drv->next = hid_drv;
        hid_drv = drv;
    }
}

void __xhci_process_fetch(xhci_device_t* dev) {
    xhci_event_ring_ctx_t* father = dev->event_ring;
    xhci_trb_t* buffer[1024];

    while(1) {
        //Paging::EnableKernel();  
        int count = 0;
        klibc::memset(buffer,0,sizeof(xhci_trb_t*) * 1024);
        if(get_trb(father,father->queue).info_s.cycle == father->cycle)
            count = __xhci_event_receive(dev,father,buffer);
        
        if(!count) {
            process::yield();
        }

        if(count) {
            xhci_trb_t* current = 0;
            for(int i = 0;i < count;i++) {
                current = buffer[i];
                switch(current->info_s.type) {
                    case TRB_TRANSFEREVENT_TYPE: {
                        xhci_usb_device_t* usbdev = usbdevs;
                        xhci_done_trb_t* trb = (xhci_done_trb_t*)current;
                        while(usbdev) {
                            if(usbdev->dev == dev) {
                                if(usbdev->slotid == trb->info_s.slot) {
                                    xhci_hid_driver_t* drv = hid_drv;
                                    while(drv) {
                                        if((std::uint32_t)drv->type == usbdev->type)
                                            drv->func(usbdev,trb);
                                        drv = drv->next;
                                    }
                                    __xhci_ask_for_help_hid(dev,usbdev);
                                }
                            }
                            usbdev = usbdev->next;
                        }
                        break;
                    }
                    default: {
                        log("xhci","xhci unsupported trb: %d",current->info_s.type);
                    }
                }
            }
        }
        
    }
}

void xhci_device(std::uint64_t addr) {

    xhci_device_t* dev = (xhci_device_t*)klibc::malloc(4096);
    klibc::memset(dev,0,4096);

    dev->xhci_phys_base = addr;
    dev->xhci_virt_base = addr + etc::hhdm();
    paging::map_range(gobject::kernel_root,addr,addr + etc::hhdm(), PAGE_SIZE,PAGING_RW | PAGING_PRESENT | PAGING_NC); // map page

    dev->cap = (xhci_cap_regs_t*)dev->xhci_virt_base;
    dev->op = (xhci_op_regs_t*)(dev->xhci_virt_base + dev->cap->caplength);
    dev->runtime = (xhci_runtime_regs_t*)(dev->xhci_virt_base + dev->cap->rtsoff);
    dev->doorbell = (uint32_t*)(dev->xhci_virt_base + dev->cap->dboff);

    uint32_t hcsparams1 = *(uint32_t*)(&dev->cap->hcsparams1);

    dev->max_ports = ((volatile hcsparams1_t*)&hcsparams1)->maxports;
    //std::uint32_t max_slots = ((volatile hcsparams1_t*)&hcsparams1)->maxslots;

    paging::map_range(gobject::kernel_root,addr,addr + etc::hhdm(), PAGE_SIZE * 16,PAGING_RW | PAGING_PRESENT | PAGING_NC); // map full

    __xhci_put_new_device(dev);
    if(dev->op->usbcmd & XHCI_USBCMD_RS)  { // wtf how does it running
        uint16_t timeout = XHCI_RESET_TIMEOUT;
        //INFO("XHCI is already running, stopping it.");
        dev->op->usbcmd &= ~(XHCI_USBCMD_RS);
        time::timer->sleep(20 * 1000);
        while(!(dev->op->usbsts & (1 << 0))) {
            if(!timeout) {
                log("xhci", "cant reset xhci device :(");
                return;
            }
            time::timer->sleep(20 * 1000);
            timeout = timeout - 1;
        }
    }

    uint16_t hci_version = ((*(volatile std::uint32_t*)dev->cap) >> 16) & 0xFFFF;

    uint8_t major = (hci_version >> 8) & 0xFF;
    uint8_t minor = hci_version & 0xFF;

    log("xhci", "hciversion: %d.%d (0x%04x)", major, minor, hci_version);

    //DEBUG("Resetting XHCI device");
    __xhci_reset(dev);

    dev->calculated_scratchpad_count = (uint16_t)((dev->cap->hcsparams2.max_scratchpad_hi << 5) | dev->cap->hcsparams2.max_scratchpad_lo);

    //INFO("Configuring XHCI OPER");
    __xhci_setup_op(dev); 
    //INFO("Configuring XHCI Runtime");
    __xhci_setup_run(dev);
    //INFO("Starting XHCI Device");
    __xhci_enable(dev);
    //INFO("Iterating USB Ports");
    __xhci_iterate_usb_ports(dev);
    //INFO("Configuring XHCI Ports");

    log("xhci", "iterating ports");
    __xhci_init_ports(dev);

    // int pid = fork();
    // if(pid == 0)
    //     __xhci_process_fetch(dev);

    process::kthread((void (*)(void*))__xhci_process_fetch,(void*)dev); 
    log("xhci", "xhci initializied");
}

#if defined(__x86_64__)
void __xhci_pci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    if(pci_dev.progIF != 0x30) {
        //INFO("Current USB device with progIF 0x%p is not XHCI !",pci_dev.progIF);
        return;
    } 

    uint32_t usb3_ports = x86_64::pci::pci_read_config32(a,b,c,0xDC);
    x86_64::pci::pci_write_config32(a,b,c,0xD8,usb3_ports);

	auto usb2_ports = x86_64::pci::pci_read_config32(a,b,c,0xD4);
	x86_64::pci::pci_write_config32(a,b,c,0xD0,usb2_ports);

    uint64_t addr = pci_dev.bar0 & ~4; // clear upper 2 bits
    addr |= ((uint64_t)pci_dev.bar1 << 32);

    std::uint32_t cmd = x86_64::pci::pci_read_config32(a,b,c,0x4);
    log("xhci_pci", "bus mastering %d, memory access %d",(cmd & (1 << 2)) ? 1 : 0, (cmd & (1 << 1)) ? 1 : 0);
    cmd |= 1 << 2;
    cmd |= 1 << 1;
    x86_64::pci::pci_write_config32(a,b,c,0x4,cmd);

    xhci_device(addr);
}
#endif

char hid_to_ps2_layout[0x48];

int input0_fd = 0;
int mouse_fd = 0;

void hid_layout_init() {
    hid_to_ps2_layout[0x04] = 0x1E;
    hid_to_ps2_layout[0x05] = 0x30;
    hid_to_ps2_layout[0x06] = 0x2E;
    hid_to_ps2_layout[0x07] = 0x20;
    hid_to_ps2_layout[0x08] = 0x12;
    hid_to_ps2_layout[0x09] = 0x21;
    hid_to_ps2_layout[0x0A] = 0x22;
    hid_to_ps2_layout[0x0B] = 0x23;
    hid_to_ps2_layout[0x0C] = 0x17;
    hid_to_ps2_layout[0x0D] = 0x24;
    hid_to_ps2_layout[0x0E] = 0x25;
    hid_to_ps2_layout[0x0F] = 0x26;
    hid_to_ps2_layout[0x10] = 0x32;
    hid_to_ps2_layout[0x11] = 0x31;
    hid_to_ps2_layout[0x12] = 0x18;
    hid_to_ps2_layout[0x13] = 0x19;
    hid_to_ps2_layout[0x14] = 0x10;
    hid_to_ps2_layout[0x15] = 0x13;
    hid_to_ps2_layout[0x16] = 0x1F;
    hid_to_ps2_layout[0x17] = 0x14;
    hid_to_ps2_layout[0x18] = 0x16;
    hid_to_ps2_layout[0x19] = 0x2F;
    hid_to_ps2_layout[0x1A] = 0x11;
    hid_to_ps2_layout[0x1B] = 0x2D;
    hid_to_ps2_layout[0x1C] = 0x15;
    hid_to_ps2_layout[0x1D] = 0x2C;
    hid_to_ps2_layout[0x1E] = 0x02;
    hid_to_ps2_layout[0x1F] = 0x03;
    hid_to_ps2_layout[0x20] = 0x04;
    hid_to_ps2_layout[0x21] = 0x05;
    hid_to_ps2_layout[0x22] = 0x06;
    hid_to_ps2_layout[0x23] = 0x07;
    hid_to_ps2_layout[0x24] = 0x08;
    hid_to_ps2_layout[0x25] = 0x09;
    hid_to_ps2_layout[0x26] = 0x0A;
    hid_to_ps2_layout[0x27] = 0x0B;
    hid_to_ps2_layout[0x28] = 0x1C;
    hid_to_ps2_layout[0x29] = 0x01;
    hid_to_ps2_layout[0x2A] = 0x0E;
    hid_to_ps2_layout[0x2B] = 0x0F;
    hid_to_ps2_layout[0x2C] = 0x39;
    hid_to_ps2_layout[0x2D] = 0x0C;
    hid_to_ps2_layout[0x2E] = 0x0D;
    hid_to_ps2_layout[0x2F] = 0x1A;
    hid_to_ps2_layout[0x30] = 0x1B;
    hid_to_ps2_layout[0x31] = 0x2B;
    hid_to_ps2_layout[0x32] = 0x2B;
    hid_to_ps2_layout[0x33] = 0x27;
    hid_to_ps2_layout[0x34] = 0x28;
    hid_to_ps2_layout[0x35] = 0x29;
    hid_to_ps2_layout[0x36] = 0x33;
    hid_to_ps2_layout[0x37] = 0x34;
    hid_to_ps2_layout[0x38] = 0x35;
    hid_to_ps2_layout[0x39] = 0x3A;
    hid_to_ps2_layout[0x3B] = 0x3C;
    hid_to_ps2_layout[0x3C] = 0x3D;
    hid_to_ps2_layout[0x3D] = 0x3E;
    hid_to_ps2_layout[0x3E] = 0x3F;
    hid_to_ps2_layout[0x3F] = 0x40;
    hid_to_ps2_layout[0x40] = 0x41;
    hid_to_ps2_layout[0x41] = 0x42;
    hid_to_ps2_layout[0x42] = 0x43;
    hid_to_ps2_layout[0x43] = 0x44;
    hid_to_ps2_layout[0x44] = 0x57;
    hid_to_ps2_layout[0x45] = 0x58;
    hid_to_ps2_layout[0x46] = 0x00;
    hid_to_ps2_layout[0x47] = 0x46;
}

void input_send(int num, uint8_t key) {

    std::uint64_t current_nano = time::timer->current_nano();

    input_event ev;
    ev.time.tv_sec = current_nano / 1000000000;
    ev.time.tv_usec = (current_nano & 1000000000) / 1000;
    ev.type = 1;
    ev.code = key & ~(1 << 7);
    ev.value = (key & (1 << 7)) ? 0 : 1;
    evdev::submit(num,ev);
}

uint8_t hid_mods_to_ps2[8] = {
    0x1D,
    0x2A, 
    0x38, 
    0x5B, 
    0x1D,
    0x36, 
    0x38,
    0x5C  
};

void __usbkeyboard_handler(xhci_usb_device_t* usbdev, xhci_done_trb_t* trb) {
    uint8_t* data = usbdev->buffers[trb->info_s.ep_id - 2];
    uint8_t mods = data[0];
    uint8_t prev_mods = usbdev->add_buffer[0];

    for (int i = 0; i < 8; i++) {
        uint8_t mask = 1 << i;
        if ((mods & mask) && !(prev_mods & mask)) {
            if (i >= 4) input_send(usbdev->evdev_num, 0xE0);
            input_send(usbdev->evdev_num, hid_mods_to_ps2[i]);
        } else if (!(mods & mask) && (prev_mods & mask)) {
            if (i >= 4) input_send(usbdev->evdev_num, 0xE0); 
            input_send(usbdev->evdev_num, hid_mods_to_ps2[i] | 0x80);
        }
    }

    for (int i = 2; i < 8; i++) {
        int isPressed = 0;
        for (int j = 2; j < 8; j++) {
            if (usbdev->add_buffer[j] == data[i]) {
                isPressed = 1;
                break;
            }
        }
        if (!isPressed && data[i] != 0) {
            if (data[i] < 0x47) {
                input_send(usbdev->evdev_num, hid_to_ps2_layout[data[i]]);
            } else if(data[i] == 0x4F) {
                input_send(usbdev->evdev_num, 0xE0);
                input_send(usbdev->evdev_num, 0x4D); 
            } else if(data[i] == 0x50) {
                input_send(usbdev->evdev_num, 0xE0);
                input_send(usbdev->evdev_num, 0x4B); 
            } else if(data[i] == 0x51) {
                input_send(usbdev->evdev_num, 0xE0);
                input_send(usbdev->evdev_num, 0x50); 
            } else if(data[i] == 0x52) {
                input_send(usbdev->evdev_num, 0xE0);
                input_send(usbdev->evdev_num, 0x48); 
            }
        }
    }

    for (int i = 2; i < 8; i++) {
        int isStillPressed = 0;
        for (int j = 2; j < 8; j++) {
            if (usbdev->add_buffer[i] == data[j]) {
                isStillPressed = 1;
                break;
            }
        }
        if (!isStillPressed && usbdev->add_buffer[i] != 0) {
            input_send(usbdev->evdev_num, hid_to_ps2_layout[usbdev->add_buffer[i]] | 0x80);
        } else if(usbdev->add_buffer[i] == 0x4F) {
                input_send(usbdev->evdev_num, 0xE0 | 0x80);
                input_send(usbdev->evdev_num, 0x4D | 0x80); 
            } else if(usbdev->add_buffer[i] == 0x50) {
                input_send(usbdev->evdev_num, 0xE0 | 0x80);
                input_send(usbdev->evdev_num, 0x4B | 0x80); 
            } else if(usbdev->add_buffer[i] == 0x51) {
                input_send(usbdev->evdev_num, 0xE0 | 0x80);
                input_send(usbdev->evdev_num, 0x50 | 0x80); 
            } else if(usbdev->add_buffer[i] == 0x52) {
                input_send(usbdev->evdev_num, 0xE0 | 0x80);
                input_send(usbdev->evdev_num, 0x48 | 0x80); 
            }
    }

    klibc::memcpy(usbdev->add_buffer, data, 8);
}


#define MOUSE_LB (1 << 0)
#define MOUSE_RB (1 << 1)
#define MOUSE_MB (1 << 2)
#define MOUSE_B4 (1 << 3)
#define MOUSE_B5 (1 << 4)

/*
input0_fd = open("/dev/masterps2keyboard",O_RDWR);
    mouse_fd = open("/dev/mastermouse",O_RDWR);
*/

#define REL_X			0x00
#define REL_Y			0x01
#define REL_Z			0x02
#define REL_RX			0x03
#define REL_RY			0x04
#define REL_RZ			0x05
#define REL_HWHEEL		0x06
#define REL_DIAL		0x07
#define REL_WHEEL		0x08
#define REL_MISC		0x09

void __usbmouse_handler(xhci_usb_device_t* usbdev, xhci_done_trb_t* trb) {
    uint8_t* data = usbdev->buffers[trb->info_s.ep_id - 2];
    
    mouse_packet_t packet;

    packet.buttons = 0;

    packet.buttons |= (data[0] & (1 << 0)) ? MOUSE_LB : 0;
    packet.buttons |= (data[0] & (1 << 1)) ? MOUSE_RB : 0;
    packet.buttons |= (data[0] & (1 << 2)) ? MOUSE_MB : 0;

    packet.x = data[1];
    packet.y = -data[2];
    packet.z = 0; // todo: figure out how to get mouse scroll wheel data

    std::uint64_t current_nano = time::timer->current_nano();
    input_event ev;
    ev.time.tv_sec = current_nano / 1000000000;
    ev.time.tv_usec = (current_nano & 1000000000) / 1000;
    if(packet.x) {
        ev.code = REL_X;
        ev.type = 2;
        ev.value = (packet.x & 0x80) ? (packet.x | 0xFFFFFF00) : packet.x;
        evdev::submit(usbdev->evdev_num,ev);
    }

    if(packet.y) {
        ev.code = REL_Y;
        ev.type = 2;
        ev.value = (packet.y & 0x80) ? (packet.y | 0xFFFFFF00) : packet.y;
        evdev::submit(usbdev->evdev_num,ev);
    }

    if(packet.z) {
        ev.code = REL_Z;
        ev.type = 2;
        ev.value = (packet.z & 0x80) ? (packet.z | 0xFFFFFF00) : packet.z;
        evdev::submit(usbdev->evdev_num,ev);
    }

    if((packet.buttons & MOUSE_LB) && !(usbdev->last_pack.buttons & MOUSE_LB)) {
        ev.code = 272;
        ev.type = 1;
        ev.value = 1;
        evdev::submit(usbdev->evdev_num,ev);
    } else if(!(packet.buttons & MOUSE_LB) && (usbdev->last_pack.buttons & MOUSE_LB)) {
        ev.code = 272;
        ev.type = 1;
        ev.value = 0;
        evdev::submit(usbdev->evdev_num,ev);
    }

    if((packet.buttons & MOUSE_RB) && !(usbdev->last_pack.buttons & MOUSE_RB)) {
        ev.code = 273;
        ev.type = 1;
        ev.value = 1;
        evdev::submit(usbdev->evdev_num,ev);
    } else if(!(packet.buttons & MOUSE_RB) && (usbdev->last_pack.buttons & MOUSE_RB)) {
        ev.code = 273;
        ev.type = 1;
        ev.value = 0;
        evdev::submit(usbdev->evdev_num,ev);
    }

    if((packet.buttons & MOUSE_MB) && !(usbdev->last_pack.buttons & MOUSE_MB)) {
        ev.code = 274;
        ev.type = 1;
        ev.value = 1;
        evdev::submit(usbdev->evdev_num,ev);
    } else if(!(packet.buttons & MOUSE_MB) && (usbdev->last_pack.buttons & MOUSE_MB)) {
        ev.code = 274;
        ev.type = 1;
        ev.value = 0;
        evdev::submit(usbdev->evdev_num,ev);
    }

    usbdev->last_pack = packet;

    time::timer->sleep(1000); // actually hid wants this only after 1 ms

}

int xhci_init() { 

    xhci_hid_register(__usbkeyboard_handler,USB_TYPE_KEYBOARD);
    xhci_hid_register(__usbmouse_handler,USB_TYPE_MOUSE);

    hid_layout_init();

#if defined(__x86_64__)
    x86_64::pci::reg(__xhci_pci_device,0x0C,0x03);
#endif
    return 0;
}