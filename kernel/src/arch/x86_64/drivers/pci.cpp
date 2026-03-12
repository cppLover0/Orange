#include <arch/x86_64/drivers/pci.hpp>
#include <cstdint>

pci_driver_t pci_drivers[256];

pci_t __pci_load(std::uint8_t bus, std::uint8_t num, std::uint8_t function) {
	pci_t pciData;
	std::uint16_t *p = (std::uint16_t *)&pciData;
	for (std::uint8_t i = 0; i < 32; i++) {
		p[i] = x86_64::pci::pci_read_config16(bus, num, function, i * 2);
	}
	return pciData;
}

void x86_64::pci::reg(void (*pcidrv)(pci_t, std::uint8_t, std::uint8_t, std::uint8_t), std::uint8_t _class, std::uint8_t subclass) {
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

void x86_64::pci::initworkspace() {
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