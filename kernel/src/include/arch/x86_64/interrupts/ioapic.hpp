
#pragma once

#include <stdint.h>

class IOAPIC {
public:
    static void Init();

    static void SetEntry(uint8_t vector,uint8_t irq,uint64_t flags,uint64_t lapic);

    static void Write(uint64_t phys_base,uint32_t reg,uint32_t value);
    static uint32_t Read(uint64_t phys_base,uint32_t reg);
};