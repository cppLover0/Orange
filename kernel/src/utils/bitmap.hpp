#pragma once
#include <klibc/stdio.hpp>
#include <klibc/stdlib.hpp>
#include <klibc/string.hpp>
#include <utils/align.hpp>

namespace utils {
    class bitmap {
      private:
        std::uint8_t* pool;
        std::uint32_t pool_size;

      public:
        bitmap(std::uint32_t size, std::uint8_t* custom_pool = 0) {
            if (custom_pool) {
                this->pool = custom_pool;
                this->pool_size = (ALIGNUP(size, 8) / 8);
            } else {
                this->pool_size = (ALIGNUP(size, 8) / 8) + 1;
                this->pool = (std::uint8_t*) klibc::malloc(pool_size);
            }
            klibc::memset(pool, 0, pool_size);
        }

        std::uint8_t test(std::uint32_t idx) {
            return pool[ALIGNDOWN(idx, 8) / 8] & (1 << (idx - ALIGNDOWN(idx, 8)));
        }

        void clear(std::uint32_t idx) {
            pool[ALIGNDOWN(idx, 8) / 8] &= ~(1 << (idx - ALIGNDOWN(idx, 8)));
        }

        void set(std::uint32_t idx) {
            pool[ALIGNDOWN(idx, 8) / 8] |= (1 << (idx - ALIGNDOWN(idx, 8)));
        }
    };
}; 