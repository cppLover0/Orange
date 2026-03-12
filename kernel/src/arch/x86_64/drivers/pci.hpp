#pragma once
#include <arch/x86_64/drivers/io.hpp>
#include <cstdint>

typedef struct {
	std::uint16_t vendorID;
	std::uint16_t deviceID;
	std::uint16_t command;
	std::uint16_t status;
	std::uint8_t revisionID;
	std::uint8_t progIF;
	std::uint8_t subclass;
	std::uint8_t _class;
	std::uint8_t cacheLineSize;
	std::uint8_t latencyTimer;
	std::uint8_t headerType;
	std::uint8_t bist;
	std::uint32_t bar0;
	std::uint32_t bar1;
	std::uint32_t bar2;
	std::uint32_t bar3;
	std::uint32_t bar4;
	std::uint32_t bar5;
	std::uint32_t cardbusCISPointer;
	std::uint16_t subsystemVendorID;
	std::uint16_t subsystemID;
	std::uint32_t expansionROMBaseAddress;
	std::uint8_t capabilitiesPointer;
	std::uint8_t reserved0;
	std::uint16_t reserved1;
	std::uint32_t reserved2;
	std::uint8_t irq;
	std::uint8_t interruptPIN;
	std::uint8_t minGrant;
	std::uint8_t maxLatency;
} __attribute__((packed)) pci_t;

typedef struct pci_cap {
	std::uint8_t id;
	std::uint8_t off;
	std::uint8_t bus;
	std::uint8_t num;
	std::uint8_t func;
	std::uint16_t venID;
	std::uint16_t devID;
	std::uint8_t data[32];
	struct pci_cap* next;
} __attribute__((packed)) pci_cap_t;

typedef struct {
	int used;
	std::uint8_t _class;
	std::uint8_t subclass;
	void (*pcidrv)(pci_t, std::uint8_t, std::uint8_t, std::uint8_t);
} __attribute__((packed)) pci_driver_t;

namespace x86_64 {
    namespace pci {

        void reg(void (*pcidrv)(pci_t, std::uint8_t, std::uint8_t, std::uint8_t), std::uint8_t _class, std::uint8_t subclass);
        void initworkspace();

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