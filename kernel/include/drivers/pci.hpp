
#include <cstdint>
#include <drivers/io.hpp>


#pragma once

/* Copypasted from old kernel :) */

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

#define PCI_BAR_MASK ((1 << 0) | (1 << 1))

namespace drivers {
	class pci {
	public:
		static void initworkspace();
		static void reg(void (*pcidrv)(pci_t, std::uint8_t, std::uint8_t, std::uint8_t), std::uint8_t _class, std::uint8_t subclass);
		static std::uint32_t in(std::int8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint8_t bytewidth);
		static void out(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value, std::uint8_t bytewidth);
	};
};