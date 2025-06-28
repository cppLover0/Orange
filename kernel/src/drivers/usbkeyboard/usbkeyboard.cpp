
#include <drivers/xhci/xhci.hpp>
#include <drivers/usbkeyboard/usbkeyboard.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/string.hpp>
#include <generic/VFS/devfs.hpp>

char hid_to_ps2_layout[] = {
    [0x04] = 0x1E, [0x05] = 0x30, [0x06] = 0x2E, [0x07] = 0x20, [0x08] = 0x12,
    [0x09] = 0x21, [0x0A] = 0x22, [0x0B] = 0x23, [0x0C] = 0x17, [0x0D] = 0x24, 
    [0x0E] = 0x25, [0x0F] = 0x26,
    [0x10] = 0x32, [0x11] = 0x31, [0x12] = 0x18, [0x13] = 0x19, [0x14] = 0x10, 
    [0x15] = 0x13, [0x16] = 0x1F, [0x17] = 0x14, [0x18] = 0x16, [0x19] = 0x2F,
    [0x1A] = 0x11, [0x1B] = 0x2D, [0x1C] = 0x15, [0x1D] = 0x2C, [0x1E] = 0x02,
    [0x1F] = 0x03, [0x20] = 0x04, [0x21] = 0x05, [0x22] = 0x06, [0x23] = 0x07,
    [0x24] = 0x08, [0x25] = 0x0A, [0x26] = 0x0B, [0x27] = 0x0B, [0x28] = 0x1C,
    [0x29] = 0x01, [0x2A] = 0x0E, [0x2B] = 0x0F, [0x2C] = 0x39, [0x2D] = 0x0C,
    [0x2E] = 0x0D, [0x2F] = 0x1A, [0x30] = 0x1B, [0x31] = 0x2B, [0x32] = 0x2B,
    [0x33] = 0x27, [0x34] = 0x28, [0x35] = 0x29, [0x36] = 0x33, [0x37] = 0x34,
    [0x38] = 0x35, [0x39] = 0x3A, [0x3B] = 0x3C, [0x3C] = 0x3D, [0x3D] = 0x3E,
    [0x3E] = 0x3F, [0x3F] = 0x40, [0x40] = 0x41, [0x41] = 0x42, [0x42] = 0x43,
    [0x43] = 0x44, [0x44] = 0x57, [0x45] = 0x58, [0x46] = 0x00, [0x47] = 0x46 
};

void __usbkeyboard_handler(xhci_usb_device_t* usbdev, xhci_done_trb_t* trb) {
    uint8_t* data = usbdev->buffers[trb->info_s.ep_id - 2];
    uint8_t add = data[0];

    if ((add == 2 || add == 32) && usbdev->add_buffer[0] != add) {
        input_send(0x2A);
    } else if ((add == 1) && usbdev->add_buffer[0] != add) {
        input_send(29);
    }

    if (usbdev->add_buffer[0] == 2 || usbdev->add_buffer[0] == 32) {
        if (add != 2 && add != 32) {
            input_send(0x2A | (1 << 7));
        }
    } else if(usbdev->add_buffer[0] == 1) {
        if(add != 1) {
            input_send(29 | (1 << 7));
            input_send(0x2A | (1 << 7));
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
            if(data[i] < 0x47) {
                input_send(hid_to_ps2_layout[data[i]]);
            }
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
            input_send(hid_to_ps2_layout[usbdev->add_buffer[i]] | (1 << 7));
        }
    }


    String::memcpy(usbdev->add_buffer, data, 8);
}


void USBKeyboard::Init() {
    XHCI::HIDRegister(__usbkeyboard_handler,USB_TYPE_KEYBOARD);
}