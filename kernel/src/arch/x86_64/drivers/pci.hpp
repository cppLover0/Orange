#include <arch/x86_64/drivers/io.hpp>
#include <cstdint>

namespace x86_64 {
    namespace pci {
        inline std::uint32_t pci_read_config32(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
            std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
            x86_64::io::outd(0xCF8, address);
            return x86_64::io::ind(0xCFC);
        }

        inline std::uint16_t pci_read_config16(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
            std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
            x86_64::io::outd(0xCF8, address);
            return (std::uint16_t)((x86_64::io::ind(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
        }

        inline std::uint8_t pci_read_config8(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
            std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
            x86_64::io::outd(0xCF8, address);
            return (std::uint8_t)((x86_64::io::ind(0xCFC) >> ((offset & 3)  * 8)) & 0xff);
        }

        inline void pci_write_config32(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value) {
            std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
            x86_64::io::outd(0xCF8, address);
            x86_64::io::outd(0xCFC, value);
        }

        inline void pci_write_config16(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value) {
            uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
            x86_64::io::outd(0xCF8, address);
            x86_64::io::outd(0xCFC,(x86_64::io::ind(0xCFC) & ~(0xffff << ((offset & 2) * 8))) | (value << ((offset & 2) * 8)));
        }

        inline void pci_write_config8(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint8_t value) {
            std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
            x86_64::io::outd(0xCF8, address);
            x86_64::io::outd(0xCFC, ((x86_64::io::ind(0xCFC) & ~(0xFF << (offset & 3))) | (value << ((offset & 3) * 8))));
        }
    };
};