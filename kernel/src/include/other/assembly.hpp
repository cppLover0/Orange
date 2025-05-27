
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

inline void __invlpg(unsigned long long virt) {
    __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
}

inline void __cpuid(int code, int code2, uint32_t *a, uint32_t *b, uint32_t *c , uint32_t *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(code),"c"(code2));
}

inline int __cpuid_string(int code, uint32_t where[4]) {
    __asm__ volatile("cpuid":"=a"(*where),"=b"(*(where+1)),"=c"(*(where+2)),"=d"(*(where+3)):"a"(code));
    return (int)where[0];
}
