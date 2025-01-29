
#include <stdint.h>

class HHDM {
public:
    
    static void applyHHDM(uint64_t offset);

    static uint64_t toVirt(uint64_t phys);

    static uint64_t toPhys(uint64_t virt);

    static uint64_t* toVirtPointer(uint64_t phys);

};