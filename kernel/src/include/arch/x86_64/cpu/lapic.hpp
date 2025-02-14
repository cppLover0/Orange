
#include <stdint.h>

class Lapic {
public:
    static void Init();

    static uint32_t Read(uint32_t reg);

    static void Write(uint32_t reg,uint32_t val);

    static void EOI();
    
    static uint32_t ID();

    static uint64_t Base();

};