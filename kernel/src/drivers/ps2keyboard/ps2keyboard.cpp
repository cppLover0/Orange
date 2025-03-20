#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include <other/log.hpp>
#include <drivers/ps2keyboard/ps2keyboard.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <arch/x86_64/cpu/lapic.hpp>

#define PS2K_PNP_ID "PNP0303"

void (*keyboard_handler)();

static uacpi_iteration_decision match_ps2k(void *user, uacpi_namespace_node *node, uacpi_u32 depth)
{

    uacpi_resources *kb_res;

    uacpi_status ret = uacpi_get_current_resources(node, &kb_res);
    if (uacpi_unlikely_error(ret)) {
        Log("PS/2 Keyboard is not initializied\n");
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }

    Log("PS/2 Keyboard is initializied !\n");

    uacpi_resource* current_res;
    int i = 0;

    for(current_res = &kb_res->entries[0];i < kb_res->length;i += current_res->length) {

        current_res = UACPI_NEXT_RESOURCE(current_res);
        if(!current_res->length)
            break;

        if(current_res->type == UACPI_RESOURCE_TYPE_IRQ) {
            for(int v = 0; v < current_res->irq.num_irqs;v++) {
                Log("Found PS/2 Keyboard IRQ %d !\n",current_res->irq.irqs[v]);

                uint8_t vector = IDT::AllocEntry();

                idt_entry_t* entry = IDT::SetEntry(vector,(void*)keyboard_handler,0x8E);
                IOAPIC::SetEntry(vector,current_res->irq.irqs[v],(current_res->irq.polarity ? 1 : 0 << 13),Lapic::ID());
                entry->ist = 2;

                Log("Registered PS/2 Keyboard IRQ at %d\n",vector);

            }
        }

    }

    uacpi_free_resources(kb_res);
    UACPI_RESOURCE_TYPE_IRQ;

    return UACPI_ITERATION_DECISION_CONTINUE;
}

void PS2Keyboard::Init(void (*key)())
{
    keyboard_handler = key;
    uacpi_find_devices(PS2K_PNP_ID, match_ps2k, NULL);
}