
#include <cstdint>
#include <generic/mm/paging.hpp>
#include <drivers/ioapic.hpp>

#include <arch/x86_64/interrupts/pic.hpp>
#include <etc/logging.hpp>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

struct acpi_madt_ioapic apics[24];
struct acpi_madt_interrupt_source_override iso[24];
std::uint8_t apic_ent, iso_ent;
extern std::uint8_t is_legacy_pic;

void drivers::ioapic::init() {
    apic_ent = 0; iso_ent = 0;
    struct uacpi_table apic_table;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,&apic_table);
    if(ret != UACPI_STATUS_OK)
        return; // ignore just use pic later
    else {
        arch::x86_64::interrupts::pic::disable(); 
        
        is_legacy_pic = 0;
    }

    struct acpi_madt* apic = (struct acpi_madt*)apic_table.virt_addr;
    struct acpi_entry_hdr* current = (struct acpi_entry_hdr*)apic->entries;
    while(1) {

        if((std::uint64_t)current >= (std::uint64_t)apic->entries + apic->hdr.length - sizeof(acpi_madt))
            break;
    
        switch (current->type)
        {
            case ACPI_MADT_ENTRY_TYPE_IOAPIC: {
                struct acpi_madt_ioapic* cur_ioapic = (acpi_madt_ioapic*)current;
                memory::paging::kernelmap(0,cur_ioapic->address);
                apics[apic_ent++] = *cur_ioapic;
                Log::Display(LEVEL_MESSAGE_INFO,"Found APIC %d with address 0x%p\n",apic_ent - 1,cur_ioapic->address);
                break;
            }
            case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: {
                struct acpi_madt_interrupt_source_override* cur_iso = (struct acpi_madt_interrupt_source_override*)current;
                iso[iso_ent++] = *cur_iso;
                Log::Display(LEVEL_MESSAGE_INFO,"Found ISO %d with address 0x%p\n",iso_ent - 1,cur_iso);
                break;
            }
        }
        current = (acpi_entry_hdr*)((std::uint64_t)current + current->length);

    }
}

void drivers::ioapic::set(std::uint8_t vec,std::uint8_t irq,std::uint64_t flags,std::uint64_t lapic) {
    struct acpi_madt_ioapic* apic;
    struct acpi_madt_interrupt_source_override* ciso;
    std::uint8_t is_found_iso = 0;
    for(std::uint8_t i = 0;i < iso_ent;i++) {
        ciso = &iso[i];
        if(ciso->source == irq) {
            is_found_iso = 1;
            break;
        }
    }

    std::uint64_t calc_flags = (lapic << 56) | flags | (vec & 0xFF);
    if(is_found_iso) {
        char pol = (ciso->flags & 0b11) == 0b11 ? 1 : 0;
        char mode  = ((ciso->flags >> 2) & 0b11) == 0b11 ? 1 : 0;
        calc_flags = (lapic << 56) | (mode << 15) | (pol << 13) | (vec & 0xff) | flags;
    }

    for(std::uint8_t i = 0;i < apic_ent; i++) {
        apic = &apics[i];
        std::uint32_t ver = read(apic->address,1);
        std::uint32_t max = ver >> 16;
        if(apic->gsi_base <= irq && apic->gsi_base + max > irq)
            break;
    }

    std::uint32_t irqreg = ((irq - apic->gsi_base) * 2) + 0x10;
    write(apic->address,irqreg,(std::uint32_t)calc_flags);
    write(apic->address,irqreg + 1,(std::uint32_t)((std::uint64_t)calc_flags >> 32));

}