#include <stdint.h>
#include <other/assembly.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <other/hhdm.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/log.hpp>
#include <generic/memory/paging.hpp>

char __lapic_init = 0;

void Lapic::Init() {
    __cli(); // if interrupts is enabled disable it
    __wrmsr(0x1B,__rdmsr(0x1B));
    Paging::KernelMap(__rdmsr(0x1b) & 0xfffff000);
    Write(0xF0,0xFF | 0x100);
    Write(0x3E0,0x3);
    Write(0x320, 32 | (1 << 16));
    Write(0x380,0xFFFFFFFF);
    HPET::Sleep(10000);
    uint64_t calibration_ticks = 0xFFFFFFFF - Read(0x390);
    Write(0x320,32 | (1 << 17));
    Write(0x3E0,0x3);
    Write(0x380,calibration_ticks);
    __lapic_init = 1;
}

uint32_t Lapic::Read(uint32_t reg) {
    return *(uint32_t*)(Base() + reg);
}

void Lapic::Write(uint32_t reg,uint32_t val) {
    *(uint32_t*)(Base() + reg) = val;
}

void Lapic::EOI() {
    Write(0xB0,0);
}
    
uint32_t Lapic::ID() {
    if(!__lapic_init)
        return 0;
    return Read(0x20) >> 24;
}

uint64_t Lapic::Base() {
    return (uint64_t)HHDM::toVirt(__rdmsr(0x1b) & 0xfffff000);
}