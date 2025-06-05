
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
#include <config.hpp>
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

void __xhci_reset(xhci_device_t* dev) {
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

void __xhci_reset_intr(xhci_device_t* dev, uint16_t intr) {

    if(intr > dev->cap->hcsparams1.maxintrs) {
        WARN("Intr is higher than maxintrs! Skipping");
        return;
    }

    dev->op->usbsts |= (1 << 3);
    dev->runtime->int_regs[intr].iman |= (1 << 0);
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
    DEBUG("Filling DCBAA\n");
    __xhci_fill_dcbaa(dev);
    DEBUG("Creating command ring\n");
    dev->com_ring = __xhci_create_command_ring(dev,256);
    dev->op->crcr = HHDM::toPhys((uint64_t)dev->com_ring->trb) | dev->com_ring->cycle; 
    dev->op->config = dev->cap->hcsparams1.maxslots;
    dev->op->dnctrl = 0xFFFF;
    DEBUG("OP Config: %d, crcr: 0x%p (0x%p), dcbaa: 0x%p (0x%p),usbsts: 0x%p, usbcmd: 0x%p, dnctrl: 0x%p\n",dev->op->config,dev->op->crcr,dev->com_ring->trb,dev->op->dcbaap,dev->dcbaa,dev->op->usbsts,dev->op->usbcmd,dev->op->dnctrl);

} 

void* __xhci_helper_map(uint64_t start,uint64_t pagelen) {
    for(uint64_t phys = start;phys < start + (PAGE_SIZE * (pagelen + 1));phys += PAGE_SIZE) {
        Paging::HHDMMap(Paging::KernelGet(),phys,PTE_PRESENT | PTE_RW | PTE_MMIO);
    }
    return (void*)HHDM::toVirt(start);
}

void __xhci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    if(pci_dev.progIF != 0x30) {
        DEBUG("Current USB device with progIF 0x%p is not XHCI !\n",pci_dev.progIF);
        return;
    } else
        DEBUG("Found USB device with progIF 0x%p is XHCI !\n",pci_dev.progIF);

    DEBUG("Parsing XHCI Device\n");

    xhci_device_t* dev = new xhci_device_t;

    uint64_t addr = pci_dev.bar0 & ~4; // clear upper 2 bits
    addr |= ((uint64_t)pci_dev.bar1 << 32);

    dev->xhci_phys_base = addr;
    dev->xhci_virt_base = HHDM::toVirt(addr);
    Paging::HHDMMap(Paging::KernelGet(),dev->xhci_phys_base,PTE_PRESENT | PTE_RW | PTE_MMIO);
    
    DEBUG("XHCI Physical address: 0x%p (BAR0: 0x%p, BAR1: 0x%p)\n",addr,pci_dev.bar0,pci_dev.bar1);

    dev->cap = (xhci_cap_regs_t*)dev->xhci_virt_base;
    dev->op = (xhci_op_regs_t*)HHDM::toVirt(dev->xhci_phys_base + dev->cap->caplength);
    dev->runtime = (xhci_runtime_regs_t*)HHDM::toVirt(dev->xhci_phys_base + dev->cap->rtsoff);
    __xhci_helper_map(HHDM::toPhys(dev->xhci_phys_base + dev->cap->rtsoff),8 + 1); // spec says what length is 0x8000 so ill map 8 pages + 1 

    DEBUG("dev->cap = 0x%p, dev->op = 0x%p (dev->cap length 0x%p), dev->runtime = 0x%p\n",dev->cap,dev->op,dev->cap->caplength,dev->runtime);

    Paging::HHDMMap(Paging::KernelGet(),HHDM::toPhys((uint64_t)dev->op),PTE_PRESENT | PTE_RW | PTE_MMIO);

    __xhci_put_new_device(dev);

    if(dev->op->usbcmd & XHCI_USBCMD_RS)  { // wtf how does it running
        uint16_t timeout = XHCI_RESET_TIMEOUT;
        DEBUG("XHCI is already running, stopping it.\n");
        dev->op->usbcmd &= ~(XHCI_USBCMD_RS);
        HPET::Sleep(20 * 1000);
        DEBUG("Waiting for xhci to stop\n");
        while(!(dev->op->usbsts & (1 << 0))) {
            __nop();
            if(!timeout) {
                WARN("Can't disable XHCI. Ignoring\n");
                break;
            }
            HPET::Sleep(20 * 1000);
            timeout = timeout - 1;
        }
    } else
        DEBUG("XHCI is not running\n");

    DEBUG("Resetting XHCI\n");
    __xhci_reset(dev);

    DEBUG("MaxPorts: %d, MaxIntrs: %d, MaxSlots: %d\n",dev->cap->hcsparams1.maxports,dev->cap->hcsparams1.maxintrs,dev->cap->hcsparams1.maxslots);
    DEBUG("IST: %d, ERSTMax: %d, MaxScratchPadHigh: %d, SPR: %d, MaxScratchPadLow: %d, ScratchPad: %d\n",dev->cap->hcsparams2.ist,dev->cap->hcsparams2.erstmax,dev->cap->hcsparams2.max_scratchpad_hi,dev->cap->hcsparams2.spr,dev->cap->hcsparams2.max_scratchpad_lo,(uint16_t)((dev->cap->hcsparams2.max_scratchpad_hi << 5) | dev->cap->hcsparams2.max_scratchpad_lo));
    dev->calculated_scratchpad_count = (uint16_t)((dev->cap->hcsparams2.max_scratchpad_hi << 5) | dev->cap->hcsparams2.max_scratchpad_lo);

    if(dev->cap->hcsparams1.maxports < 0x1 || dev->cap->hcsparams1.maxports > 0xFF)
        WARN("XHCI maxports is higher 0xFF or lower than 0x1 (%d).",dev->cap->hcsparams1.maxports);

    DEBUG("Configuring XHCI OP\n");
    __xhci_setup_op(dev);

    INFO("XHCI Initializied\n");

    DEBUG("Waiting 1 second !\n");
    HPET::Sleep(3*1000*1000);

}

void XHCI::Init() {
    PCI::Reg(__xhci_device,0x0C,0x03);
    DEBUG("Registered XHCI to PCI\n");
}