
#include <cstdint>

#pragma once

namespace drivers {
    class io {
    private:
    public:
        io() {

        }

        inline void outb(std::uint16_t port, std::uint8_t val) {
            __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline void outw(std::uint16_t port, std::uint16_t val) {
            __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline void outd(std::uint16_t port, std::uint32_t val) {
            __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
        }

        inline std::uint8_t inb(std::uint16_t port) {
            std::uint8_t ret;
            __asm__ volatile ("inb %1, %0"
                            : "=a"(ret)
                            : "Nd"(port)
                            : "memory");
            return ret;
        }

        inline std::uint16_t inw(std::uint16_t port) {
            std::uint16_t ret;
            __asm__ volatile ("inw %1, %0"
                            : "=a"(ret)
                            : "Nd"(port)
                            : "memory");
            return ret;
        }

        inline std::uint32_t ind(std::uint16_t port) {
            std::uint32_t ret;
            __asm__ volatile ("inl %1, %0"
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