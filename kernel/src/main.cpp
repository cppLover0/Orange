
#include <drivers/xhci.hpp>
#include <drivers/pci.hpp>

#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/syscalls/sockets.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/pic.hpp>
#include <arch/x86_64/interrupts/pit.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/smp.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#include <generic/mm/paging.hpp>
#include <generic/vfs/ustar.hpp>
#include <drivers/kvmtimer.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <drivers/serial.hpp>
#include <drivers/cmos.hpp>
#include <drivers/acpi.hpp>
#include <etc/assembly.hpp>
#include <drivers/tsc.hpp>
#include <etc/logging.hpp>
#include <uacpi/event.h>
#include <etc/etc.hpp>
#include <limine.h>

#include <generic/vfs/fd.hpp>

char is_shift_pressed = 0;
char is_ctrl_pressed = 0;

const char en_layout_translation[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

const char en_layout_translation_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};
\
#define ASCII_CTRL_OFFSET 96

#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA
#define CTRL_PRESSED 29
#define CTRL_RELEASED 157

#include <generic/locks/spinlock.hpp>

std::uint16_t KERNEL_GOOD_TIMER = HPET_TIMER;

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    Log::Display(LEVEL_MESSAGE_OK,"Got uacpi power_button !\n");
    return UACPI_INTERRUPT_HANDLED;
}

void interrupt_process_handle(void* arg) {
    while(1) {
        asm volatile("sti");
        asm volatile("pause");
        asm volatile("cli");
        yield();
    }
}

arch::x86_64::process_t* init = 0;

static void doKeyWork(uint8_t key,userspace_fd_t* fd_to_write) {

        if(key == SHIFT_PRESSED) {
            is_shift_pressed = 1;
            return;
        } else if(key == SHIFT_RELEASED) {
            is_shift_pressed = 0;
            return;
        }

        if(key == CTRL_PRESSED) {
            is_ctrl_pressed = 1;
            return;
        } else if(key == CTRL_RELEASED) {
            is_ctrl_pressed = 0;
            return;
        }

        if(!(key & (1 << 7))) {
            char layout_key;

            if(!is_shift_pressed)
                layout_key = en_layout_translation[key];
            else
                layout_key = en_layout_translation_shift[key];

            if(is_ctrl_pressed)
                layout_key = en_layout_translation[key] - ASCII_CTRL_OFFSET;

            vfs::vfs::write(fd_to_write,&layout_key,1);
     }
}

void ktty(void* arg) {
    userspace_fd_t kbd;
    userspace_fd_t tty;
    memset(&kbd,0,sizeof(userspace_fd_t));
    memset(&tty,0,sizeof(userspace_fd_t));
    memcpy(kbd.path,"/dev/ps2keyboard0",sizeof("/dev/ps2keyboard0"));
    memcpy(tty.path,"/dev/tty0",sizeof("/dev/tty0")); // tty0 is reserved for kernel tty

    while(1) {
        asm volatile("cli");
        std::int32_t kbd_poll = vfs::vfs::poll(&kbd,POLLIN);
        std::int32_t tty_poll = vfs::vfs::poll(&tty,POLLIN);
        asm volatile("sti");

        if(kbd_poll > 0) {
            char buffer[32];
            memset(buffer,0,32);
            asm volatile("cli");
            std::int64_t bytes_read = vfs::vfs::read(&kbd,buffer,32);
            for(int i = 0;i < bytes_read;i++) {
                doKeyWork(buffer[i], &tty);
            }
            asm volatile("sti");
        }

        if(tty_poll > 0) {
            char buffer[128];
            memset(buffer,0,128);
            asm volatile("cli");
            std::int64_t bytes_read = vfs::vfs::read(&tty,buffer,32);
            asm volatile("sti");
            if(bytes_read > 0) {
                Log::RawDisp(buffer,bytes_read);
            }
        }
        asm volatile("cli");
        yield();
        asm volatile("sti");
    }

}

extern "C" void main() {
    
    Other::ConstructorsInit();
    asm volatile("cli");

    drivers::serial serial(DEFAULT_SERIAL_PORT);

    __wrmsr(0xC0000101,0);

    memory::pmm::_physical::init();
    memory::heap::init();

    Log::Init();

    memory::paging::init();
    //Log::Display(LEVEL_MESSAGE_OK,"Paging initializied\n");

    arch::x86_64::cpu::gdt::init();
    //Log::Display(LEVEL_MESSAGE_OK,"GDT initializied\n");

    arch::x86_64::interrupts::idt::init();
    //Log::Display(LEVEL_MESSAGE_OK,"IDT initializied\n");

    arch::x86_64::interrupts::pic::init();
    Log::Display(LEVEL_MESSAGE_OK,"PIC initializied\n");

    drivers::kvmclock::init();
    drivers::acpi::init();
    Log::Display(LEVEL_MESSAGE_OK,"ACPI initializied\n");
    
    vfs::vfs::init();
    Log::Display(LEVEL_MESSAGE_OK,"VFS initializied\n");

    arch::x86_64::cpu::sse::init();
    //Log::Display(LEVEL_MESSAGE_OK,"SSE initializied\n");

    arch::x86_64::cpu::mp::init();
    arch::x86_64::cpu::mp::sync(0);
    Log::Display(LEVEL_MESSAGE_OK,"SMP initializied\n");

    arch::x86_64::scheduling::init();
    Log::Display(LEVEL_MESSAGE_OK,"Scheduling initializied\n");

    init = arch::x86_64::scheduling::create();
    extern int how_much_cpus;

    for(int i = 0;i < how_much_cpus; i++) {
        arch::x86_64::scheduling::create_kernel_thread(interrupt_process_handle,0); // we need to have processes which will do sti for interrupt waiting
    }

    xhci_init();

    arch::x86_64::scheduling::create_kernel_thread(ktty,0);

    drivers::pci::initworkspace();
    Log::Display(LEVEL_MESSAGE_OK,"PCI initializied\n");

    vfs::ustar::copy();
    Log::Display(LEVEL_MESSAGE_OK,"USTAR parsed\n");

    arch::x86_64::syscall::init();
    Log::Display(LEVEL_MESSAGE_OK,"Syscalls initializied\n");

    sockets::init();
    Log::Display(LEVEL_MESSAGE_OK,"Sockets initializied\n");

    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );

    char* argv[] = {0};
    char* envp[] = {"TERM=linux","SHELL=/bin/bash","PATH=/usr/bin:/bin",0};

    Log::Display(LEVEL_MESSAGE_INFO,"Trying to sync cpus...\n");
    arch::x86_64::cpu::mp::sync(1);

    arch::x86_64::scheduling::loadelf(init,"/bin/bash",argv,envp,0);
    arch::x86_64::scheduling::wakeup(init);

    vfs::fdmanager* fd = (vfs::fdmanager*)init->fd;

    int stdin = fd->create(init);
    int stdout = fd->create(init);
    int stderr = fd->create(init);

    userspace_fd_t *stdins = fd->search(init,stdin);
    userspace_fd_t *stdouts = fd->search(init,stdout);
    userspace_fd_t* stderrs = fd->search(init,stderr);

    memcpy(stdins->path,"/dev/pts/0",sizeof("/dev/pts/0"));
    memcpy(stdouts->path,"/dev/pts/0",sizeof("/dev/pts/0"));
    memcpy(stderrs->path,"/dev/pts/0",sizeof("/dev/pts/0"));

    stdins->index = 0;
    stdouts->index = 1;
    stderrs->index = 2;

    stdins->is_cached_path = 0;
    stdouts->is_cached_path = 0;
    stderrs->is_cached_path = 0;

    extern locks::spinlock* vfs_lock;
    extern locks::spinlock pmm_lock;

    Log::Display(LEVEL_MESSAGE_FAIL,"\e[1;1H\e[2J");
    arch::x86_64::cpu::lapic::tick(arch::x86_64::cpu::data()->lapic_block);

    dmesg("Now we are in userspace...");

    setwp();

    asm volatile("sti");
    while(1) {
        
        asm volatile("hlt");
    }

}
