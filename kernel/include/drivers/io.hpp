
#include <cstdint>

#pragma once

namespace drivers {
    class io {
    private:
    public:
        io() {

        }

        inline void outb(std::uint16_t port,std::uint8_t val) {
            __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline void outw(std::uint16_t port,std::uint16_t val) {
            __asm__ volatile ( "outw %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline void outd(std::uint16_t port,std::uint32_t val) {
            __asm__ volatile ( "outd %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline std::uint8_t inb(std::uint16_t port) {
            std::uint8_t ret;
            __asm__ volatile ( "inb %w1, %b0"
                        : "=a"(ret)
                        : "Nd"(port)
                        : "memory");
            return ret;
        }

        inline std::uint16_t inw(std::uint16_t port) {
            std::uint16_t ret;
            __asm__ volatile ( "inw %w1, %b0"
                        : "=a"(ret)
                        : "Nd"(port)
                        : "memory");
            return ret;
        }

        inline std::uint32_t ind(std::uint16_t port) {
            std::uint32_t ret;
            __asm__ volatile ( "ind %w1, %b0"
                        : "=a"(ret)
                        : "Nd"(port)
                        : "memory");
            return ret;
        }

        inline void wait() {
            outb(0x80,0);
        }

    };
};