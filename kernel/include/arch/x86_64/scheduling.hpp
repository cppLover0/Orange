
#include <cstdint>

#pragma once

#include <arch/x86_64/syscalls/signal.hpp>

#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/vfs/vfs.hpp>

#include <etc/logging.hpp>

#include <generic/locks/spinlock.hpp>

#include <atomic>

#define PROCESS_STATE_NONE    0
#define PROCESS_STATE_KILLED  1
#define PROCESS_STATE_RUNNING 2
#define PROCESS_STATE_ZOMBIE  3

#define	WNOHANG		1	/* Don't block waiting.  */
#define	WUNTRACED	2	/* Report status of stopped children.  */

#define MIN2(a, b) ((a) < (b) ? (a) : (b))
#define MAX2(a, b) ((a) > (b) ? (a) : (b))

extern "C" void yield();

#define PREPARE_SIGNAL(proc) if(proc->sig->is_not_empty() && 1)
#define PROCESS_SIGNAL(proc) yield();

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elfheader_t;
  
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elfprogramheader_t;

typedef struct {
    std::uint64_t interp_entry;
    std::uint64_t real_entry;
    std::uint64_t phdr;
    std::uint64_t phnum;
    std::uint64_t phentsize;
    int status;
} elfloadresult_t;

#define __NGREG 23

#define __pollution(n) n

struct _fpxreg {
	unsigned short __pollution(significand)[4];
	unsigned short __pollution(exponent);
	unsigned short __padding[3];
};

struct _xmmreg {
	uint32_t __pollution(element)[4];
};

struct _fpstate {
	uint16_t __pollution(cwd);
	uint16_t __pollution(swd);
	uint16_t __pollution(ftw);
	uint16_t __pollution(fop);
	uint64_t __pollution(rip);
	uint64_t __pollution(rdp);
	uint32_t __pollution(mxcsr);
	uint32_t __pollution(mxcr_mask);
	struct _fpxreg _st[8];
	struct _xmmreg _xmm[16];
	uint32_t __padding[24];
};

typedef struct {
	unsigned long __pollution(gregs)[__NGREG];
	struct _fpstate *__pollution(fpregs);
	unsigned long __reserved1[8];
} mcontext_t;

#define REG_R8 0
#define REG_R9 1
#define REG_R10 2
#define REG_R11 3
#define REG_R12 4
#define REG_R13 5
#define REG_R14 6
#define REG_R15 7
#define REG_RDI 8
#define REG_RSI 9
#define REG_RBP 10
#define REG_RBX 11
#define REG_RDX 12
#define REG_RAX 13
#define REG_RCX 14
#define REG_RSP 15
#define REG_RIP 16
#define REG_EFL 17
#define REG_CSGSFS 18
#define REG_ERR 19
#define REG_TRAPNO 20
#define REG_OLDMASK 21
#define REG_CR2 22

inline void mcontext_to_int_frame(mcontext_t* out, int_frame_t* src) {
    src->r8 = out->gregs[REG_R8];
    src->r9 = out->gregs[REG_R9];
    src->r10 = out->gregs[REG_R10]; 
    src->r11 = out->gregs[REG_R11];
    src->r12 = out->gregs[REG_R12];
    src->r13 = out->gregs[REG_R13];
    src->r14 = out->gregs[REG_R14];
    src->r15 = out->gregs[REG_R15];
    src->rdi = out->gregs[REG_RDI];
    src->rsi = out->gregs[REG_RSI];
    src->rbp = out->gregs[REG_RBP];
    src->rbx = out->gregs[REG_RBX];
    src->rdx = out->gregs[REG_RDX];
    src->rax = out->gregs[REG_RAX];
    src->rcx = out->gregs[REG_RCX];
    src->rsp = out->gregs[REG_RSP];
    src->rip = out->gregs[REG_RIP];
    src->rflags = out->gregs[REG_EFL];
    src->cs = out->gregs[REG_CSGSFS]; // todo: implement SMEP
    src->ss = out->gregs[REG_ERR]; // why not
    if(src->cs > 0x30) {
        Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"broken src->cs for mcontext to int frame");
        asm volatile("hlt");
    }
}

inline void int_frame_to_mcontext(int_frame_t* src, mcontext_t* out) {
    out->gregs[REG_R8] = src->r8;
    out->gregs[REG_R9] = src->r9;
    out->gregs[REG_R10] = src->r10;
    out->gregs[REG_R11] = src->r11;
    out->gregs[REG_R12] = src->r12;
    out->gregs[REG_R13] = src->r13;
    out->gregs[REG_R14] = src->r14;
    out->gregs[REG_R15] = src->r15;
    out->gregs[REG_RDI] = src->rdi;
    out->gregs[REG_RSI] = src->rsi;
    out->gregs[REG_RBP] = src->rbp;
    out->gregs[REG_RBX] = src->rbx;
    out->gregs[REG_RDX] = src->rdx;
    out->gregs[REG_RAX] = src->rax;
    out->gregs[REG_RCX] = src->rcx;
    out->gregs[REG_RSP] = src->rsp;
    out->gregs[REG_RIP] = src->rip;
    out->gregs[REG_EFL] = src->rflags;
    out->gregs[REG_CSGSFS] = src->cs; 
    out->gregs[REG_ERR] = src->ss;
    if(!src->cs > 0x30) {
        Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"broken src->cs for int frame to mcontext");
        asm volatile("hlt");
    } 

    /* Other registers must be untouched (not implemented) */
}

enum ELF_Type {
    PT_NULL,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NOTE,
    PT_SHLIB,
    PT_PHDR,
    PT_LOPROC=0x70000000, //reserved
    PT_HIPROC=0x7FFFFFFF  //reserved
};

typedef enum {
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOPROC = 0xff00,
    ET_HIPROC = 0xffff
} obj_type_t;

typedef enum {
    AT_NULL = 0,
    AT_IGNORE = 1,
    AT_EXECFD = 2,
    AT_PHDR = 3,
    AT_PHENT = 4,
    AT_PHNUM = 5,
    AT_PAGESZ = 6,
    AT_BASE = 7,
    AT_FLAGS = 8,
    AT_ENTRY = 9,
    AT_NOTELF = 10,
    AT_UID = 11,
    AT_EUID = 12,
    AT_GID = 13,
    AT_EGID = 14,
    AT_PLATFORM = 15,
    AT_HWCAP = 16,
    AT_CLKTCK = 17,
    AT_SECURE = 23,
    AT_BASE_PLATFORM = 24,
    AT_RANDOM = 25,
    AT_HWCAP2 = 26,
    AT_RSEQ_FEATURE_SIZE = 27,
    AT_RSEQ_ALIGN = 28,
    AT_HWCAP3 = 29,
    AT_HWCAP4 = 30,
    AT_EXECFN = 31
} auxv_t;

extern "C" void schedulingScheduleAndChangeStack(std::uint64_t stack, int_frame_t* ctx);
extern "C" void schedulingSchedule(int_frame_t* ctx);
extern "C" void schedulingEnter();
extern "C" void schedulingEnd(int_frame_t* ctx);

namespace arch {
    namespace x86_64 {
        
        typedef struct process {
            std::uint32_t id;
            std::uint8_t status;
            std::uint8_t waitpid_state;

            std::uint64_t original_cr3;
            int_frame_t ctx;
            int_frame_t sig_ctx;

            int_frame_t* sys_ctx;

            std::atomic<int> target_cpu;

            std::uint64_t fs_base;

            locks::spinlock lock;
            locks::spinlock kill_lock; /* Never should be setup by not kill() function */

            locks::spinlock _3rd_kill_lock;
            locks::spinlock futex_lock;

            locks::spinlock fd_lock;

            void* default_sig_handler;
            void* sig_handlers[31];

            std::uint8_t is_sig_real;
            std::uint32_t alloc_fd;
            std::uint32_t* fd_ptr;

            std::uint64_t old_stack;

            int is_shared_fd;

            std::uint32_t reversedforid;
            std::uint32_t* vmm_id;

            userspace_fd_t* fd_pool;
            vfs::passingfd_manager* pass_fd;
            signalmanager* sig;

            int is_cloexec;

            void* fd;

            char* vmm_start;
            char* vmm_end;

            char* sse_ctx;
            char* cwd;
            char* name;

            int sys;

            int prio;

            std::uint64_t ts;

            int exit_code;
            int is_cloned;
            std::uint64_t* original_cr3_pointer;

            std::uint64_t futex;
            std::uint64_t syscall_stack;
            std::uint64_t user_stack;

            std::uint64_t create_timestamp;
            std::uint64_t exit_timestamp;

            std::uint32_t parent_id;

            int uid;

            int debug0;
            int debug1;

            int is_debug;

            struct process* next;

        } process_t;

static_assert(sizeof(process_t) < 4096,"process_t is bigger than page size");

        typedef struct process_queue_run_list {
            struct process_queue_run_list* next;
            process_t* proc;
            char is_used;
        } process_queue_run_list_t;
        
        class scheduling {
        public:
            static void init();
            static process_t* create();
            static process_t* fork(process_t* proc,int_frame_t* ctx);
            static process_t* clone(process_t* proc,int_frame_t* ctx);
            static process_t* by_pid(int pid);
            static void kill(process_t* proc);
            static void wakeup(process_t* proc);
            static int futexwake(process_t* proc, int* lock);
            static void futexwait(process_t* proc, int* lock, int val, int* original_lock, std::uint64_t ts);
            static int loadelf(process_t* proc,char* path,char** argv,char** envp,int free_mem);
            static process_t* head_proc_();

        };
    }
}

