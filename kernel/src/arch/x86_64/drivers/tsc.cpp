#define BILLION 1000000000UL
#include <cstdint>
#include <drivers/acpi.hpp>
#include <arch/x86_64/drivers/tsc.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>

#include <generic/arch.hpp>

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void drivers::tsc::init() {

    if(time::timer == nullptr) {
        klibc::printf("TSC: Can't initialize without timer !\n");
    }

    uint64_t tsc_start, tsc_end;
    std::uint64_t time_start,time_end;
    
    tsc_start = rdtsc();
    time_start = time::timer->current_nano();
    
    time::timer->sleep(100000); 
    
    tsc_end = rdtsc();
    time_end = time::timer->current_nano();
    
    uint64_t time_ns = time_end - time_start;
    uint64_t tsc_diff = tsc_end - tsc_start;

    uint64_t tsc_freq;
    
    if (tsc_diff > UINT64_MAX / BILLION) {
        uint64_t tsc_high = tsc_diff >> 32;
        uint64_t tsc_low = tsc_diff & 0xFFFFFFFF;
        
        uint64_t temp_high = (tsc_high * BILLION) >> 32;
        uint64_t temp_low = (tsc_high * BILLION) << 32;
        if (temp_low < (tsc_high * BILLION)) temp_high++; 
        
        uint64_t temp = temp_low + tsc_low * BILLION;
        if (temp < tsc_low * BILLION) temp_high++; 
        
        tsc_freq = (temp / time_ns + (temp_high / time_ns)) << 32;
    } else {
        tsc_freq = (tsc_diff * BILLION) / time_ns;
    }
    
    x86_64::cpu_data()->tsc_freq = tsc_freq;

    static bool is_print = 0;
    if(!is_print) {klibc::printf("TSC: TSC Frequency is %llu\r\n", tsc_freq); is_print = 1;}
    drivers::tsc_timer* tsc_timer = new drivers::tsc_timer;
    time::setup_timer(tsc_timer);
}

namespace drivers {
    void tsc_timer::sleep(std::uint64_t us) {
        std::uint64_t current = this->current_nano();
        std::uint64_t end_ns = us * 1000;
        std::uint64_t target = current + end_ns;
        
        while(this->current_nano() < target) {
            arch::pause();
        }
    }

    std::uint64_t tsc_timer::current_nano() {
        std::uint64_t freq = x86_64::cpu_data()->tsc_freq;

        if(freq == 0) { // redirect to previous timer
            if(time::previous_timer) {
                return time::previous_timer->current_nano();
            } else {
                klibc::printf("TSC: Trying to get current nano timestamp without any timer from non calibrated tsc !\n");
                return 0;
            }
        }

        uint32_t lo, hi;
        asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        std::uint64_t tsc_v = ((uint64_t)hi << 32) | lo;
        
        if (tsc_v > UINT64_MAX / BILLION) {
            uint64_t high = tsc_v >> 32;
            uint64_t low = tsc_v & 0xFFFFFFFF;
            
            uint64_t high_part = (high * BILLION) / freq;
            uint64_t high_rem = (high * BILLION) % freq;
            
            uint64_t low_part = (low * BILLION) / freq;
            uint64_t low_rem = (low * BILLION) % freq;
            
            uint64_t result = (high_part << 32) + low_part;
            
            uint64_t total_rem = (high_rem << 32) + low_rem;
            result += total_rem / freq;
            
            return result;
        } else {
            return (tsc_v * BILLION) / freq;
        }
    }

    int tsc_timer::get_priority() {
        return 9999999; // undertale reference
    }
}