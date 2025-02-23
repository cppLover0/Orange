
#include <stdint.h>

class IOAPIC {
public:
    static void Init();
    static void Write(uint64_t base,uint32_t reg,uint32_t value);
    static void Read(uint64_t base,uint32_t reg,uint32_t value);
};