
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
#include <drivers/io/io.hpp>

struct acpi_madt_ioapic io_apics[24]; 
char ioapic_entries = 0;

struct acpi_madt_interrupt_source_override iso[24];
char iso_entries = 0;

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

void __pic_disable() {
    IO::OUT(0x20,0x11,1);
    IO::OUT(0xA0,0x11,1);
    IO::OUT(0x21,0x20,1);
    IO::OUT(0xA1,0x28,1);
    IO::OUT(0x21,0x2,1);
    IO::OUT(0xA1,0x4,1);
    IO::OUT(0x21,1,1);
    IO::OUT(0xA1,1,1);
    IO::OUT(0x21,0xFF,1);
    IO::OUT(0xA1,0xFF,1);
}

void IOAPIC::Init() {
    __pic_disable();
    struct uacpi_table apic_table;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,&apic_table);
    pAssert(ret == UACPI_STATUS_OK,"Can't find APIC Table");

    struct acpi_madt* apic = (struct acpi_madt*)apic_table.virt_addr;
    struct acpi_entry_hdr* current = (struct acpi_entry_hdr*)apic->entries;

    char entry = 0;

    Log(LOG_LEVEL_INFO,"IOAPIC: Start: 0x%p, End: 0x%p\n",(uint64_t)apic->entries,(uint64_t)apic->entries + apic->hdr.length);

    while(1) {
        
        if((uint64_t)current >= (uint64_t)apic->entries + apic->hdr.length - sizeof(acpi_madt)) 
            break;

        Log(LOG_LEVEL_INFO,"MADT Entry %d: Length: 0x%p, Type: 0x%p, Addr: 0x%p\n",entry,current->length,current->type,current);

        switch(current->type) {
            
            case ACPI_MADT_ENTRY_TYPE_IOAPIC: {
                struct acpi_madt_ioapic* current_ioapic = (acpi_madt_ioapic*)current;
                Paging::KernelMap(current_ioapic->address);
                Log(LOG_LEVEL_INFO,"IOAPIC %d: Base: 0x%p, GSI_Base: 0x%p\n",ioapic_entries,current_ioapic->address,current_ioapic->gsi_base);
                String::memcpy(&io_apics[ioapic_entries],current_ioapic,sizeof(acpi_madt_ioapic));
                ioapic_entries++;
                break;
            }

            case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: {
                struct acpi_madt_interrupt_source_override* current_iso = (struct acpi_madt_interrupt_source_override*)current;
                Log(LOG_LEVEL_INFO,"ISO %d: Source: %d, GSI: %d, Bus: %d, Flags: 0x%p\n",iso_entries,current_iso->source,current_iso->gsi,current_iso->bus,current_iso->flags);
                String::memcpy(&iso[iso_entries],current_iso,sizeof(acpi_madt_interrupt_source_override));
                iso_entries++;
                break;
            }

            default:
                break;

        }

        current = (acpi_entry_hdr*)((uint64_t)current + current->length);
    
        entry++;

    }

}

char ISOtoBIT(char i) {
    i = i & 0b11;
    if(i == 0b10)
        return -1;
    else if(i == 0b11)
        return 0b1;
    else if(i == 0b01)
        return 0b0;
    return 0;

}

void IOAPIC::SetEntry(uint8_t vector,uint8_t irq,uint64_t flags,uint64_t lapic) {

    struct acpi_madt_ioapic* current_ioapic;
    struct acpi_madt_interrupt_source_override* current_iso;
    char is_success = 0;

    for(char i = 0;i < iso_entries;i++) {
        current_iso = &iso[i];

        if(current_iso->source == irq) {
            is_success = 1;
            break;
        }

    }

    uint64_t ioapic_info =  (lapic << 56) | flags | (vector & 0xFF); 

    if(!is_success) 
        current_iso = 0;
    
    is_success = 0;

    if(current_iso) {
        char polarity = (current_iso->flags & 0b11) == 0b11 ? 1 : 0;
        char mode = ((current_iso->flags >> 2) & 0b11) == 0b11 ? 1 : 0;
        ioapic_info = ((uint64_t)lapic << 56) | (mode << 15) | (polarity << 13) | (vector & 0xFF) | flags;
    }
        
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
    Write(current_ioapic->address,irq_register + 1,(uint32_t)((uint64_t)ioapic_info >> 32));

}