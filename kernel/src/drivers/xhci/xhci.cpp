
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
        ctx->trb[ctx->trb_limit - 1].info = (6 << 10) | (1 << 1) | (ctx->cycle << 0); // setup trb type and do ctx->cycle
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

    dev->dcbaa = HHDM::toPhys((uint64_t)dcbaa);
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

void* __xhci_helper_map(uint64_t start,uint64_t pagelen) {
    for(uint64_t phys = start;phys < start + (PAGE_SIZE * (pagelen + 1));phys += PAGE_SIZE) {
        Paging::HHDMMap(Paging::KernelGet(),phys,PTE_PRESENT | PTE_RW | PTE_MMIO);
    }
    return (void*)HHDM::toVirt(start);
}

void __xhci_testings(xhci_device_t* dev) {
    xhci_trb_t trb;
    String::memset(&trb,0,sizeof(xhci_trb_t));
    trb.info_s.type = TRB_ENABLESLOTCOMMAND_TYPE;
    __xhci_command_ring_queue(dev,dev->com_ring,&trb);
    INFO("Sending TRB_ENABLESLOTCOMMAND to XHCI Controller\n");
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

void __xhci_event(void* arg) {
    INFO("Got XHCI Event !\n");
}

void __xhci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    if(pci_dev.progIF != 0x30) {
        INFO("Current USB device with progIF 0x%p is not XHCI !\n",pci_dev.progIF);
        return;
    } else
        INFO("Found USB device with progIF 0x%p is XHCI !\n",pci_dev.progIF);

    xhci_device_t* dev = new xhci_device_t;

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

    if(dev->cap->hcsparams1.maxports < 0x1 || dev->cap->hcsparams1.maxports > 0xFF)
        WARN("XHCI maxports is higher 0xFF or lower than 0x1 (%d).",dev->cap->hcsparams1.maxports);

    INFO("Configuring XHCI OPER\n");
    __xhci_setup_op(dev); 

    INFO("Configuring XHCI Runtime\n");
    __xhci_setup_run(dev);

    INFO("Starting XHCI Device\n");
    __xhci_enable(dev);

    int xhci_fetch = Process::createProcess((uint64_t)__xhci_process_fetch,0,0,0,0);
    
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