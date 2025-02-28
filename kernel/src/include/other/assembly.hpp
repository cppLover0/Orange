
#include <stdint.h>

#pragma once

inline uint64_t __rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo; 
}

inline void __wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF); 
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

inline void __nop() {
    __asm__ volatile ("nop");
}

inline void __cli() {
    __asm__ volatile ("cli");
}

inline void __sti() {
    __asm__ volatile ("sti");
}

inline void __hlt() {
    __asm__ volatile ("hlt");
}
