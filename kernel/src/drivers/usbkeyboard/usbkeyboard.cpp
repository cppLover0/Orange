
#include <drivers/xhci/xhci.hpp>
#include <drivers/usbkeyboard/usbkeyboard.hpp>
#include <generic/tty/tty.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/string.hpp>

uint8_t __hid_to_tty_kbd[] = {
    [4] = 'a',[5] = 'b',[6] = 'c',[7] = 'd',[8] = 'e',[9] = 'f',[0xA] = 'g',
    [0xB] = 'h',[0xC] = 'i',[0xD] = 'j',[0xE] = 'k',[0xF] = 'l',[0x10] = 'm',
    [0x11] = 'n',[0x12] = 'o',[0x13] = 'p',[0x14] = 'q',[0x15] = 'r',[0x16] = 's',
    [0x17] = 't',[0x18] = 'u',[0x19] = 'v',[0x1A] = 'w',[0x1B] = 'x',[0x1C] = 'y',
    [0x1D] = 'z',[0x1E] = '1',[0x1F] = '2',[0x20] = '3',[0x21] = '4',[0x22] = '5',
    [0x23] = '6',[0x24] = '7',[0x25] = '8',[0x26] = '9',[0x27] = '0',[0x28] = '\n',
    [0x29] = 27,[0x2A] = 127,[0x2B] = '\t',[0x2C] = ' ',[0x2D] = '-',[0x2E] = '=',
    [0x2F] = '[',[0x30] = ']',[0x31] = '\\', [0x32] = '\0',[0x33] = ';',[0x34] = '\'',
    [0x35] = '`',[0x36] = ',',[0x37] = '.',[0x38] = '/',[0x39] = '\0',[0x3A] = '\0',[0x3B] = '\0',
    [0x3C] = '\0',[0x3D] = '\0',[0x3E] = '\0',[0x3F] = '\0',[0x40] = '\0',[0x41] = '\0',[0x42] = '\0',
    [0x43] = '\0',[0x44] = '\0',[0x45] = '\0',[0x46] = '\0',[0x47] = '\0',[0x48] = '\0'
};

uint8_t __usb_to_upper(char c) {

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

void __usbkeyboard_send_tty(int is_released,uint8_t add,uint8_t key) {
    if(key > 3 && key < 0x48) {
        uint8_t tty_key = __hid_to_tty_kbd[key];
        if(add == 2 || add == 32) {
            tty_key = __usb_to_upper(tty_key);
        } else if(add == 1 || add == 16)
            tty_key -= 96;
            
        if(is_released)
            tty_key |= (1 << 7);

        __tty_receive_ipc(tty_key);
    }
}

void __usbkeyboard_handler(xhci_usb_device_t* usbdev, xhci_done_trb_t* trb) {
    uint8_t* data = usbdev->buffers[trb->info_s.ep_id - 2];
    uint8_t add = data[0];

    if ((add == 2 || add == 32) || usbdev->add_buffer[0] != add) {
        __tty_receive_ipc(15);
    }

    if (usbdev->add_buffer[0] == 2 || usbdev->add_buffer[0] == 32) {
        if (add != 2 && add != 32) {
            __tty_receive_ipc(15 | (1 << 7));
        }
    }

    for (int i = 2; i < 8; i++) {
        int isPressed = 0;
        for (int j = 2; j < 8; j++) {
            if (usbdev->add_buffer[j] == data[i]) {
                isPressed = 1;
                break; 
            }
        }
        if (!isPressed && data[i] != 0) { 
            __usbkeyboard_send_tty(0, add, data[i]);
        }
    }

    for (int i = 2; i < 8; i++) {
        int isStillPressed = 0; 
        for (int j = 2; j < 8; j++) {
            if (usbdev->add_buffer[i] == data[j]) {
                isStillPressed = 1; 
                break; 
            }
        }
        if (!isStillPressed && usbdev->add_buffer[i] != 0) { 
            __usbkeyboard_send_tty(1, add, usbdev->add_buffer[i]); 
        }
    }

    String::memcpy(usbdev->add_buffer, data, 8);
}


void USBKeyboard::Init() {
    XHCI::HIDRegister(__usbkeyboard_handler,USB_TYPE_KEYBOARD);
}