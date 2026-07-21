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
#define ARCH_INIT_MP 2
#define IRQ_TYPE_OTHER 0 
#define IRQ_TYPE_LEGACY 1 
#define IRQ_TYPE_MSI 2 

namespace arch {

    void init(int stage);

    void disable_interrupts();
    void enable_interrupts();
    void wait_for_interrupt();
    void hcf();
    void pause();
    void tlb_flush(std::uintptr_t hint, std::uintptr_t len);
    const char* name();

    void enable_paging(std::uintptr_t root);
    void map_page(std::uintptr_t root, std::uint64_t phys, std::uintptr_t virt, int flags);
    std::int64_t get_phys_from_page(std::uintptr_t root, std::uintptr_t virt);
    void destroy_root(std::uintptr_t root, int level);
    void copy_higher_half(std::uintptr_t root, std::uintptr_t src_root);
    bool is_dirty_address(std::uintptr_t root, std::uintptr_t virt);
    void clear_dirty_bit(std::uintptr_t root, std::uintptr_t virt);

    int level_paging();

    bool test_interrupts();

    std::uint64_t current_root();

    void memory_barrier();

    int register_handler(int irq, int type, std::uint64_t flags, void (*func)(void* arg), void* arg);

    void panic(char* msg);
    




    [[gnu::weak]] void fill_root(std::uintptr_t root);
};