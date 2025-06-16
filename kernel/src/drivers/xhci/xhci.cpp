
#include <stdint.h>
#include <drivers/xhci/xhci.hpp>
#include <other/log.hpp>
#include <other/debug.hpp>
#include <generic/memory/heap.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include <drivers/pci/pci.hpp>
#include <other/assembly.hpp>
#include <other/hhdm.hpp>
#include <drivers/hpet/hpet.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <config.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <other/string.hpp>

xhci_device_t* xhci_list;

void __xhci_put_new_device(xhci_device_t* dev) {
    if(!dev) {
        WARN("Got zero xhci_device_t* dev. Skipping.\n");
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
    *dev->doorbell = value;
}

void __xhci_doorbell_id(xhci_device_t* dev,uint32_t idx,uint32_t val) {
    dev->doorbell[idx] = val;
}

void __xhci_reset(xhci_device_t* dev) {

    HPET::Sleep(50*1000); // wait some time

    dev->op->usbcmd |= (1 << 1);
    uint16_t timeout = XHCI_RESET_TIMEOUT;
    while(dev->op->usbsts & (1 << 11)) {
        if(!timeout) {
            WARN("Can't reset XHCI Controller, ignoring\n");
            break;
        }
        HPET::Sleep(5 * 1000);
        timeout = timeout - 1;
    }

    HPET::Sleep(50000);

}

void __xhci_enable(xhci_device_t* dev) {
    HPET::Sleep(50*1000);
    dev->op->usbcmd |= (1 << 0);
    uint16_t timeout = XHCI_RESET_TIMEOUT;
    while(dev->op->usbsts & (1 << 0)) {
        if(!timeout) {
            WARN("Can't start XHCI Controller, crying\n");
            break;
        }
        HPET::Sleep(5 * 1000);
        timeout = timeout - 1;
    }

    HPET::Sleep(50000);

}

void __xhci_reset_intr(xhci_device_t* dev, uint16_t intr) {

    if(intr > dev->cap->hcsparams1.maxintrs) {
        WARN("Intr is higher than maxintrs! Skipping");
        return;
    }

    dev->op->usbsts |= (1 << 3);
    dev->runtime->int_regs[intr].iman |= (1 << 0);
}

void __xhci_punch_intr(xhci_device_t* dev,uint16_t intr) {
    dev->runtime->int_regs[intr].erdp.event_busy = 1;
}

void __xhci_update_stepfather(xhci_device_t* dev,xhci_event_ring_ctx_t* grandpa) {
    grandpa->father->erdp_val = grandpa->table[0].base + (grandpa->queue * sizeof(xhci_trb_t));
}

xhci_trb_t* __xhci_move_father(xhci_device_t* dev,xhci_event_ring_ctx_t* ctx) {
    xhci_trb_t* trb = &ctx->trb[ctx->queue++];
    if(ctx->queue == ctx->trb_limit) {
        ctx->cycle = !ctx->cycle;
        ctx->queue = 0;
    }
    return trb;
}

xhci_event_ring_ctx_t* __xhci_create_event_ring(xhci_device_t* dev,uint16_t trb_size,IR_t* stepfather,uint16_t sons) {
    xhci_event_ring_ctx_t* ring_info = new xhci_event_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->father = stepfather; // :pensive:
    ring_info->trb_limit = trb_size;
    ring_info->trb = 0;

    ring_info->table = (xhci_erst_t*)PMM::VirtualBigAlloc(ALIGNPAGEUP(sons * sizeof(xhci_erst_t) / 4096));

    uint64_t phys_trb = PMM::BigAlloc(16);
    ring_info->table[0].base = phys_trb;
    ring_info->table[0].size = trb_size;

    ring_info->trb = (xhci_trb_t*)HHDM::toVirt(phys_trb); 

    stepfather->erstsz = sons;
    __xhci_update_stepfather(dev,ring_info);
    stepfather->erstba = HHDM::toPhys((uint64_t)ring_info->table);

    return ring_info;

}

xhci_trb_t get_trb(xhci_event_ring_ctx_t* father,uint16_t idx) {
    return father->trb[idx];
}

int __xhci_event_receive(xhci_device_t* dev,xhci_event_ring_ctx_t* father,xhci_trb_t** out) { // it will return trb virtual addresses 
    int len = 0;
    while(get_trb(father,father->queue).info_s.cycle == father->cycle)
        out[len++] = __xhci_move_father(dev,father);
    __xhci_punch_intr(dev,0);
    __xhci_update_stepfather(dev,father);
    return len;
}

xhci_command_ring_ctx_t* __xhci_create_command_ring(xhci_device_t* dev,uint16_t trb_size) {
    xhci_command_ring_ctx_t* ring_info = new xhci_command_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->trb_limit = trb_size;

    ring_info->trb = (xhci_trb_t*)PMM::VirtualBigAlloc(ALIGNPAGEUP(trb_size * sizeof(xhci_trb_t) / 4096));
    ring_info->trb[trb_size - 1].base = HHDM::toPhys((uint64_t)ring_info->trb); // loop it
    ring_info->trb[trb_size - 1].info = (6 << 10) | (1 << 1) | (1 << 0);
    return ring_info;
}


void __xhci_command_ring_queue(xhci_device_t* dev,xhci_command_ring_ctx_t* ctx,xhci_trb_t* trb) {
    trb->info |= ctx->cycle;
    String::memcpy(&ctx->trb[ctx->queue++],trb,sizeof(xhci_trb_t));
    if(ctx->queue == ctx->trb_limit) {
        ctx->queue = 0;
        ctx->trb[ctx->trb_limit - 1].info = (6 << 10) | (1 << 1) | (ctx->cycle << 0); 
        ctx->cycle = !ctx->cycle;
    }
}

void __xhci_setup_run(xhci_device_t* dev) {
    IR_t* head = &dev->runtime->int_regs[0];
    head->iman |= (1 << 1);
    
    dev->event_ring = __xhci_create_event_ring(dev,256,head,1);

    __xhci_reset_intr(dev,0);
} 

void __xhci_fill_dcbaa(xhci_device_t* dev) {
    uint64_t* dcbaa = (uint64_t*)PMM::VirtualBigAlloc(ALIGNPAGEUP(8*dev->cap->hcsparams1.maxslots) / PAGE_SIZE);

    if(dev->calculated_scratchpad_count) {
        uint64_t* list = (uint64_t*)PMM::VirtualBigAlloc(ALIGNPAGEUP(8*dev->calculated_scratchpad_count) / PAGE_SIZE);
        for(uint16_t i = 0;i < dev->calculated_scratchpad_count;i++) {
            uint64_t hi = PMM::Alloc();
            list[i] = hi;
        }
        dcbaa[0] = HHDM::toPhys((uint64_t)list);
    }

    dev->dcbaa = (uint64_t*)dcbaa;
    dev->op->dcbaap = HHDM::toPhys((uint64_t)dcbaa);
    HPET::Sleep(5000);
}

void __xhci_setup_op(xhci_device_t* dev) {
    __xhci_fill_dcbaa(dev);
    dev->com_ring = __xhci_create_command_ring(dev,128); // i dont think what command ring will be fat 
    dev->op->crcr = HHDM::toPhys((uint64_t)dev->com_ring->trb) | dev->com_ring->cycle; 
    dev->op->config = dev->cap->hcsparams1.maxslots;
    dev->op->dnctrl = 0xFFFF;
} 

uint32_t* __xhci_portsc(xhci_device_t* dev,uint32_t portnum) {
    return ((uint32_t*)HHDM::toVirt(dev->xhci_phys_base + 0x400 + dev->cap->caplength + (0x10 * portnum)));
}

void* __xhci_helper_map(uint64_t start,uint64_t pagelen) {
    for(uint64_t phys = start;phys < start + (PAGE_SIZE * (pagelen + 1));phys += PAGE_SIZE) {
        Paging::HHDMMap(Paging::KernelGet(),phys,PTE_PRESENT | PTE_RW | PTE_MMIO);
    }
    return (void*)HHDM::toVirt(start);
}

void __xhci_testings(xhci_device_t* dev) {
    xhci_trb_t trb;
    String::memset(&trb,0,sizeof(xhci_trb_t));
    trb.info_s.type = TRB_NOOPCOMMAND_TYPE;
    trb.info |= (1 << 5);
    __xhci_command_ring_queue(dev,dev->com_ring,&trb);
    INFO("Sending TRB_NOOPCOMMAND to XHCI Controller\n");
    __xhci_doorbell(dev,0);
    HPET::Sleep(1000*1000);
}

const char* trb_type_to_str(int type) {
    switch(type) {
        case TRB_COMMANDCOMPLETIONEVENT_TYPE:
            return "TRB_COMMAND_COMPLETION_EVENT";
        default:
            return "Unknown";
    }
}

void __xhci_process_fetch(xhci_device_t* dev) {
    xhci_event_ring_ctx_t* father = dev->event_ring;
    xhci_trb_t* buffer[1024];
    while(1) {
        int count = 0;
        String::memset(buffer,0,sizeof(xhci_trb_t*) * 1024);
        if(get_trb(father,father->queue).info_s.cycle == father->cycle)
            count = __xhci_event_receive(dev,father,buffer);

        if(count) {
            xhci_trb_t* current = 0;
            for(int i = 0;i < count;i++) {
                current = buffer[i];
                INFO("Got TRB event %s ! (%d) \n",trb_type_to_str(current->info_s.type),current->info_s.type);
            }
        }
        
    }
}

xhci_trb_t __xhci_event_wait(xhci_device_t* dev,int type) {
    xhci_event_ring_ctx_t* father = dev->event_ring;
    xhci_trb_t* buffer[1024];
    int timeout = 100;
    while(1) {
        int count = 0;
        String::memset(buffer,0,sizeof(xhci_trb_t*) * 1024);
        if(get_trb(father,father->queue).info_s.cycle == father->cycle)
            count = __xhci_event_receive(dev,father,buffer);

        if(count) {
            xhci_trb_t* current = 0;
            for(int i = 0;i < count;i++) {
                current = buffer[i];
                if(current->info_s.type == type) {
                    return *current;
                } 
            }
        }

        if(--timeout == 0) {
            xhci_trb_t t;
            t.base = 0xDEAD;
            return t;
        }

        HPET::Sleep(5000);
        
    }
}

void __xhci_clear_event(xhci_device_t* dev) {
    int count = 0;
    xhci_trb_t* buffer[1024];
    if(get_trb(dev->event_ring,dev->event_ring->queue).info_s.cycle == dev->event_ring->cycle)
            count = __xhci_event_receive(dev,dev->event_ring,buffer);
}

xhci_port_ring_ctx_t* __xhci_setup_port_ring(uint16_t trb_count,uint16_t slot) {
    xhci_port_ring_ctx_t* ring_info = new xhci_port_ring_ctx_t;
    ring_info->cycle = 1;
    ring_info->queue = 0;
    ring_info->slot = slot;
    ring_info->trb_limit = trb_count;
    ring_info->trb = (xhci_trb_t*)PMM::VirtualBigAlloc(16);

    ring_info->trb[trb_count - 1].base = HHDM::toPhys((uint64_t)ring_info->trb);
    ring_info->trb[trb_count - 1].info = (6 << 10) | (1 << 1) | (1 << 0);
    return ring_info;
}

// its same as command ring
void __xhci_port_ring_queue(xhci_port_ring_ctx_t* ctx,xhci_trb_t* trb) {
    trb->info |= ctx->cycle;
    String::memcpy(&ctx->trb[ctx->queue++],trb,sizeof(xhci_trb_t));
    if(ctx->queue == ctx->trb_limit) {
        ctx->queue = 0;
        ctx->trb[ctx->trb_limit - 1].info = (6 << 10) | (1 << 1) | (ctx->cycle << 0); 
        ctx->cycle = !ctx->cycle;
    }
}

void __xhci_create_dcbaa(xhci_device_t* dev,uint32_t slotid) {
    dev->dcbaa[slotid] = PMM::Alloc();
}

xhci_usb_device_t* __xhci_alloc_dev(uint32_t portnum,uint32_t slotid) {
    xhci_usb_device_t* dev = new xhci_usb_device_t;
    dev->portnum = portnum;
    dev->slotid = slotid;
    dev->transfer_ring = __xhci_setup_port_ring(256,slotid);
    dev->phys_input_ctx = PMM::Alloc();
    return dev;
}

int __xhci_syscall_usbtest(int_frame_t* ctx) {

    uint16_t id = 0;
    xhci_device_t* current = xhci_list;
    while(current) {
        INFO("Doorbelling xhci dev %d from proc %d\n",id++,CpuData::Access()->current->id);
        __xhci_testings(current);
        current = current->next;
    }

    return 0;
}

int __xhci_enable_slot(xhci_device_t* dev, int portnum) {
    xhci_trb_t trb;

    String::memset(&trb,0,sizeof(xhci_trb_t));
    trb.info_s.type = TRB_ENABLESLOTCOMMAND_TYPE;

    __xhci_clear_event(dev);

    __xhci_command_ring_queue(dev,dev->com_ring,&trb);
    __xhci_doorbell(dev,0);
    HPET::Sleep(10000);
    
    xhci_trb_t ret = __xhci_event_wait(dev,TRB_COMMANDCOMPLETIONEVENT_TYPE);

    if(ret.base == 0xDEAD)
        return 0;

    xhci_slot_trb_t* slot_ret = (xhci_slot_trb_t*)&ret;

    if(slot_ret->ret_code != 1)
        WARN("Can't allocate slot for port %d (ret %d)\n",portnum,ret.status & 0xFF);

    return slot_ret->info_s.slotid;

}

int __xhci_set_addr(xhci_device_t* dev,uint32_t id,char bsr) {
    xhci_set_addr_trb_t trb;
    trb.base = dev->dcbaa[id];
    trb.info_s.bsr = 0;
    trb.info_s.type = TRB_ADDRESSDEVICECOMMAND_TYPE;
    trb.info_s.slotid = id;

    __xhci_clear_event(dev);

    __xhci_command_ring_queue(dev,dev->com_ring,(xhci_trb_t*)&trb);
    __xhci_doorbell(dev,0);
    HPET::Sleep(10000);
    
    xhci_trb_t ret = __xhci_event_wait(dev,TRB_COMMANDCOMPLETIONEVENT_TYPE);

    if(ret.ret_code != 1)
        ERROR("Can't set XHCI port address (ret %d)\n",ret.status & 0xFF);

    return ret.ret_code;

}

void __xhci_send_usb_request_packet(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_usb_command_t usbcommand,void* out,uint64_t len) {
    void* status_buf = PMM::VirtualAlloc();
    void* desc_buf = PMM::VirtualAlloc();

    String::memset(out,0,len);

    xhci_setupstage_trb_t setup;
    xhci_datastage_trb_t data;
    xhci_eventdata_trb_t event0;
    xhci_statusstage_trb_t status;
    xhci_eventdata_trb_t event1;

    String::memset(&setup,0,sizeof(xhci_trb_t));
    String::memset(&data,0, sizeof(xhci_trb_t));
    String::memset(&event0,0,sizeof(xhci_trb_t));
    String::memset(&status,0,sizeof(xhci_trb_t));
    String::memset(&event1,0,sizeof(xhci_trb_t));

    String::memcpy(&setup.command,&usbcommand,sizeof(xhci_usb_command_t));

    setup.type = TRB_SETUPSTAGE_TYPE;
    setup.trt = 3;
    setup.imdata = 1;
    setup.len = 8;

    data.type = TRB_DATASTAGE_TYPE;
    data.data = HHDM::toPhys((uint64_t)desc_buf);
    data.len = len;
    data.chain = 1;
    data.direction = 1;

    event0.type = TRB_EVENTDATA_TYPE;
    event0.base = HHDM::toPhys((uint64_t)status_buf);
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

    __xhci_clear_event(dev);

    __xhci_doorbell_id(dev,usbdev->slotid,1);
    HPET::Sleep(10000);

    xhci_trb_t ret = __xhci_event_wait(dev,TRB_TRANSFEREVENT_TYPE);

    if(ret.base == 0xDEAD) {
        WARN("Timeout on port %d\n",usbdev->portnum);
    }

    HPET::Sleep(10000);

    String::memcpy(out,desc_buf,len);

    PMM::VirtualFree(desc_buf);
    PMM::VirtualFree(status_buf);
}

void __xhci_get_usb_descriptor(xhci_device_t* dev,xhci_usb_device_t* usbdev,void* out,uint64_t len) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = 1 << 8;
    usbcommand.index = 0;
    usbcommand.len = len;

    __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,len);
}

uint16_t __xhci_get_speed(xhci_device_t* dev,uint32_t portsc) {
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

void __xhci_reset_dev(xhci_device_t* dev,uint32_t portnum) {
    volatile uint32_t* portsc = (volatile uint32_t*)__xhci_portsc(dev,portnum);
    uint32_t load_portsc = *portsc;

    if(!(*portsc & (1 << 9))) {
        load_portsc |= (1 << 9);
        *portsc = load_portsc;
        HPET::Sleep(50000);
        if(!(*portsc & (1 << 9))) {
            WARN("I am overcooked...\n");
        }
    }

    uint8_t old_bit = load_portsc & (1 << 1);

    if(dev->usb3ports[portnum]) {
        load_portsc |= (1 << 31);
        *portsc = load_portsc;
    } else {
        load_portsc |= 0x10;
        *portsc = load_portsc;
    }

    uint16_t time = 25;
    
    if(dev->usb3ports[portnum]) {
        while(*portsc & (1 << 19)) {
            if(time-- == 0) {
                WARN("Can't reset USB 3.0 device with port %d, ignoring\n",portnum);
                break;
            }
            HPET::Sleep(50000);
        }
    } else {
        while((*portsc & (1 << 1)) == old_bit) {
            if(time-- == 0) {
                WARN("Can't reset USB 2.0 device with port %d, ignoring\n",portnum);
                break;
            }
            HPET::Sleep(50000);
        }
    }
    
    HPET::Sleep(500000);
 
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

void __xhci_read_usb_string(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_string_descriptor_t* out,uint8_t index,uint8_t lang) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = (3 << 8) | index;
    usbcommand.index = lang;
    usbcommand.len = sizeof(xhci_usb_descriptor_header);

    __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);

    usbcommand.len = out->head.len;

    __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
}

void __xhci_read_usb_lang_string(xhci_device_t* dev,xhci_usb_device_t* usbdev,xhci_lang_descriptor_t* out) {
    xhci_usb_command_t usbcommand;
    usbcommand.type = 0x80;
    usbcommand.request = 6;
    usbcommand.value = (3 << 8);
    usbcommand.index = 0;
    usbcommand.len = sizeof(xhci_usb_descriptor_header);

    __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);

    usbcommand.len = out->head.len;

    __xhci_send_usb_request_packet(dev,usbdev,usbcommand,out,usbcommand.len);
}

void __xhci_print_device_info(xhci_device_t* dev,xhci_usb_device_t* usb_dev) {
    xhci_lang_descriptor_t lang;

    __xhci_read_usb_lang_string(dev,usb_dev,&lang);
    uint16_t lang0 = lang.lang[0];

    xhci_string_descriptor_t product;
    xhci_string_descriptor_t manufacter;

    __xhci_read_usb_string(dev,usb_dev,&product,usb_dev->desc->product1,lang0);
    __xhci_read_usb_string(dev,usb_dev,&manufacter,usb_dev->desc->manufacter,lang0);

    char product0[256];
    char manufacter0[256];

    String::memset(product0,0,256);
    String::memset(manufacter0,0,256);

    __xhci_unicode_to_ascii(product.str,product0);
    __xhci_unicode_to_ascii(product.str,manufacter0);

    INFO("%s %s\n",product0,manufacter0);

}

void __xhci_init_dev(xhci_device_t* dev,int portnum) {


    volatile uint32_t* portsc = __xhci_portsc(dev,portnum);
    uint32_t load_portsc = *portsc;
    if(!(load_portsc & 1)) 
        return; 

    INFO("Trying to configure %s device on port %d\n",dev->usb3ports[portnum] == 1 ? "USB3.0" : "USB2.0",portnum);

    __xhci_reset_dev(dev,portnum);

    int id = __xhci_enable_slot(dev,portnum);

    xhci_usb_device_t* usb_dev = __xhci_alloc_dev(portnum,id);
    __xhci_create_dcbaa(dev,usb_dev->slotid);

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

    xhci_input_ctx_t* input_ctx = (xhci_input_ctx_t*)HHDM::toVirt(dev->dcbaa[id]);
    input_ctx->input_ctx.A = (1 << 0) | (1 << 1);
    input_ctx->slot.contextentries = 1;
    input_ctx->slot.speed = (load_portsc & 0x3C00) >> 10;
    input_ctx->slot.porthubnum = portnum + 1;
    input_ctx->ep0.endpointtype = 4;
    input_ctx->ep0.cerr = 3;
    input_ctx->ep0.maxpacketsize = speed;
    input_ctx->ep0.base = HHDM::toPhys((uint64_t)usb_dev->transfer_ring->trb) | usb_dev->transfer_ring->cycle;
    input_ctx->ep0.averagetrblen = 8;

    if(__xhci_set_addr(dev,id,0) != 1)
        return;

    dev->dcbaa[id] += 64;

    xhci_usb_descriptor_t* descriptor = (xhci_usb_descriptor_t*)PMM::VirtualAlloc();
    __xhci_get_usb_descriptor(dev,usb_dev,(void*)descriptor,8);

    input_ctx->ep0.maxpacketsize = descriptor->maxpacketsize;

    __xhci_get_usb_descriptor(dev,usb_dev,(void*)descriptor,descriptor->head.len);

    usb_dev->desc = descriptor;


    if(!speed) {
        ERROR("Broken USB Device/Firmware, can't continue work. skipping\n");
        return; //now i cant ignore anymore...
    }

    __xhci_print_device_info(dev,usb_dev);

    INFO("Found USB Device %s (%d, %d) on port %d and slot %d\n",((load_portsc & 0x3C00) >> 10) < 7 ? speed_to_str[(load_portsc & 0x3C00) >> 10] : "Broken (0 MB/S - USB ?)",(load_portsc & 0x3C00) >> 10,speed,portnum,usb_dev->slotid);

    

}

void __xhci_init_ports(xhci_device_t* dev) {
    INFO("MaxPorts count: %d\n",dev->max_ports);
    for(int i = 0;i <= dev->max_ports;i++) {
        uint32_t* portsc = __xhci_portsc(dev,i);
        __xhci_init_dev(dev,i);
    }
}

void __xhci_iterate_usb_ports(xhci_device_t* dev) {
    uint64_t phys_cap = ((dev->cap->hccparams1.xECP * 4) + dev->xhci_phys_base);
    volatile uint32_t* cap = (volatile uint32_t*)HHDM::toVirt(phys_cap);
    xhci_ext_cap_t load_cap;
    load_cap.full = *cap;
    //INFO("0x%p\n",cap);
    while(1) {
        //INFO("Found cap with id %d\n",load_cap.id);

        if(load_cap.id == 2) {

            xhci_usb_cap_t usb_cap;

            usb_cap.firsthalf = cap[0];
            usb_cap.name = cap[1];
            usb_cap.thirdhalf = cap[2];
            usb_cap.fourhalf = cap[3];

            //INFO("UsbCap with major %d\n",usb_cap.major);

            if(usb_cap.major == 3) {
                for(uint8_t i = usb_cap.portoffset - 1;i <= (usb_cap.portoffset - 1) + usb_cap.portcount - 1;i++) {
                    dev->usb3ports[i] = 1;
                    //INFO("Found USB 3.0 Port %d !\n",i);
                }
            } else {
                for(uint8_t i = usb_cap.portoffset - 1;i <= (usb_cap.portoffset - 1) + usb_cap.portcount - 1;i++) {
                    dev->usb3ports[i] = 0;
                    //INFO("Found USB 2.0 Port %d !\n",i);
                }
            }

        } else if(load_cap.id == 1) {
            *((uint32_t*)((uint64_t)cap + 4)) &= ~((1 << 0) | (1 << 13) | (1 << 4) | (1 << 14) | (1 << 15));
            *cap |= (1 << 24);

            HPET::Sleep(500 * 1000);
            if(*cap & (1 << 16)) 
                WARN("Bios is annoying, can't fuck up this\n");

        }

        

        if(!load_cap.nextcap)
            break;


        cap = (uint32_t*)((uint64_t)cap + (load_cap.nextcap * 4));
        load_cap.full = *cap;

    }
}

void __xhci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    if(pci_dev.progIF != 0x30) {
        INFO("Current USB device with progIF 0x%p is not XHCI !\n",pci_dev.progIF);
        return;
    } else
        INFO("Found USB device with progIF 0x%p is XHCI !\n",pci_dev.progIF);

    xhci_device_t* dev = (xhci_device_t*)PMM::VirtualAlloc();

    uint64_t addr = pci_dev.bar0 & ~4; // clear upper 2 bits
    addr |= ((uint64_t)pci_dev.bar1 << 32);

    dev->xhci_phys_base = addr;
    dev->xhci_virt_base = HHDM::toVirt(addr);
    Paging::HHDMMap(Paging::KernelGet(),dev->xhci_phys_base,PTE_PRESENT | PTE_RW | PTE_MMIO);

    dev->cap = (xhci_cap_regs_t*)dev->xhci_virt_base;
    dev->op = (xhci_op_regs_t*)HHDM::toVirt(dev->xhci_phys_base + dev->cap->caplength);
    dev->runtime = (xhci_runtime_regs_t*)HHDM::toVirt(dev->xhci_phys_base + dev->cap->rtsoff);
    dev->doorbell = (uint32_t*)HHDM::toVirt(dev->xhci_phys_base + dev->cap->dboff);
    __xhci_helper_map(dev->xhci_phys_base + dev->cap->rtsoff,8 + 1); // spec says what length is 0x8000 so ill map 8 pages + 1 
    __xhci_helper_map(dev->xhci_phys_base + dev->cap->caplength + 0x400,2); // 0x400-0x13FF


    Paging::HHDMMap(Paging::KernelGet(),HHDM::toPhys((uint64_t)dev->doorbell),PTE_PRESENT | PTE_RW | PTE_MMIO);
    Paging::HHDMMap(Paging::KernelGet(),HHDM::toPhys((uint64_t)dev->op),PTE_PRESENT | PTE_RW | PTE_MMIO);

    __xhci_put_new_device(dev);

    if(dev->op->usbcmd & XHCI_USBCMD_RS)  { // wtf how does it running
        uint16_t timeout = XHCI_RESET_TIMEOUT;
        INFO("XHCI is already running, stopping it.\n");
        dev->op->usbcmd &= ~(XHCI_USBCMD_RS);
        HPET::Sleep(20 * 1000);
        while(!(dev->op->usbsts & (1 << 0))) {
            __nop();
            if(!timeout) {
                WARN("Can't disable XHCI. Ignoring\n");
                break;
            }
            HPET::Sleep(20 * 1000);
            timeout = timeout - 1;
        }
    }

    //DEBUG("Resetting XHCI device\n");
    __xhci_reset(dev);

    dev->calculated_scratchpad_count = (uint16_t)((dev->cap->hcsparams2.max_scratchpad_hi << 5) | dev->cap->hcsparams2.max_scratchpad_lo);
    dev->max_ports = dev->cap->hcsparams1.maxports;

    if(dev->max_ports < 0x1 || dev->max_ports > 0xFF)
        WARN("XHCI maxports is higher 0xFF or lower than 0x1 (%d).",dev->max_ports);
    else
        INFO("XHCI maxports is good (%d)\n",dev->max_ports);

    INFO("Configuring XHCI OPER\n");
    __xhci_setup_op(dev); 

    INFO("Configuring XHCI Runtime\n");
    __xhci_setup_run(dev);

    INFO("Starting XHCI Device\n");
    __xhci_enable(dev);

    INFO("Iterating USB Ports\n");
    __xhci_iterate_usb_ports(dev);

    INFO("Configuring XHCI Ports\n");
    __xhci_init_ports(dev);

    INFO("Creating XHCI process\n");
    int xhci_fetch = Process::createProcess((uint64_t)__xhci_process_fetch,0,0,0,0);
    
    INFO("Configuring XHCI process\n");
    process_t* xhci_proc1 = Process::ByID(xhci_fetch);
    xhci_proc1->ctx.cr3 = HHDM::toPhys((uint64_t)Paging::KernelGet());  
    xhci_proc1->nah_cr3 = HHDM::toPhys((uint64_t)Paging::KernelGet());  
    xhci_proc1->ctx.rdi = (uint64_t)dev;
    Process::WakeUp(xhci_fetch);

    INFO("XHCI Initializied\n");

}

void XHCI::Init() {
    PCI::Reg(__xhci_device,0x0C,0x03);
    INFO("Registered XHCI to PCI\n");
}