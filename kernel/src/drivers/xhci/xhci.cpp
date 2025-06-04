
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
    while((dev->op->usbcmd & (1 << 1)) || dev->op->usbsts & (1 << 11)) {
        if(!(--timeout)) {
            WARN("Can't reset XHCI! Timeout fault\n");
            return;
        }
        HPET::Sleep(10000);
    }

    HPET::Sleep(50000);

}

void __xhci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    if(pci_dev.progIF != 0x30)
        DEBUG("Current USB device with progIF 0x%p is not XHCI !\n",pci_dev.progIF);
    else
        DEBUG("Found USB device with progIF 0x%p is XHCI !\n",pci_dev.progIF);

    DEBUG("Parsing XHCI Device\n");

    xhci_device_t* dev = new xhci_device_t;

    uint64_t addr = pci_dev.bar0;
    addr |= ((uint64_t)pci_dev.bar1 << 32);

    dev->xhci_phys_base = addr;
    dev->xhci_virt_base = HHDM::toVirt(addr);
    Paging::Map(Paging::KernelGet(),dev->xhci_phys_base,dev->xhci_virt_base,PTE_PRESENT | PTE_RW | PTE_MMIO);
    
    DEBUG("XHCI Physical address: 0x%p (BAR0: 0x%p, BAR1: 0x%p)\n",addr,pci_dev.bar0,pci_dev.bar1);

    dev->cap = (xhci_cap_regs_t*)dev->xhci_virt_base;
    dev->op = (xhci_op_regs_t*)dev->xhci_virt_base + dev->cap->caplength;

    DEBUG("dev->cap = 0x%p, dev->op = 0x%p\n",dev->cap,dev->op);

    Paging::HHDMMap(Paging::KernelGet(),HHDM::toPhys((uint64_t)dev->op),PTE_PRESENT | PTE_RW | PTE_MMIO);

    __xhci_put_new_device(dev);

    if(dev->op->usbcmd & XHCI_USBCMD_RS)  { // wtf how does it running
        DEBUG("XHCI is already running, stopping it.\n");
        dev->op->usbcmd = 0;
        HPET::Sleep(20 * 1000);
        DEBUG("Waiting for xhci to stop\n");
        while(!(dev->op->usbsts & (1 << 0)))
            __builtin_ia32_pause();
    } else
        DEBUG("XHCI is not running\n");

    DEBUG("Resetting XHCI\n");
    __xhci_reset(dev);

    INFO("XHCI Initializied\n");

    DEBUG("Waiting 1 second !\n");
    HPET::Sleep(1000*1000);

}

void XHCI::Init() {
    PCI::Reg(__xhci_device,0x0C,0x03);
    DEBUG("Registered XHCI to PCI\n");
}