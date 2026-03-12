#include <cstdint>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/assembly.hpp>
#include <generic/time.hpp>

std::uint32_t x86_64::lapic::id() {
    if(is_x2apic) {
        return read(0x20);
    } else {
        return read(0x20) >> 24;
    }
}

void x86_64::lapic::eoi() {
    write(0xB0,0);
}

void x86_64::lapic::tick(std::uint64_t tick) { 
    write(0x380,tick);
}

void x86_64::lapic::off() {
    uint32_t icr_low = (0x3 << 18) | (0x5 << 8) | (1 << 14);
    write(0x300, icr_low); 
}

std::uint64_t x86_64::lapic::init(std::uint32_t us) {
    std::uint32_t a,b,c,d;
    assembly::cpuid(1,0,&a,&b,&c,&d);

    static bool is_print = 0;

    if(c & (1 << 21)) {
                            // x2apic present
        if(!is_print) klibc::printf("X2APIC: Enabling x2apic (base is 0x%p)\r\n",0x800);
        assembly::wrmsr(0x1B,assembly::rdmsr(0x1B) | (1 << 10) | (1 << 11));
        is_x2apic = 1;
    } else {
        assembly::wrmsr(0x1B,assembly::rdmsr(0x1B));
    }

                        
    paging::map_range(gobject::kernel_root,assembly::rdmsr(0x1B) & 0xFFFFF000,(assembly::rdmsr(0x1B) & 0xFFFFF000) + etc::hhdm(),PAGE_SIZE, PAGING_PRESENT | PAGING_RW);
    write(0xf0,0xff | 0x100);
    write(0x3e0,1);
    write(0x320,32 | (1 << 16));
    write(0x380,0xFFFFFFFF);
    time::timer->sleep(us);
    std::uint64_t ticks = 0xFFFFFFFF - read(0x390);
    write(0x320, 32 | (1 << 17));
    write(0x3e0,1);
    write(0x380,ticks);

    if(!is_lapic_init) {
        if(!is_print) klibc::printf("LAPIC: Calibration time is %lli, ticks %lli\r\n",us,ticks);
        is_print = 1;
        is_lapic_init = 1;
    }

    return ticks;
}