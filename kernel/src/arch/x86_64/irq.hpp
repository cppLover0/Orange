#pragma once
#include <cstdint>

typedef struct {
    void (*func)(void* arg);
    void* arg;
    int irq;
    char is_userspace;
} irq_t;

namespace x86_64 {
    namespace irq {
        std::uint8_t create(std::uint16_t irq,std::uint8_t type,void (*func)(void* arg),void* arg,std::uint64_t flags);
    };
};