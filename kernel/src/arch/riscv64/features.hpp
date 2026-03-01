#pragma once

#include <klibc/stdio.hpp>
#include <cstdint>

namespace riscv64 {

#define SATP64_MODE 0xF000000000000000
#define PAGING_3LEVEL 8
#define PAGING_4LEVEL 9
#define PAGING_5LEVEL 10

    inline int level_paging = 0;
    inline int raw_level_paging = 0;

    static inline int get_paging_level() {
        if (level_paging == 0) {
            std::uint64_t satp_value = 0;
            asm volatile("csrr %0, satp" : "=r"(satp_value));
            std::uint64_t current_mode = (satp_value & SATP64_MODE) >> 60;

            raw_level_paging = current_mode;

            switch (current_mode) {
                case PAGING_3LEVEL: riscv64::level_paging = 3; break;
                case PAGING_4LEVEL: riscv64::level_paging = 4; break;
                case PAGING_5LEVEL: riscv64::level_paging = 5; break;
                default:            klibc::printf("Unsupported level paging for riscv64 ! (%d)\n", current_mode); return 0;
            };
        }
        return riscv64::level_paging;
    }
}; // namespace riscv64
