#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include <other/log.hpp>
#include <drivers/ps2keyboard/ps2keyboard.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <generic/locks/spinlock.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/VFS/devfs.hpp>
#include <arch/x86_64/interrupts/irq.hpp>
#include <drivers/io/io.hpp>
#include <generic/VFS/devfs.hpp>

short PS2Keyboard::Get() {
    char status = IO::IN(0x64,1);

    if(status & 0x01) {
        uint8_t keycode = IO::IN(0x60,1);
        input_send(keycode);
    }

    return 0;

}

void ps2_keyboard_handler(void* arg) {
    PS2Keyboard::Get();
}

static uacpi_iteration_decision match_ps2k(void *user, uacpi_namespace_node *node, uacpi_u32 depth)
{

    uacpi_resources *kb_res;

    uacpi_status ret = uacpi_get_current_resources(node, &kb_res);
    if (uacpi_unlikely_error(ret)) {
        INFO("PS/2 Keyboard is not initializied\n");
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }

    uacpi_resource* current_res;
    int i = 0;

    for(current_res = &kb_res->entries[0];i < kb_res->length;i += current_res->length) {

        current_res = UACPI_NEXT_RESOURCE(current_res);
        if(!current_res->length)
            break;

        if(current_res->type == UACPI_RESOURCE_TYPE_IRQ) {
            for(int v = 0; v < current_res->irq.num_irqs;v++) {
                INFO("Found PS/2 Keyboard IRQ %d !\n",current_res->irq.irqs[v]);

                int vector = IRQ::Create(current_res->irq.irqs[v],IRQ_TYPE_LEGACY,ps2_keyboard_handler,0,((current_res->irq.polarity ? 1 : 0) << 13));
                INFO("Registered PS/2 Keyboard IRQ at %d\n",vector);

            }
        }

    }

    INFO("PS/2 Keyboard is initializied !\n");

    uacpi_free_resources(kb_res);
    UACPI_RESOURCE_TYPE_IRQ;

    return UACPI_ITERATION_DECISION_CONTINUE;
}

void PS2Keyboard::Init()
{
    uacpi_find_devices(PS2K_PNP_ID, match_ps2k, NULL);
}