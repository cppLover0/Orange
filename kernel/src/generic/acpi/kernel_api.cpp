#pragma once

#include <uacpi/types.h>
#include <uacpi/platform/arch_helpers.h>
#include <generic/limineA/limineinfo.hpp>
#include <drivers/pci/pci.hpp>
#include <drivers/io/io.hpp>
#include <drivers/serial/serial.hpp>
#include <generic/memory/heap.hpp>
#include <generic/memory/paging.hpp>
#include <generic/locks/spinlock.hpp>
#include <other/hhdm.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/assembly.hpp>
#include <uacpi/kernel_api.h>
#include <other/log.hpp>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>

#ifdef __cplusplus
extern "C" {
#endif

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

// Returns the PHYSICAL address of the RSDP structure via *out_rsdp_address.
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    LimineInfo info;
    *out_rsdp_address = info.rsdp_address;
    return UACPI_STATUS_OK;
}

/*
 * Open a PCI device at 'address' for reading & writing.
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

void uacpi_kernel_pci_device_close(uacpi_handle addr) {
    delete (void*)addr;
}

/*
 * Read & write the configuration space of a previously open PCI device.
 */
uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = PCI::IN(pci->bus,pci->device,pci->function,offset,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = PCI::IN(pci->bus,pci->device,pci->function,offset,2);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    *value = PCI::IN(pci->bus,pci->device,pci->function,offset,4);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    PCI::OUT(pci->bus,pci->device,pci->function,offset,(uint32_t)value,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    PCI::OUT(pci->bus,pci->device,pci->function,offset,(uint32_t)value,2);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    uacpi_pci_address* pci = (uacpi_pci_address*)device;
    PCI::OUT(pci->bus,pci->device,pci->function,offset,(uint32_t)value,4);
    return UACPI_STATUS_OK;
}

/*
 * Map a SystemIO address at [base, base + len) and return a kernel-implemented
 * handle that can be used for reading and writing the IO range.
 */
uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    uacpi_io_struct_t* io = new uacpi_io_struct_t;
    io->base = base;
    io->len = len;
    *out_handle = (uacpi_handle*)io;
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
 * You are NOT allowed to break e.g. a 4-byte access into four 1-byte accesses.
 * Hardware ALWAYS expects accesses to be of the exact width.
 */
uacpi_status uacpi_kernel_io_read8(
    uacpi_handle hndl, uacpi_size offset, uacpi_u8 *out_value
) {
    *out_value = (uint8_t)IO::IN(*(uint16_t*)hndl + offset,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(
    uacpi_handle hndl, uacpi_size offset, uacpi_u16 *out_value
) {
    *out_value = (uint16_t)IO::IN(*(uint16_t*)hndl + offset,2);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(
    uacpi_handle hndl, uacpi_size offset, uacpi_u32 *out_value
) {
    *out_value = (uint32_t)IO::IN(*(uint16_t*)hndl + offset,4);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle hndl, uacpi_size offset, uacpi_u8 in_value
) {
    IO::OUT(*(uint16_t*)hndl + offset,in_value,1);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(
    uacpi_handle hndl, uacpi_size offset, uacpi_u16 in_value
) {
    IO::OUT(*(uint16_t*)hndl + offset,in_value,2);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(
    uacpi_handle hndl, uacpi_size offset, uacpi_u32 in_value
) {
    IO::OUT(*(uint16_t*)hndl + offset,in_value,4);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uint64_t start = ALIGNPAGEDOWN(addr);
    void* base = (void*)HHDM::toVirt(addr);
    for(uint64_t i = start; i < start + len;i+=PAGE_SIZE) {
        Paging::KernelMap(i);
    }   
    return base;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    // hhdm doesnt need unmap
}

/*
 * Allocate a block of memory of 'size' bytes.
 * The contents of the allocated memory are unspecified.
 */
void *uacpi_kernel_alloc(uacpi_size size) {
    return KHeap::Malloc(size);
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
    delete mem;
}
#else
void uacpi_kernel_free(void *mem, uacpi_size size_hint);
#endif

#ifndef UACPI_FORMATTED_LOGGING
void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* str) {
    int level = LOG_LEVEL_INFO;
    if(lvl == UACPI_LOG_WARN)
        level = LOG_LEVEL_WARNING;
    else if(lvl == UACPI_LOG_ERROR)
        level = LOG_LEVEL_ERROR;

    Log(level,"%s",str);

}
#else
UACPI_PRINTF_DECL(2, 3)
void uacpi_kernel_log(uacpi_log_level, const uacpi_char*, ...);
void uacpi_kernel_vlog(uacpi_log_level, const uacpi_char*, uacpi_va_list);
#endif

/*
 * Returns the number of nanosecond ticks elapsed since boot,
 * strictly monotonic.
 */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return HPET::NanoCurrent();
}

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec) {
    HPET::Sleep(usec);
}

/*
 * Sleep for N milliseconds.
 */
void uacpi_kernel_sleep(uacpi_u64 msec) {
    HPET::Sleep(msec * 1000);
}

/*
 * Create/free an opaque non-recursive kernel mutex object.
 */
uacpi_handle uacpi_kernel_create_mutex(void) {
    char* lock = new char;
    *lock = 0;
    return (uacpi_handle)lock;
}
void uacpi_kernel_free_mutex(uacpi_handle hdnl) {
    delete (void*)hdnl;
}

/*
 * Create/free an opaque kernel (semaphore-like) event object.
 */
uacpi_handle uacpi_kernel_create_event(void) {
    return (uacpi_handle)1;
}
void uacpi_kernel_free_event(uacpi_handle) {
    __nop();
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
    spinlock_lock((char*)lock);
    return UACPI_STATUS_OK;
}
void uacpi_kernel_release_mutex(uacpi_handle lock) {
    spinlock_unlock((char*)lock);
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
    __nop();
}

/*
 * Reset the event counter to 0.
 */
void uacpi_kernel_reset_event(uacpi_handle) {
    __nop();
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
    uint8_t vector = IDT::AllocEntry();
    *out_irq_handle = (uacpi_handle)vector;
    idt_entry_t* entry = IDT::SetEntry(vector,(void*)base,0x8E);
    IOAPIC::SetEntry(vector,irq,0,Lapic::ID());
    entry->ist = 2;
    //Log("Setupping UACPI vector at vec: %d irq: %d\n",vector,irq);
    return UACPI_STATUS_OK;
}

/*
 * Uninstall an interrupt handler. 'irq_handle' is the value returned via
 * 'out_irq_handle' during installation.
 */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    return UACPI_STATUS_OK; // i dont implemented this
}


/*
 * Create/free a kernel spinlock object.
 *
 * Unlike other types of locks, spinlocks may be used in interrupt contexts.
 */
uacpi_handle uacpi_kernel_create_spinlock(void) {
    char* lock = new char;
    return (uacpi_handle)lock;
}
void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    delete (void*)lock;
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
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    spinlock_lock((char*)lock);
    return 1;
}
void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags) {
    spinlock_unlock((char*)lock);
}

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
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
    return UACPI_STATUS_OK;
}

#ifdef __cplusplus
}
#endif
