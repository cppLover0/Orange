
#include <uacpi/types.h>
#include <uacpi/kernel_api.h>
#include <uacpi/platform/arch_helpers.h>
#if defined(__x86_64__)
#include <arch/x86_64/drivers/io.hpp>
#include <arch/x86_64/drivers/pci.hpp>
#include <arch/x86_64/irq.hpp>
#endif
#include <generic/bootloader/bootloader.hpp>
#include <generic/hhdm.hpp>
#include <generic/paging.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>
#include <generic/heap.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/time.hpp>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {

    if(bootloader::bootloader->get_rsdp() == nullptr)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = ((std::uint64_t)bootloader::bootloader->get_rsdp() - etc::hhdm());
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    paging::map_range(gobject::kernel_root,addr,addr + etc::hhdm(),ALIGNUP(len, PAGE_SIZE),PAGING_PRESENT | PAGING_RW);
    return (void*)(addr + etc::hhdm());
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)addr;
    (void)len;
}

void uacpi_kernel_log(uacpi_log_level, const uacpi_char* msg) {
    klibc::printf("uACPI: %s\r", msg);
}

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)kheap::opt_malloc(sizeof(uacpi_pci_address));
    *pci = address;
    *out_handle = (uacpi_handle)pci;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle hnd) {
    kheap::opt_free((void*)hnd);
}

typedef struct {
    uint64_t base;
    uint64_t len;
} __attribute__((packed)) uacpi_io_struct_t;

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    uacpi_io_struct_t* iomap = (uacpi_io_struct_t*)kheap::opt_malloc(sizeof(uacpi_io_struct_t));
    iomap->base = base;
    iomap->len = len;
    *out_handle = (uacpi_handle*)iomap;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    kheap::opt_free((void*)handle);
}

#if defined(__x86_64__)

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = x86_64::pci::pci_read_config8(pci->bus,pci->device,pci->function,offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = x86_64::pci::pci_read_config16(pci->bus,pci->device,pci->function,offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = x86_64::pci::pci_read_config32(pci->bus,pci->device,pci->function,offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    x86_64::pci::pci_write_config8(pci->bus,pci->device,pci->function,offset,value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    x86_64::pci::pci_write_config16(pci->bus,pci->device,pci->function,offset,value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    x86_64::pci::pci_write_config32(pci->bus,pci->device,pci->function,offset,value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 *out_value
) {
    *out_value = x86_64::io::inb(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 *out_value
) {
    *out_value = x86_64::io::inw(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 *out_value
) {
    *out_value = x86_64::io::ind(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 in_value
) {
    x86_64::io::outb(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 in_value
) {
    x86_64::io::outw(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 in_value
) {
    x86_64::io::outd(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}

#else 

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    (void)device;
    (void)offset;
    (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 *out_value
) {
    (void)hnd;
    (void)offset;
    (void)out_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_io_read16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 *out_value
) {
    (void)hnd;
    (void)offset;
    (void)out_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_io_read32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 *out_value
) {
    (void)hnd;
    (void)offset;
    (void)out_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 in_value
) {
    (void)hnd;
    (void)offset;
    (void)in_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_io_write16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 in_value
) {
    (void)hnd;
    (void)offset;
    (void)in_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_io_write32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 in_value
) {
    (void)hnd;
    (void)offset;
    (void)in_value;
    return UACPI_STATUS_UNIMPLEMENTED;
}


#endif

void *uacpi_kernel_alloc(uacpi_size size) {
    return kheap::opt_malloc(size);
}

void uacpi_kernel_free(void *mem) {
    kheap::opt_free(mem);
}

/*
 * Returns the number of nanosecond ticks elapsed since boot,
 * strictly monotonic.
 */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    if(!time::timer) {
        klibc::printf("uACPI: generic timer is missing !\r\n");
        return 0;
    }

    return time::timer->current_nano();
}

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec) {
    if(!time::timer) {
        klibc::printf("uACPI: generic timer is missing !\r\n");
        return;
    }

    time::timer->sleep(usec);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    if(!time::timer) {
        klibc::printf("uACPI: generic timer is missing !\r\n");
        return;
    }

    time::timer->sleep(msec * 1000);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
    locks::spinlock* lock = (locks::spinlock*)kheap::opt_malloc(sizeof(locks::spinlock));
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_mutex(uacpi_handle hnd) {
    kheap::opt_free((void*)hnd);
}

uacpi_handle uacpi_kernel_create_event(void) {
    return (uacpi_handle)1;
}

void uacpi_kernel_free_event(uacpi_handle) {
    asm volatile("nop");
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return 0;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle lock, uacpi_u16) {
    (void)lock;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle lock) {
    (void)lock;
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
    return 1;
}

void uacpi_kernel_signal_event(uacpi_handle) {
    asm volatile("nop");
}

void uacpi_kernel_reset_event(uacpi_handle) {
    asm volatile("nop");
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
    return UACPI_STATUS_OK;
}

#if defined(__x86_64__)

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler base, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    (void)ctx;
    std::uint8_t vec = x86_64::irq::create(irq, IRQ_TYPE_LEGACY, (void (*)(void*))((void*)base), 0, 0);
    *out_irq_handle = (uacpi_handle)((std::uint64_t)vec);

    return UACPI_STATUS_OK;
}

#else

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler base, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    (void)irq;
    (void)base;
    (void)ctx;
    (void)out_irq_handle;
    return UACPI_STATUS_OK;
}

#endif

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    return 1;
}

/*
 * Restore the state of the interrupt flags to the kernel-defined value provided
 * in 'state'.
 */
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    (void)state;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    (void)irq_handle;
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    locks::spinlock* lock = (locks::spinlock*)kheap::opt_malloc(sizeof(locks::spinlock));
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle hnd) {
    kheap::opt_free((void*)hnd);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle hnd) {
    (void)hnd;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle hnd, uacpi_cpu_flags) {
    (void)hnd;
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
    (void)ctx;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}
