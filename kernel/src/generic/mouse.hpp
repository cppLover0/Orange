#pragma once 

#include <cstdint>

typedef struct {
    unsigned char buttons;
    unsigned char x;
    unsigned char y;
    unsigned char z;
} __attribute__((packed)) mouse_packet_t;

namespace mouse {
    void init();
    void submit(mouse_packet_t packet);
}