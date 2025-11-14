
#include <cstdint>

#pragma once

#include <etc/assembly.hpp>
#include <etc/libc.hpp>

#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/scheduling.hpp>

typedef struct {
    std::uint64_t lapic_block;
    std::uint64_t user_stack;
    std::uint64_t kernel_stack;
    std::uint64_t timer_ist_stack;
    int last_sys;
    struct {
        std::uint16_t cpu_id;
    } smp;
    struct {
        std::uint64_t freq;
    } tsc;
    struct {
        int_frame_t temp_ctx;
        arch::x86_64::process_t* proc;
        arch::x86_64::process_queue_run_list_t* next;
    } temp;
} cpudata_t;

namespace arch {
    namespace x86_64 {
        namespace cpu {
            inline static cpudata_t* data() {
                std::uint64_t cpudata = __rdmsr(0xC0000101);
                if(!cpudata) {
                    cpudata = (std::uint64_t)new cpudata_t;
                    memset((void*)cpudata,0,sizeof(cpudata_t));
                    __wrmsr(0xC0000101,cpudata);
                }
                return (cpudata_t*)cpudata;
            }
        };
    };
};