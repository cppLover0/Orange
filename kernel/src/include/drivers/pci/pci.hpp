
#include <stdint.h>

#pragma once

typedef struct {
	uint16_t vendorID;
	uint16_t deviceID;
	uint16_t command;
	uint16_t status;
	uint8_t revisionID;
	uint8_t progIF;
	uint8_t subclass;
	uint8_t _class;
	uint8_t cacheLineSize;
	uint8_t latencyTimer;
	uint8_t headerType;
	uint8_t bist;
	uint32_t bar0;
	uint32_t bar1;
	uint32_t bar2;
	uint32_t bar3;
	uint32_t bar4;
	uint32_t bar5;
	uint32_t cardbusCISPointer;
	uint16_t subsystemVendorID;
	uint16_t subsystemID;
	uint32_t expansionROMBaseAddress;
	uint8_t capabilitiesPointer;
	uint8_t reserved0;
	uint16_t reserved1;
	uint32_t reserved2;
	uint8_t irq;
	uint8_t interruptPIN;
	uint8_t minGrant;
	uint8_t maxLatency;
} __attribute__((packed)) pci_t;

typedef struct pci_cap {
	uint8_t id;
	uint8_t off;
	uint8_t bus;
	uint8_t num;
	uint8_t func;
	uint16_t venID;
	uint16_t devID;
	uint8_t data[32];
	struct pci_cap* next;
} __attribute__((packed)) pci_cap_t;

typedef struct {
	int used;
	uint8_t _class;
	uint8_t subclass;
	void (*pcidrv)(pci_t, uint8_t, uint8_t, uint8_t);
} __attribute__((packed)) pci_driver_t;

#define PCI_BAR_MASK ((1 << 0) | (1 << 1))

class PCI {
public:
    static uint32_t IN(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint8_t bytewidth);

    static void OUT(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint32_t value,uint8_t bytewidth);

    static uint8_t Reg(void (*pcidrv)(pci_t, uint8_t, uint8_t, uint8_t), uint8_t _class, uint8_t subclass);

    static void Init();
};