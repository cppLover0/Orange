
#include <cstdint>

#pragma once

#include <arch/x86_64/assembly.hpp>
#include <generic/scheduling.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <generic/arch.hpp>

typedef struct {
    std::uint64_t user_stack;
    std::uint64_t kernel_stack;
    std::uint64_t timer_ist_stack;
    std::uint32_t cpu;
    std::uint64_t tsc_freq;
    thread* current_thread;
} cpudata_t;

namespace x86_64 {

    inline std::uint64_t cpu_data_const = 0;

    inline static void init_cpu_data() {
        std::uint64_t cpudata = assembly::rdmsr(0xC0000101);
        if(!cpudata) {
            cpudata = (std::uint64_t)new cpudata_t;
            cpu_data_const = cpudata;
            klibc::memset((void*)cpudata,0,sizeof(cpudata_t));
            assembly::wrmsr(0xC0000101,cpudata);
        }
    }

    inline static void restore_cpu_data() {
        assembly::wrmsr(0xC0000101,cpu_data_const);
    }

    inline static cpudata_t* cpu_data() {
        std::uint64_t cpudata = assembly::rdmsr(0xC0000101);
        if(!cpudata) {
            klibc::printf("cpudata: fdgkldfglfkdfdjnbdff\r\n");
            arch::hcf();
        }
        return (cpudata_t*)cpudata;
    }
};
