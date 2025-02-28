
#include <stdint.h>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <other/log.hpp>
#include <other/assert.hpp>
#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>
#include <other/string.hpp>
#include <other/hhdm.hpp>
#include <generic/memory/paging.hpp>

struct acpi_madt_ioapic io_apics[24]; 
char ioapic_entries = 0;

void IOAPIC::Write(uint64_t phys_base,uint32_t reg,uint32_t value) {
    uint64_t virt = HHDM::toVirt(phys_base);
    *(volatile uint32_t*)virt = reg;
    *(volatile uint32_t*)(virt + 0x10) = value;
}

uint32_t IOAPIC::Read(uint64_t phys_base,uint32_t reg) {
    uint64_t virt = HHDM::toVirt(phys_base);
    *(volatile uint32_t*)virt = reg;
    return *(volatile uint32_t*)(virt + 0x10);
}

void IOAPIC::Init() {
    struct uacpi_table apic_table;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,&apic_table);
    pAssert(ret == UACPI_STATUS_OK,"Can't find APIC Table");

    struct acpi_madt* apic = (struct acpi_madt*)apic_table.virt_addr;
    struct acpi_entry_hdr* current = (struct acpi_entry_hdr*)apic->entries;

    char entry = 0;

    Log("IOAPIC: Start: 0x%p, End: 0x%p\n",(uint64_t)apic->entries,(uint64_t)apic->entries + apic->hdr.length);

    while(1) {
        
        if((uint64_t)current >= (uint64_t)apic->entries + apic->hdr.length - sizeof(acpi_madt)) 
            break;

        Log("MADT Entry %d: Length: 0x%p, Type: 0x%p, Addr: 0x%p\n",entry,current->length,current->type,current);

        switch(current->type) {
            
            case ACPI_MADT_ENTRY_TYPE_IOAPIC: {
                struct acpi_madt_ioapic* current_ioapic = (acpi_madt_ioapic*)current;
                Paging::KernelMap(current_ioapic->address);
                Log("IOAPIC %d: Base: 0x%p, GSI_Base: 0x%p\n",ioapic_entries,current_ioapic->address,current_ioapic->gsi_base);
                String::memcpy(&io_apics[ioapic_entries],current_ioapic,sizeof(acpi_madt_ioapic));
                ioapic_entries++;
                break;
            }

            default:
                break;

        }

        current = (acpi_entry_hdr*)((uint64_t)current + current->length);
    
        entry++;

    }

}

void IOAPIC::SetEntry(uint8_t vector,uint8_t irq,uint32_t flags,uint64_t lapic) {

    uint64_t ioapic_info =  (lapic << 56) | flags | (vector & 0xFF); 
    struct acpi_madt_ioapic* current_ioapic;
    char is_success = 0;

    for(char i = 0;i < ioapic_entries;i++) {
        current_ioapic = &io_apics[i];
        uint32_t apic_ver = Read(current_ioapic->address,1);
        uint32_t max_redirection_entries = apic_ver >> 16;
        if(current_ioapic->gsi_base <= irq && current_ioapic->gsi_base + max_redirection_entries > irq) {
            is_success = 1;
            break; 
        }
    }

    pAssert(is_success == 1,"Can't find a free IOAPIC entry for IRQ");

    uint32_t irq_register = ((irq - current_ioapic->gsi_base) * 2) + 0x10;
    Write(current_ioapic->address,irq_register,(uint32_t)ioapic_info);
    Write(current_ioapic->address,irq_register + 1,(uint32_t)ioapic_info >> 32);

}