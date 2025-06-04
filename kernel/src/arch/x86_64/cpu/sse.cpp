
#include <stdint.h>
#include <other/assembly.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#include <other/log.hpp>
#include <other/debug.hpp>

uint64_t __sse_size = 0;
char __sse_is_initializied = 0;
char __sse_legacy_save = 0;

uint64_t __sse_cr4_read() {
  uint64_t val;
  asm volatile("mov %%cr4, %0" : "=r"(val));
  return val;
}

void __sse_cr4_write(uint64_t val) {
  asm volatile("mov %0, %%cr4" : : "r"(val) : "memory");
}

uint64_t __sse_cr0_read() {
  uint64_t val;
  asm volatile("mov %%cr0, %0" : "=r"(val));
  return val;
}

void __sse_cr0_write(uint64_t val) {
  asm volatile("mov %0, %%cr0" : : "r"(val) : "memory");
}


void __sse_xsetbv(uint64_t val) {
    asm volatile("xsetbv" : : "a"(val), "d"(val >> 32),"c"(0) : "memory");
}

uint64_t __sse_xgetbv() {
    uint32_t a,d;
    asm volatile("xgetbv" : "=a"(a),"=d"(d) : "c"(0) : "memory");
    return ((uint64_t)d << 32) | a;
}

void __sse_xsave(void* buf) {
    uint64_t xcr0 = __sse_xgetbv();
    asm volatile("xsave (%0)" :: "r"(buf), "a"(xcr0 & 0xFFFFFFFF), "d"(xcr0 >> 32), "c"(0): "memory");
}

void __sse_xrstor(void* buf) {
    uint64_t xcr0 = __sse_xgetbv();
    asm volatile("xrstor (%0)" :: "r"(buf), "a"(xcr0 & 0xFFFFFFFF), "d"(xcr0 >> 32), "c"(0): "memory");
}

void SSE::Init() {
    uint32_t a,b,c,d;
    __cpuid(1,0,&a,&b,&c,&d);
    if(!__sse_is_initializied) {
        if(c & SSE_XSAVE_SUPPORT) {
            __cpuid(13,0,&a,&b,&c,&d);
            INFO("CPU Supports SSE_XSAVE with size %d\n",c);
            __sse_legacy_save = 0;
            __sse_size = c;
        } else {
            INFO("CPU Doesn't supports SSE_XSAVE\n");
            __sse_legacy_save = 1;
            __sse_size = 512;
        }
        __sse_is_initializied = 1;
    }
    uint64_t cr4 = __sse_cr4_read();

    cr4 |= DEFAULT_SSE_FLAGS;

    uint64_t cr0 = __sse_cr0_read();

    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);

    __sse_cr0_write(cr0);

    uint64_t sse_control = 0;

    __sse_cr4_write(cr4);
    
    __cpuid(1,0,&a,&b,&c,&d);
    if(c & SSE_XSAVE_SUPPORT)
        cr4 |= SSE_XSAVE_CR4;
    else
        return;

    __sse_cr4_write(cr4);
    
    __cpuid(13,0,&a,&b,&c,&d);

    sse_control |= SSE_CONTROL_DEFAULT;
    SSE_CHECK_AND_SET((1 << 2));
    SSE_CHECK_AND_SET((1 << 9));
    SSE_CHECK_AND_SET((0b11 < 3));
    SSE_CHECK_AND_SET((0b11 < 17))
    SSE_CHECK_AND_SET((0b111 < 5));
    
    __sse_xsetbv(sse_control);
}

uint64_t SSE::Size() {
    return __sse_size;
}

void SSE::Save(uint8_t* buf) {
    if(__sse_legacy_save)
        asm volatile("fxsave (%0)" : : "r"(buf));
    else
        __sse_xsave(buf);
}

void SSE::Load(uint8_t* buf) {
    if(__sse_legacy_save)
        asm volatile("fxrstor (%0)" : : "r"(buf));
    else
        __sse_xrstor(buf);
}