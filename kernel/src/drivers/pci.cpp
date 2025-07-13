
#include <cstdint>
#include <drivers/io.hpp>

#include <drivers/pci.hpp>

char pci_spinlock = 0;

pci_driver_t pci_drivers[256];

std::uint32_t pci_read_config32(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
    drivers::io io;
	std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	io.outd(0xCF8, address);
	return io.ind(0xCFC);
}

std::uint16_t pci_read_config16(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
    drivers::io io;
	std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	io.outd(0xCF8, address);
	return (std::uint16_t)((io.ind(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
}

std::uint8_t pci_read_config8(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset) {
    drivers::io io;
	std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	io.outd(0xCF8, address);
	return (std::uint8_t)((io.ind(0xCFC) >> ((offset & 3)  * 8)) & 0xff);
}

void pci_write_config32(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value) {
    drivers::io io;
	std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset);
	io.outd(0xCF8, address);
	io.outd(0xCFC, value);
}

void pci_write_config16(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value) {
    drivers::io io;
	uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	io.outd(0xCF8, address);
	io.outd(0xCFC,(io.ind(0xCFC) & ~(0xffff << ((offset & 2) * 8))) | (value << ((offset & 2) * 8)));
}

void pci_write_config8(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint8_t value) {
    drivers::io io;
	std::uint32_t address = (1 << 31) | (bus << 16) | (num << 11) | (function << 8) | (offset & 0xfc);
	io.outd(0xCF8, address);
	io.outd(0xCFC, ((io.ind(0xCFC) & ~(0xFF << (offset & 3))) | (value << ((offset & 3) * 8))));
}

std::uint32_t drivers::pci::in(std::int8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint8_t bytewidth) {
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

void drivers::pci::out(std::uint8_t bus, std::uint8_t num, std::uint8_t function, std::uint8_t offset, std::uint32_t value, std::uint8_t bytewidth) {
    switch(bytewidth) {
        case 1:
            pci_write_config8(bus,num,function,offset,(std::uint8_t)value);
            break;
        case 2:
            pci_write_config16(bus,num,function,offset,(std::uint16_t)value);
            break;
        case 4:
            pci_write_config32(bus,num,function,offset,(std::uint32_t)value);
            break;
        default:
            break;
    }
}

pci_t __pci_load(std::uint8_t bus, std::uint8_t num, std::uint8_t function) {
	pci_t pciData;
	std::uint16_t *p = (std::uint16_t *)&pciData;
	for (std::uint8_t i = 0; i < 32; i++) {
		p[i] = pci_read_config16(bus, num, function, i * 2);
	}
	return pciData;
}

void drivers::pci::reg(void (*pcidrv)(pci_t, std::uint8_t, std::uint8_t, std::uint8_t), std::uint8_t _class, std::uint8_t subclass) {
	for (std::uint16_t i = 0; i < 256; i++) {
		if (!pci_drivers[i].used) {
			pci_drivers[i].used = true;
			pci_drivers[i]._class = _class;
			pci_drivers[i].subclass = subclass;
			pci_drivers[i].pcidrv = pcidrv;
		}
	}
}

void __pci_launch(pci_t pci, std::uint8_t bus, std::uint8_t device, std::uint8_t function) {
	for (std::uint16_t i = 0; i < 256; i++) {
		if (pci_drivers[i].used && pci_drivers[i]._class == pci._class && pci_drivers[i].subclass == pci.subclass) {
			pci_drivers[i].pcidrv(pci, bus, device, function);
            return;
		}
	}
}

void drivers::pci::initworkspace() {
	pci_t c_pci;
	for (std::uint16_t bus = 0; bus < 256; bus++) {
		c_pci = __pci_load(bus, 0, 0);
		if (c_pci.vendorID != 0xFFFF) {
			for (std::uint8_t device = 0; device < 32; device++) {
				c_pci = __pci_load(bus, device, 0);
				if (c_pci.vendorID != 0xFFFF) {
					__pci_launch(c_pci, bus, device, 0);
					for (std::uint8_t function = 1; function < 8; function++) {
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