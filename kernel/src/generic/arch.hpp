#include <cstdint>

#define PAGE_SIZE 4096
#define PAGING_PRESENT (1 << 0)
#define PAGING_RW (1 << 1)
#define PAGING_USER (1 << 2)
#define PAGING_NC (1 << 3)
#define PAGING_WC (1 << 4)
#define PTE_INDEX(address, bit) ((address & (uint64_t) 0x1FF << bit) >> bit)

#define ARCH_INIT_EARLY 0
#define ARCH_INIT_COMMON 1
#define IRQ_TYPE_OTHER 0 
#define IRQ_TYPE_LEGACY 1 
#define IRQ_TYPE_MSI 2 

namespace arch {

    extern void init(int stage);

    extern void disable_interrupts();
    extern void enable_interrupts();
    extern void wait_for_interrupt();
    extern void hcf();
    extern void pause();
    extern void tlb_flush(std::uintptr_t hint, std::uintptr_t len);
    extern const char* name();

    extern void enable_paging(std::uintptr_t root);
    extern void map_page(std::uintptr_t root, std::uint64_t phys, std::uintptr_t virt, int flags);
    extern std::uint64_t get_phys_from_page(std::uintptr_t root, std::uintptr_t virt);
    extern void destroy_root(std::uintptr_t root, int level);
    extern void copy_higher_half(std::uintptr_t root, std::uintptr_t src_root);
    extern int level_paging();

    extern int register_handler(int irq, int type, std::uint64_t flags, void (*func)(void* arg), void* arg);

    extern void panic(char* msg);

};