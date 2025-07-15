
#include <cstdint>

#pragma once

inline std::uint64_t __rdmsr(std::uint32_t msr) {
    std::uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((std::uint64_t)hi << 32) | lo; 
}

inline void __wrmsr(std::uint32_t msr, std::uint64_t value) {
    std::uint32_t lo = (uint32_t)(value & 0xFFFFFFFF); 
    std::uint32_t hi = (uint32_t)(value >> 32);
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

inline void __cpuid(int code, int code2, std::uint32_t *a, std::uint32_t *b, std::uint32_t *c , std::uint32_t *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(code),"c"(code2));
}

inline int __cpuid_string(int code, std::uint32_t where[4]) {
    __asm__ volatile("cpuid":"=a"(*where),"=b"(*(where+1)),"=c"(*(where+2)),"=d"(*(where+3)):"a"(code));
    return (int)where[0];
}

inline std::uint64_t __rdtsc() {
    unsigned int hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}