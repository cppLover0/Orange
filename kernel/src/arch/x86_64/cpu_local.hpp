
#include <cstdint>

#pragma once

#include <arch/x86_64/assembly.hpp>
#include <generic/scheduling.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <cstddef>
#include <type_traits>
#include <generic/arch.hpp>

struct cpudata_t {
    std::uint64_t user_stack;
    std::uint64_t kernel_stack;
    std::uint64_t timer_ist_stack;
    std::uint32_t cpu;
    std::uint64_t tsc_freq;
    void* pvclock_buffer;
    thread* current_thread;
};

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

    template <typename T, std::size_t Offset> [[nodiscard]] static inline T cpu_local_read() {
        T out;
        asm volatile("mov %%gs:%c1, %0" : "=r"(out) : "i"(Offset));
        return out;
    }

    template <typename T, std::size_t Offset, typename V> static inline void cpu_local_write(V&& value) {
        static_assert(std::is_same_v<std::remove_cvref_t<V>, std::remove_cv_t<T>>, "member type and value type are not compatible");
        T v = static_cast<T>(value);
        asm volatile("mov %0, %%gs:%c1" : : "r"(v), "i"(Offset));
    }

};

#define CPU_LOCAL_READ(field) x86_64::cpu_local_read<decltype(((cpudata_t*) nullptr)->field), offsetof(cpudata_t, field)>()
#define CPU_LOCAL_WRITE(field, value) x86_64::cpu_local_write<decltype(((cpudata_t*) nullptr)->field), offsetof(cpudata_t, field)>((value))