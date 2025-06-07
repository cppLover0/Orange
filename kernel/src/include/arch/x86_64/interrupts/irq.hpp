
#include <stdint.h>

#define IRQ_TYPE_OTHER 0 
#define IRQ_TYPE_LEGACY 1 // default irq, will be automatic configured with ioapic
#define IRQ_TYPE_MSI 2 // can me msi-x too, need to be configured from pci driver

typedef struct {
    void (*func)(void* arg);
    void* arg;
} irq_t;

class IRQ {
public:
    static uint8_t Create(uint16_t irq,uint8_t type,void (*func)(void* arg),void* arg,uint64_t flags);
};