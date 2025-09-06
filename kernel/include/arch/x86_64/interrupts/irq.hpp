
#include <cstdint>

#pragma once

#define IRQ_TYPE_OTHER 0 
#define IRQ_TYPE_LEGACY 1 
#define IRQ_TYPE_MSI 2 
#define IRQ_TYPE_LEGACY_USERSPACE 3

typedef struct {
    void (*func)(void* arg);
    void* arg;
    char is_userspace;
} irq_t;

namespace arch {
    namespace x86_64 {
        namespace interrupts {
            class irq {
            public:
                static std::uint8_t create(std::uint16_t irq,std::uint8_t type,void (*func)(void* arg),void* arg,std::uint64_t flags);
                static void reset();
            };
        };
    };
};