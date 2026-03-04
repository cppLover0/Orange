#include <arch/aarch64/cpu/el.hpp>
#include <generic/arch.hpp>
#include <klibc/stdio.hpp>
#include <generic/paging.hpp>
#include <generic/pmm.hpp>

#define KERNEL_STACK_SIZE 32 * 1024

void print_regs(aarch64::el::int_frame* ctx) {
    klibc::printf("Kernel panic\n\nReason: %s\n\r","cpu exception");
    klibc::printf("x0:  0x%p x1:  0x%p x2:  0x%p x3:  0x%p\nx4:  0x%p x5:  0x%p x6:  0x%p x7:  0x%p\nx8:  0x%p x9:  0x%p x10: 0x%p "
                  "x11: 0x%p\nx12: 0x%p x13: 0x%p x14: 0x%p x15: 0x%p\nx16: 0x%p x17: 0x%p x18: 0x%p x19: 0x%p\nx20: 0x%p "
                  "x21: 0x%p x22: 0x%p x23: 0x%p\nx24: 0x%p x25: 0x%p x26: 0x%p x27: 0x%p\nx28: 0x%p x29: 0x%p x30: 0x%p\r\n"
                  "sp: 0x%p pc: 0x%p flags: 0x%p, 1 %d",ctx->x[0],ctx->x[1],ctx->x[2],ctx->x[3],ctx->x[4],ctx->x[5],ctx->x[6],ctx->x[7],ctx->x[8],ctx->x[9],ctx->x[10],ctx->x[11],ctx->x[12],
                ctx->x[13],ctx->x[14],ctx->x[15],ctx->x[16],ctx->x[17],ctx->x[18],ctx->x[19],ctx->x[20],ctx->x[21],ctx->x[22],ctx->x[23],ctx->x[24],ctx->x[25],ctx->x[26],ctx->x[27],ctx->x[28],
            ctx->x[29], ctx->x[30],ctx->sp, ctx->ip, ctx->flags, 1);
}

std::uint8_t gaster[] = {
#embed "src/gaster.txt"
};


void print_ascii_art() {
    klibc::printf("%s\n",gaster);
}

[[gnu::weak]] void arch::panic(char* msg) {
    print_ascii_art();
    klibc::printf("Panic with message \"%s\"\r\n",msg);
}

extern "C" void C_fault_handler(aarch64::el::int_frame* frame) {
    print_ascii_art();
    print_regs(frame);
    arch::hcf();
}

extern "C" void C_irq_handler(aarch64::el::int_frame* frame) {
    print_ascii_art();
    print_regs(frame);
    arch::hcf();
}

extern "C" void C_userspace_fault_handler(aarch64::el::int_frame* frame) {
    print_ascii_art();
    (void)frame;
    print_regs(frame);
    arch::hcf();
}

extern "C" void C_userspace_irq_handler(aarch64::el::int_frame* frame) {
    print_ascii_art();
    (void)frame;
    print_regs(frame);
    arch::hcf();
}

extern "C" char int_table[];
std::uint64_t sp_el1_kernel_stack = 0;

void aarch64::el::init() {
    sp_el1_kernel_stack = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm());
    asm volatile("msr VBAR_EL1, %0" :: "r"(int_table));
    klibc::printf("EL: aarch64 int_table is 0x%p sp_el1 is 0x%p\r\n",int_table, sp_el1_kernel_stack);
}