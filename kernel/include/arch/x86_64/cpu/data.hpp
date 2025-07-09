
#include <cstdint>

#pragma once

#include <etc/assembly.hpp>
#include <etc/libc.hpp>

typedef struct {
    std::uint64_t kernel_stack;
    std::uint64_t user_stack;
    struct smp {
        std::uint8_t cpu_id;
    };
} cpudata_t;

namespace arch {
    namespace x86_64 {
        namespace cpu {
            inline cpudata_t* data() {
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