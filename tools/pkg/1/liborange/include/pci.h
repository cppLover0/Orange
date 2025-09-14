#include <stdint.h>
#include <orange/io.h>

#include <stdio.h>

#ifndef LIBORANGE_PCI_H
#define LIBORANGE_PCI_H

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

uint32_t pci_read_config32(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	outd(0xCF8, address);
	return ind(0xCFC);
}

uint16_t pci_read_config16(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	outd(0xCF8, address);
	return(uint16_t)((ind(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
}

uint8_t pci_read_config8(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	outd(0xCF8, address);
	return (uint8_t)((ind(0xCFC) >> ((offset & 3)  * 8)) & 0xff);
}

void pci_write_config32(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint32_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	outd(0xCF8, address);
	outd(0xCFC, value);
}

void pci_write_config16(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint32_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	outd(0xCF8, address);
	outd(0xCFC,(ind(0xCFC) & ~(0xffff << ((offset & 2) * 8))) | (value << ((offset & 2) * 8)));
}

void pci_write_config8(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint8_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	outd(0xCF8, address);
	outd(0xCFC, ((ind(0xCFC) & ~(0xFF << (offset & 3))) | (value << ((offset & 3) * 8))));
}

uint32_t pci_in(int8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint8_t bytewidth) {
    uint32_t value = 0;
    switch(bytewidth) {
        case 1:    
            value = pci_read_config8(bus,num,function,offset);
            break;
        case 2:
            value = pci_read_config16(bus,num,function,offset);
            break;
        case 4:
            value = pci_read_config32(bus,num,function,offset);
            break;
        default:
            break;
    }

    return value;

}

void pci_out(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint32_t value, uint8_t bytewidth) {
    switch(bytewidth) {
        case 1:
            pci_write_config8(bus,num,function,offset,(uint8_t)value);
            break;
        case 2:
            pci_write_config16(bus,num,function,offset,(uint16_t)value);
            break;
        case 4:
            pci_write_config32(bus,num,function,offset,(uint32_t)value);
            break;
        default:
            break;
    }
}

pci_t __pci_load(uint8_t bus, uint8_t num, uint8_t function) {
	pci_t pciData;
	uint16_t *p = (uint16_t *)&pciData;
	for (uint8_t i = 0; i < 32; i++) {
		p[i] = pci_read_config16(bus, num, function, i * 2);
	}
	return pciData;
}

void pci_reg(pci_driver_t* pci_drivers,void (*pcidrv)(pci_t, uint8_t, uint8_t, uint8_t), uint8_t _class, uint8_t subclass) {
	for (uint16_t i = 0; i < 256; i++) {
		if (!pci_drivers[i].used) {
			pci_drivers[i].used = true;
			pci_drivers[i]._class = _class;
			pci_drivers[i].subclass = subclass;
			pci_drivers[i].pcidrv = pcidrv;
		}
	}
}

void __pci_launch(pci_driver_t* pci_drivers,pci_t pci, uint8_t bus, uint8_t device, uint8_t function) {
	for (uint16_t i = 0; i < 256; i++) {
		if (pci_drivers[i].used && pci_drivers[i]._class == pci._class && pci_drivers[i].subclass == pci.subclass) {
			pci_drivers[i].pcidrv(pci, bus, device, function);
            return;
		}
	}
}

void pci_initworkspace(pci_driver_t* pci_drivers) {
	pci_t c_pci;
	for (uint16_t bus = 0; bus < 256; bus++) {
		c_pci = __pci_load(bus, 0, 0);
		if (c_pci.vendorID != 0xFFFF) {
			for (uint8_t device = 0; device < 32; device++) {
				c_pci = __pci_load(bus, device, 0);
				if (c_pci.vendorID != 0xFFFF) {
					__pci_launch(pci_drivers,c_pci, bus, device, 0);
					for (uint8_t function = 1; function < 8; function++) {
						pci_t pci = __pci_load(bus, device, function);
						if (pci.vendorID != 0xFFFF) {
							__pci_launch(pci_drivers,pci, bus, device, function);
						}
					}
				}
			}
		}
	}
}

#endif