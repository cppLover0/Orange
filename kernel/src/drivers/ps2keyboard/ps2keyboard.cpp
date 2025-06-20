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
#include <generic/tty/tty.hpp>

void (*keyboard_handler)();
char kmap[255] = {
 
    '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 
    (char)127, '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
    '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ', '*', 
    '\0', ' ', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', '\0', 
    '\0'
 
};
 
char __shift_pressed = 0;

char __ps_2_to_upper(char c) {

    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
 
    } 
 
    switch (c) {
 
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '`': return '~';
        case '.': return '>';
        case ',': return '<';
        case '\\': return '|';
 
        default: break; 
 
    }
 
    if (c == '\'') {
        return '\"';
    }
 
    return c; 
}

#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA
#define CTRL_PRESSED 29
#define CTRL_RELEASED 157
#define CTRL_PRESSED_OFFSET 96

char __ctrl_pressed = 0;

char __ps2_read(char keycodeps2) {

        unsigned char keycode = keycodeps2;

        if(keycode == CTRL_PRESSED)
            __ctrl_pressed = 1;
        else if(keycode == CTRL_RELEASED)
            __ctrl_pressed = 0;
        
        if(keycode == SHIFT_PRESSED)
            __tty_receive_ipc(15);
        else if(keycode == SHIFT_RELEASED)
            __tty_receive_ipc(15 | (1 << 7));

        if(keycode <= 0x58 && keycode != CTRL_PRESSED) {
            keycode = kmap[keycode];
 
            if(__shift_pressed && !__ctrl_pressed)
                keycode = __ps_2_to_upper(keycode);

            if(__ctrl_pressed)
                keycode = keycode - CTRL_PRESSED_OFFSET;
 
        }

        if(keycode & (1 << 7)) {
            return 0;
        }

        return keycode;

    return 0;

}

char __last_key = 0;

int kbd_read(char* buffer,long hint_size) {
    
    if(!buffer) return 10;

    *buffer = __last_key;

    return 0;
}

ps2keyboard_pipe_struct_t* head_ps2pipe;
ps2keyboard_pipe_struct_t* last_ps2pipe;

ps2keyboard_pipe_struct_t* __kbd_allocate_pipe() {

    if(!head_ps2pipe) {
        head_ps2pipe = new ps2keyboard_pipe_struct_t;
        head_ps2pipe->is_used_anymore = 0;
    }

    ps2keyboard_pipe_struct_t* current = head_ps2pipe;
    while(current) {
        if(!current->is_used_anymore)
            break;
        current = current->next;
    }

    if(!current) {
        current = new ps2keyboard_pipe_struct_t;
        last_ps2pipe->next = current;
        last_ps2pipe = current;
    }

    return current;


}

void __kbd_send_ipc(uint8_t keycode) {

    if(!head_ps2pipe) {
        head_ps2pipe = new ps2keyboard_pipe_struct_t;
        head_ps2pipe->is_used_anymore = 0;
    }
    
    ps2keyboard_pipe_struct_t* current = head_ps2pipe;
    while(current) { 

        if(current->is_used_anymore && current->pipe) {
            current->pipe->buffer[0] = keycode;
            current->pipe->buffer_size = 1;
            if(current->pipe->type == PIPE_WAIT)
                current->is_used_anymore = 0;
            current->pipe->is_received = 0; 
        }
        current = current->next;
    }

}

int kbd_askforpipe(pipe_t* pipe) {

    ps2keyboard_pipe_struct_t* pip = __kbd_allocate_pipe();

    pip->pipe = pipe;
    pip->is_used_anymore = 1;
    pip->pipe->type = PIPE_INSTANT;
    pip->pipe->is_used = 1;

    return 0;

}

short PS2Keyboard::Get() {
    char status = IO::IN(0x64,1);

    if(status & 0x01) {
        uint8_t keycode = IO::IN(0x60,1);

        int is_released = keycode & (1 << 7);

        if(keycode == SHIFT_PRESSED)
            __shift_pressed = 1;
        
        if(keycode == SHIFT_RELEASED)
            __shift_pressed = 0;

        __last_key = keycode;
        uint8_t vt100_keycode = __ps2_read(keycode & ~(1 << 7));
        if(vt100_keycode) {
            if(is_released)
                __tty_receive_ipc(vt100_keycode | (1 << 7));
            else 
                __tty_receive_ipc(vt100_keycode);
        }

        __kbd_send_ipc(keycode);

        return __last_key;

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

    devfs_reg_device("/kbd",0,kbd_read,kbd_askforpipe,0,0);

    INFO("PS/2 Keyboard is initializied !\n");

    uacpi_free_resources(kb_res);
    UACPI_RESOURCE_TYPE_IRQ;

    return UACPI_ITERATION_DECISION_CONTINUE;
}

void PS2Keyboard::EOI() {
    Lapic::EOI();
}


void PS2Keyboard::Init(void (*key)())
{
    keyboard_handler = key;
    uacpi_find_devices(PS2K_PNP_ID, match_ps2k, NULL);
}