
#include <stdint.h>
#include <drivers/io/io.hpp>
#include <drivers/pci/pci.hpp>
#include <generic/locks/spinlock.hpp>

char pci_spinlock = 0;

pci_driver_t pci_drivers[256];

uint32_t pci_read_config32(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	IO::OUT(0xCF8, address,4);
	return IO::IN(0xCFC,4);
}

uint16_t pci_read_config16(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	IO::OUT(0xCF8, address,4);
	return (uint16_t)((IO::IN(0xCFC,4) >> ((offset & 2) * 8)) & 0xffff);
}

uint8_t pci_read_config8(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	IO::OUT(0xCF8, address,4);
	return (uint8_t)((IO::IN(0xCFC,4) >> ((offset & 3)  * 8)) & 0xff);
}

void pci_write_config32(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint32_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	IO::OUT(0xCF8, address,4);
	IO::OUT(0xCFC, value,4);
}

void pci_write_config16(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint32_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	IO::OUT(0xCF8, address,4);
	IO::OUT(0xCFC,(IO::IN(0xCFC,4) & ~(0xffff << ((offset & 2) * 8))) | (value << ((offset & 2) * 8)),4);
}

void pci_write_config8(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset, uint8_t value) {
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	IO::OUT(0xCF8, address,4);
	IO::OUT(0xCFC, ((IO::IN(0xCFC,4) & ~(0xFF << (offset & 3))) | (value << ((offset & 3) * 8))),4);
}

uint32_t PCI::IN(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint8_t bytewidth) {
    uint32_t value = 0;
    spinlock_lock(&pci_spinlock);
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

    spinlock_unlock(&pci_spinlock);

    return value;

}

void PCI::OUT(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint32_t value,uint8_t bytewidth) {
    spinlock_lock(&pci_spinlock);
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
    spinlock_unlock(&pci_spinlock);
}

pci_t __pci_load(uint8_t bus, uint8_t num, uint8_t function) {
	pci_t pciData;
	uint16_t *p = (uint16_t *)&pciData;
	for (uint8_t i = 0; i < 32; i++) {
		p[i] = pci_read_config16(bus, num, function, i * 2);
	}
	return pciData;
}

uint8_t PCI::Reg(void (*pcidrv)(pci_t, uint8_t, uint8_t, uint8_t), uint8_t _class, uint8_t subclass) {
	for (uint16_t i = 0; i < 256; i++) {
		if (!pci_drivers[i].used) {
			pci_drivers[i].used = true;
			pci_drivers[i]._class = _class;
			pci_drivers[i].subclass = subclass;
			pci_drivers[i].pcidrv = pcidrv;
			return true;
		}
	}
	return false;
}

void __pci_launch(pci_t pci, uint8_t bus, uint8_t device, uint8_t function) {
	for (uint16_t i = 0; i < 256; i++) {
		if (pci_drivers[i].used && pci_drivers[i]._class == pci._class && pci_drivers[i].subclass == pci.subclass) {
			pci_drivers[i].pcidrv(pci, bus, device, function);
            return;
		}
	}
}

void PCI::Init() {
	pci_t c_pci;
	for (uint16_t bus = 0; bus < 256; bus++) {
		c_pci = __pci_load(bus, 0, 0);
		if (c_pci.vendorID != 0xFFFF) {
			for (uint8_t device = 0; device < 32; device++) {
				c_pci = __pci_load(bus, device, 0);
				if (c_pci.vendorID != 0xFFFF) {
					__pci_launch(c_pci, bus, device, 0);
					for (uint8_t function = 1; function < 8; function++) {
						pci_t pci = __pci_load(bus, device, function);
						if (pci.vendorID != 0xFFFF) {
							__pci_launch(pci, bus, device, function);
						}
					}
				}
			}
		}
	}
}