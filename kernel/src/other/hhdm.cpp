
#include <stdint.h>
#include <other/hhdm.hpp>

uint64_t hhdm_offset;

void HHDM::applyHHDM(uint64_t offset) {
    hhdm_offset = offset;
}

uint64_t HHDM::toVirt(uint64_t phys) {
    return phys + hhdm_offset;
}

uint64_t HHDM::toPhys(uint64_t virt) {
    return virt - hhdm_offset;
}

uint64_t* HHDM::toVirtPointer(uint64_t phys) {
    return (uint64_t*)(phys + hhdm_offset);
}
