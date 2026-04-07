#include <cstdint>
#include <generic/bootloader/bootloader.hpp>
#include <utils/cxx/cxx_constructors.hpp>
#include <generic/arch.hpp>
#include <utils/flanterm.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/heap.hpp>
#include <drivers/acpi.hpp>
#include <generic/time.hpp>
#include <drivers/nvme.hpp>
#include <drivers/powerbutton.hpp>
#include <generic/mp.hpp>
#include <generic/vfs.hpp>
#include <drivers/xhci.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/tty.hpp>
#include <generic/fbdev.hpp>
#include <generic/sysfs.hpp>
#include <generic/modules.hpp>
#include <utils/cmdline.hpp>
#include <generic/elf.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/drivers/pci.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <arch/x86_64/drivers/serial.hpp>
#endif

extern std::size_t memory_size;
extern int is_early;

void scheduling_test(void* arg) {
    static locks::mutex lock;
    arch::disable_interrupts();
    while(true) {
        lock.lock();
        klibc::printf("%lli",arg);
        lock.unlock();
        time::timer->sleep(5000);
        process::yield();
    }
}

extern "C" void main() {
    utils::cxx::init_constructors();
    bootloader::init();
#if defined(__x86_64__)
    x86_64::serial::init();
#endif
    pmm::init();
    paging::init();
    kheap::init();
    utils::flanterm::init();
    utils::flanterm::fullinit();
    cmdline::init();
    log("pmm", "total usable memory: %lli bytes", memory_size);
    log("paging", "enabled kernel root with %d level paging", arch::level_paging());
    log("kheap", "available memory %lli bytes", KHEAP_SIZE);

    thread* init_thread = process::create_process(true);
    init_thread->fd = (void*)(new vfs::fdmanager);
    init_thread->vmem = new vmm;
    init_thread->vmem->root = &init_thread->original_root;

    acpi::init_tables();
    arch::init(ARCH_INIT_EARLY);
    if(!cmdline::parser->contains("noacpi")) {
        acpi::full_init();
    } else {
        log("acpi", "full acpi init is disabled with cmdline");
    }
    is_early = 0;
    arch::init(ARCH_INIT_COMMON);
    mp::init();
    mp::sync();
    vfs::init();
    if(!cmdline::parser->contains("noacpi")) {
        drivers::powerbutton::init();
    } else {
        log("powerbutton", "no acpi means no power button");
    }
    drivers::nvme::init();
    xhci_init();

#if defined(__x86_64__)
    x86_64::pci::initworkspace();
    log("pci", "launched all drivers");
#endif
    tty::init();
    fbdev::init();
    //sysfs::dump();

    modules::init();

    // thread* thread = process::kthread(scheduling_test, (void*)1);
    // process::wakeup(thread);
    // thread = process::kthread(scheduling_test, (void*)2);
    // process::wakeup(thread);
    // thread = process::kthread(scheduling_test, (void*)3);
    // process::wakeup(thread);
    char init[256] = {};
    file_descriptor initfd = {};
    stat initstat = {};

    assert(cmdline::parser->contains("init"), "there's no init !");
    cmdline::parser->find("init", init, 256);

    log("init", "loading init %s", init);

    int status = vfs::open(&initfd, init, true, false);
    assert(status == 0, "there's no init ! (status %d)", status);
    assert(initfd.vnode.read, "no.");
    assert(initfd.vnode.stat, "no 2x.");
    assert(initfd.vnode.stat(&initfd, &initstat) == 0, "helo");

    char* init_buffer = (char*)(pmm::buddy::alloc(initstat.st_size).phys + etc::hhdm());
    initfd.vnode.read(&initfd, init_buffer, initstat.st_size);

    assert(elf::is_valid_elf(init_thread ,init_buffer), "init is not valid elf ! (%s)", init);

    pmm::buddy::free((std::uint64_t)init_buffer - etc::hhdm());

    char* argv[] = {0};
    char* envp[] = {0};

    init_thread->vmem->init_root();
    elf::exec(init_thread, init, argv, envp);

    vfs::fdmanager* manager = (vfs::fdmanager*)init_thread->fd;
    manager->fd_ptr = &init_thread->fd_ptr;
    file_descriptor* stdio = manager->createlowest(-1);
    file_descriptor* stdout = manager->createlowest(-1);
    file_descriptor* stderr = manager->createlowest(-1);
    status = 0;

    status = vfs::open(stdio, (char*)"/dev/pts/0", false, false);
    assert(status == 0, "no tty :( %d",status);

    status = vfs::open(stdout, (char*)"/dev/pts/0", false, false);
    assert(status == 0, "no tty :( %d",status);

    status = vfs::open(stderr, (char*)"/dev/pts/0", false, false);
    assert(status == 0, "no tty :( %d",status);

    process::wakeup(init_thread);

    log("main", "Boot is done");

    arch::enable_interrupts();
    while(1) {
        //klibc::printf("current sec %lli\r\n",time::timer->current_nano() / (1000 * 1000 * 1000));
        arch::wait_for_interrupt();
    }
}
