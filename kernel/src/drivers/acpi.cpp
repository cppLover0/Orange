
#include <cstdint>
#include <drivers/acpi.hpp>

#include <etc/bootloaderinfo.hpp>

#include <uacpi/types.h>
#include <uacpi/platform/arch_helpers.h>
#include <generic/mm/paging.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>

#include <generic/locks/spinlock.hpp>

#include <arch/x86_64/interrupts/irq.hpp>

#include <drivers/ioapic.hpp>

#include <drivers/pci.hpp>
#include <drivers/io.hpp>

#include <drivers/tsc.hpp>
#include <drivers/hpet.hpp>

#include <arch/x86_64/cpu/lapic.hpp>

#include <etc/logging.hpp>

#include <etc/etc.hpp>

#include <uacpi/utilities.h>
#include <uacpi/event.h>

#include <generic/time.hpp>

extern std::uint64_t __hpet_counter();

void drivers::acpi::init() {
    uacpi_status ret = uacpi_initialize(0);

    drivers::hpet::init();
    Log::Display(LEVEL_MESSAGE_OK,"HPET initializied\n");
    
    drivers::tsc::init();
    Log::Display(LEVEL_MESSAGE_OK,"TSC initializied\n");

    drivers::ioapic::init();
    Log::Display(LEVEL_MESSAGE_OK,"IOAPIC initializied\n");

    arch::x86_64::cpu::lapic::init(1000);
    Log::Display(LEVEL_MESSAGE_OK,"LAPIC initializied\n");

    ret = uacpi_namespace_load();

    ret = uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    ret = uacpi_namespace_initialize();
    ret = uacpi_finalize_gpe_initialization();

}

#ifdef __cplusplus
extern "C" {
#endif

// Returns the PHYSICAL address of the RSDP structure via *out_rsdp_address.
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    *out_rsdp_address = BootloaderInfo::AccessRSDP();
    return UACPI_STATUS_OK;
}

/*
 * Map a physical memory range starting at 'addr' with length 'len', and return
 * a virtual address that can be used to access it.
 *
 * NOTE: 'addr' may be misaligned, in this case the host is expected to round it
 *       down to the nearest page-aligned boundary and map that, while making
 *       sure that at least 'len' bytes are still mapped starting at 'addr'. The
 *       return value preserves the misaligned offset.
 *
 *       Example for uacpi_kernel_map(0x1ABC, 0xF00):
 *           1. Round down the 'addr' we got to the nearest page boundary.
 *              Considering a PAGE_SIZE of 4096 (or 0x1000), 0x1ABC rounded down
 *              is 0x1000, offset within the page is 0x1ABC - 0x1000 => 0xABC
 *           2. Requested 'len' is 0xF00 bytes, but we just rounded the address
 *              down by 0xABC bytes, so add those on top. 0xF00 + 0xABC => 0x19BC
 *           3. Round up the final 'len' to the nearest PAGE_SIZE boundary, in
 *              this case 0x19BC is 0x2000 bytes (2 pages if PAGE_SIZE is 4096)
 *           4. Call the VMM to map the aligned address 0x1000 (from step 1)
 *              with length 0x2000 (from step 3). Let's assume the returned
 *              virtual address for the mapping is 0xF000.
 *           5. Add the original offset within page 0xABC (from step 1) to the
 *              resulting virtual address 0xF000 + 0xABC => 0xFABC. Return it
 *              to uACPI.
 */
void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    extern std::uint64_t kernel_cr3;
    memory::paging::maprange(kernel_cr3,addr,(std::uint64_t)Other::toVirt(addr),len,PTE_PRESENT | PTE_RW);
    return Other::toVirt(addr);
}

/*
 * Unmap a virtual memory range at 'addr' with a length of 'len' bytes.
 *
 * NOTE: 'addr' may be misaligned, see the comment above 'uacpi_kernel_map'.
 *       Similar steps to uacpi_kernel_map can be taken to retrieve the
 *       virtual address originally returned by the VMM for this mapping
 *       as well as its true length.
 */
void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    asm volatile("nop");
}

#ifndef UACPI_FORMATTED_LOGGING
void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* msg) {
    Log::Display(LEVEL_MESSAGE_INFO,"UACPI: %s",msg);
}
#else
UACPI_PRINTF_DECL(2, 3)
void uacpi_kernel_log(uacpi_log_level, const uacpi_char*, ...);
void uacpi_kernel_vlog(uacpi_log_level, const uacpi_char*, uacpi_va_list);
#endif

/*
 * Only the above ^^^ API may be used by early table access and
 * UACPI_BAREBONES_MODE.
 */
#ifndef UACPI_BAREBONES_MODE

/*
 * Convenience initialization/deinitialization hooks that will be called by
 * uACPI automatically when appropriate if compiled-in.
 */
#ifdef UACPI_KERNEL_INITIALIZATION
/*
 * This API is invoked for each initialization level so that appropriate parts
 * of the host kernel and/or glue code can be initialized at different stages.
 *
 * uACPI API that triggers calls to uacpi_kernel_initialize and the respective
 * 'current_init_lvl' passed to the hook at that stage:
 * 1. uacpi_initialize() -> UACPI_INIT_LEVEL_EARLY
 * 2. uacpi_namespace_load() -> UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED
 * 3. (start of) uacpi_namespace_initialize() -> UACPI_INIT_LEVEL_NAMESPACE_LOADED
 * 4. (end of) uacpi_namespace_initialize() -> UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED
 */
uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl);
void uacpi_kernel_deinitialize(void);
#endif

/*
 * Open a PCI device at 'address' for reading & writing.
 *
 * Note that this must be able to open any arbitrary PCI device, not just those
 * detected during kernel PCI enumeration, since the following pattern is
 * relatively common in AML firmware:
 *    Device (THC0)
 *    {
 *        // Device at 00:10.06
 *        Name (_ADR, 0x00100006)  // _ADR: Address
 *
 *        OperationRegion (THCR, PCI_Config, Zero, 0x0100)
 *        Field (THCR, ByteAcc, NoLock, Preserve)
 *        {
 *            // Vendor ID field in the PCI configuration space
 *            VDID,   32
 *        }
 *
 *        // Check if the device at 00:10.06 actually exists, that is reading
 *        // from its configuration space returns something other than 0xFFs.
 *        If ((VDID != 0xFFFFFFFF))
 *        {
 *            // Actually create the rest of the device's body if it's present
 *            // in the system, otherwise skip it.
 *        }
 *    }
 *
 * The handle returned via 'out_handle' is used to perform IO on the
 * configuration space of the device.
 */
uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    uacpi_pci_address* pci = new uacpi_pci_address;
    *pci = address;
    *out_handle = (uacpi_handle)pci;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle hnd) {
    delete (void*)hnd;
}

/*
 * Read & write the configuration space of a previously open PCI device.
 */
uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = drivers::pci::in(pci->bus,pci->device,pci->function,offset,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = drivers::pci::in(pci->bus,pci->device,pci->function,offset,2);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = drivers::pci::in(pci->bus,pci->device,pci->function,offset,4);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    drivers::pci::out(pci->bus,pci->device,pci->function,offset,value,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    drivers::pci::out(pci->bus,pci->device,pci->function,offset,value,2);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    drivers::pci::out(pci->bus,pci->device,pci->function,offset,value,4);
    return UACPI_STATUS_OK;
}

/*
 * Map a SystemIO address at [base, base + len) and return a kernel-implemented
 * handle that can be used for reading and writing the IO range.
 *
 * NOTE: The x86 architecture uses the in/out family of instructions
 *       to access the SystemIO address space.
 */

typedef struct {
    uint64_t base;
    uint64_t len;
} __attribute__((packed)) uacpi_io_struct_t;

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    uacpi_io_struct_t* iomap = new uacpi_io_struct_t;
    iomap->base = base;
    iomap->len = len;
    *out_handle = (uacpi_handle*)iomap;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle handle) {
    delete (void*)handle;
}

/*
 * Read/Write the IO range mapped via uacpi_kernel_io_map
 * at a 0-based 'offset' within the range.
 *
 * NOTE:
 * The x86 architecture uses the in/out family of instructions
 * to access the SystemIO address space.
 *
 * You are NOT allowed to break e.g. a 4-byte access into four 1-byte accesses.
 * Hardware ALWAYS expects accesses to be of the exact width.
 */
uacpi_status uacpi_kernel_io_read8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 *out_value
) {
    drivers::io io;
    *out_value = io.inb(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 *out_value
) {
    drivers::io io;
    *out_value = io.inw(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 *out_value
) {
    drivers::io io;
    *out_value = io.ind(*(std::uint16_t*)hnd + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle hnd, uacpi_size offset, uacpi_u8 in_value
) {
    drivers::io io;
    io.outb(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(
    uacpi_handle hnd, uacpi_size offset, uacpi_u16 in_value
) {
    drivers::io io;
    io.outw(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(
    uacpi_handle hnd, uacpi_size offset, uacpi_u32 in_value
) {
    drivers::io io;
    io.outd(*(std::uint16_t*)hnd + offset,in_value);
    return UACPI_STATUS_OK;
}

/*
 * Allocate a block of memory of 'size' bytes.
 * The contents of the allocated memory are unspecified.
 */
void *uacpi_kernel_alloc(uacpi_size size) {
    return memory::heap::malloc(size);
}

#ifdef UACPI_NATIVE_ALLOC_ZEROED
/*
 * Allocate a block of memory of 'size' bytes.
 * The returned memory block is expected to be zero-filled.
 */
void *uacpi_kernel_alloc_zeroed(uacpi_size size);
#endif

/*
 * Free a previously allocated memory block.
 *
 * 'mem' might be a NULL pointer. In this case, the call is assumed to be a
 * no-op.
 *
 * An optionally enabled 'size_hint' parameter contains the size of the original
 * allocation. Note that in some scenarios this incurs additional cost to
 * calculate the object size.
 */
#ifndef UACPI_SIZED_FREES
void uacpi_kernel_free(void *mem) {
    memory::heap::free(mem);
}
#else
void uacpi_kernel_free(void *mem, uacpi_size size_hint);
#endif

/*
 * Returns the number of nanosecond ticks elapsed since boot,
 * strictly monotonic.
 */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return time::counter();
}

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec) {
    time::sleep(usec);
}

/*
 * Sleep for N milliseconds.
 */
void uacpi_kernel_sleep(uacpi_u64 msec) {
    time::sleep(msec * 1000);
}

/*
 * Create/free an opaque non-recursive kernel mutex object.
 */
uacpi_handle uacpi_kernel_create_mutex(void) {
    locks::spinlock* lock = new locks::spinlock;
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_mutex(uacpi_handle hnd) {
    delete (void*)hnd;
}

/*
 * Create/free an opaque kernel (semaphore-like) event object.
 */
uacpi_handle uacpi_kernel_create_event(void) {
    return (uacpi_handle)1;
}

void uacpi_kernel_free_event(uacpi_handle) {
    asm volatile("nop");
}

/*
 * Returns a unique identifier of the currently executing thread.
 *
 * The returned thread id cannot be UACPI_THREAD_ID_NONE.
 */
uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return 0;
}

/*
 * Try to acquire the mutex with a millisecond timeout.
 *
 * The timeout value has the following meanings:
 * 0x0000 - Attempt to acquire the mutex once, in a non-blocking manner
 * 0x0001...0xFFFE - Attempt to acquire the mutex for at least 'timeout'
 *                   milliseconds
 * 0xFFFF - Infinite wait, block until the mutex is acquired
 *
 * The following are possible return values:
 * 1. UACPI_STATUS_OK - successful acquire operation
 * 2. UACPI_STATUS_TIMEOUT - timeout reached while attempting to acquire (or the
 *                           single attempt to acquire was not successful for
 *                           calls with timeout=0)
 * 3. Any other value - signifies a host internal error and is treated as such
 */
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle lock, uacpi_u16) {
    locks::spinlock* slock = (locks::spinlock*)lock;
    slock->lock();
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle lock) {
    locks::spinlock* slock = (locks::spinlock*)lock;
    slock->unlock();
}

/*
 * Try to wait for an event (counter > 0) with a millisecond timeout.
 * A timeout value of 0xFFFF implies infinite wait.
 *
 * The internal counter is decremented by 1 if wait was successful.
 *
 * A successful wait is indicated by returning UACPI_TRUE.
 */
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
    return 1;
}

/*
 * Signal the event object by incrementing its internal counter by 1.
 *
 * This function may be used in interrupt contexts.
 */
void uacpi_kernel_signal_event(uacpi_handle) {
    asm volatile("nop");
}

/*
 * Reset the event counter to 0.
 */
void uacpi_kernel_reset_event(uacpi_handle) {
    asm volatile("nop");
}

/*
 * Handle a firmware request.
 *
 * Currently either a Breakpoint or Fatal operators.
 */
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
    return UACPI_STATUS_OK;
}

/*
 * Install an interrupt handler at 'irq', 'ctx' is passed to the provided
 * handler for every invocation.
 *
 * 'out_irq_handle' is set to a kernel-implemented value that can be used to
 * refer to this handler from other API.
 */

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler base, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    std::uint8_t vec = arch::x86_64::interrupts::irq::create(irq,IRQ_TYPE_LEGACY,(std::int32_t (*)(void*))base,0,0);
    *out_irq_handle = (uacpi_handle)vec;
    return UACPI_STATUS_OK;
}

/*
 * Uninstall an interrupt handler. 'irq_handle' is the value returned via
 * 'out_irq_handle' during installation.
 */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    asm volatile("nop");
    return UACPI_STATUS_OK;
}

/*
 * Create/free a kernel spinlock object.
 *
 * Unlike other types of locks, spinlocks may be used in interrupt contexts.
 */
uacpi_handle uacpi_kernel_create_spinlock(void) {
    locks::spinlock* lock = new locks::spinlock;
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle hnd) {
    delete (void*)hnd;
}

/*
 * Lock/unlock helpers for spinlocks.
 *
 * These are expected to disable interrupts, returning the previous state of cpu
 * flags, that can be used to possibly re-enable interrupts if they were enabled
 * before.
 *
 * Note that lock is infalliable.
 */
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle hnd) {
    locks::spinlock* lock = (locks::spinlock*)hnd;
    lock->lock();
    return UACPI_STATUS_OK;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle hnd, uacpi_cpu_flags) {
    locks::spinlock* lock = (locks::spinlock*)hnd;
    lock->unlock();
}

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
    asm volatile("nop");
    return UACPI_STATUS_OK;
}

/*
 * Waits for two types of work to finish:
 * 1. All in-flight interrupts installed via uacpi_kernel_install_interrupt_handler
 * 2. All work scheduled via uacpi_kernel_schedule_work
 *
 * Note that the waits must be done in this order specifically.
 */
uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    asm volatile("nop");
    return UACPI_STATUS_OK;
}

#endif // !UACPI_BAREBONES_MODE

#ifdef __cplusplus
}
#endif
