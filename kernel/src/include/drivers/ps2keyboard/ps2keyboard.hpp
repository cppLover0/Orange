
#include <stdint.h>

#pragma once

#define PS2K_PNP_ID "PNP0303"
#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA

class PS2Keyboard {
public:
    static void Init(void (*key)());
    static void EOI();
    static short Get();
};