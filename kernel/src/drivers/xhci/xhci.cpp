
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

void __xhci_device(pci_t pci_dev,uint8_t a, uint8_t b,uint8_t c) {
    Log(LOG_LEVEL_DEBUG,"XHCI Initializied\n");
}

void XHCI::Init() {
    PCI::Reg(__xhci_device,0x0C,0x03);
    DLog("Registered XHCI to PCI\n");
}