
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

#define PREPARE_SIGNAL(proc) if(proc->sig->is_not_empty_sigset(&proc->current_sigset) && 1)
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
    std::uint64_t interp_base;
    std::uint64_t base;
} elfloadresult_t;

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

typedef struct {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} Elf64_Dyn;

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_STRSZ    10

typedef uint64_t u64;

# define CSIGNAL       0x000000ff /* Signal mask to be sent at exit.  */
# define CLONE_VM      0x00000100 /* Set if VM shared between processes.  */
# define CLONE_FS      0x00000200 /* Set if fs info shared between processes.  */
# define CLONE_FILES   0x00000400 /* Set if open files shared between processes.  */
# define CLONE_SIGHAND 0x00000800 /* Set if signal handlers shared.  */
# define CLONE_PIDFD   0x00001000 /* Set if a pidfd should be placed
				     in parent.  */
# define CLONE_PTRACE  0x00002000 /* Set if tracing continues on the child.  */
# define CLONE_VFORK   0x00004000 /* Set if the parent wants the child to
				     wake it up on mm_release.  */
# define CLONE_PARENT  0x00008000 /* Set if we want to have the same
				     parent as the cloner.  */
# define CLONE_THREAD  0x00010000 /* Set to add to same thread group.  */
# define CLONE_NEWNS   0x00020000 /* Set to create new namespace.  */
# define CLONE_SYSVSEM 0x00040000 /* Set to shared SVID SEM_UNDO semantics.  */
# define CLONE_SETTLS  0x00080000 /* Set TLS info.  */
# define CLONE_PARENT_SETTID 0x00100000 /* Store TID in userlevel buffer
					   before MM copy.  */
# define CLONE_CHILD_CLEARTID 0x00200000 /* Register exit futex and memory
					    location to clear.  */
# define CLONE_DETACHED 0x00400000 /* Create clone detached.  */
# define CLONE_UNTRACED 0x00800000 /* Set if the tracing process can't
				      force CLONE_PTRACE on this clone.  */
# define CLONE_CHILD_SETTID 0x01000000 /* Store TID in userlevel buffer in
					  the child.  */
# define CLONE_NEWCGROUP    0x02000000	/* New cgroup namespace.  */
# define CLONE_NEWUTS	0x04000000	/* New utsname group.  */
# define CLONE_NEWIPC	0x08000000	/* New ipcs.  */
# define CLONE_NEWUSER	0x10000000	/* New user namespace.  */
# define CLONE_NEWPID	0x20000000	/* New pid namespace.  */
# define CLONE_NEWNET	0x40000000	/* New network namespace.  */
# define CLONE_IO	0x80000000	/* Clone I/O context.  */

struct clone_args {
    u64 flags;        /* Flags bit mask */
    u64 pidfd;        /* Where to store PID file descriptor
                         (int *) */
    u64 child_tid;    /* Where to store child TID,
                         in child's memory (pid_t *) */
    u64 parent_tid;   /* Where to store child TID,
                         in parent's memory (pid_t *) */
    u64 exit_signal;  /* Signal to deliver to parent on
                         child termination */
    u64 stack;        /* Pointer to lowest byte of stack */
    u64 stack_size;   /* Size of stack */
    u64 tls;          /* Location of new TLS */
    u64 set_tid;      /* Pointer to a pid_t array
                         (since Linux 5.5) */
    u64 set_tid_size; /* Number of elements in set_tid
                         (since Linux 5.5) */
    u64 cgroup;       /* File descriptor for target cgroup
                         of child (since Linux 5.7) */
};

struct stimeval {
        long long    tv_sec;         /* seconds */
        long long    tv_usec;        /* microseconds */
};


struct itimerval {
    struct stimeval it_interval; /* следующее значение */
    struct stimeval it_value;    /* текущее значение */
};

#define ITIMER_REAL             0
#define ITIMER_VIRTUAL          1
#define ITIMER_PROF             2

namespace arch {
    namespace x86_64 {

        struct sigset_list {
            int sig;
            sigset_t sigset;
            sigset_list* next;
        };
        
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

            itimerval itimer;
            std::int64_t next_alarm;

            itimerval vitimer;
            std::int64_t virt_timer;

            itimerval proftimer;
            std::int64_t prof_timer;

            sigset_t current_sigset;
            sigset_t temp_sigset;
            int is_restore_sigset;

            void* sig_handlers[36];
            void* ret_handlers[36];
            int sig_flags[36];

            std::atomic<int> is_execd;

            sigset_t sigsets[36];
            stack_t altstack;

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

            sigtrace* sigtrace_obj;

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

            std::uint64_t time_start;

            std::uint64_t futex;
            std::uint64_t syscall_stack;
            std::uint64_t user_stack;

            std::uint64_t create_timestamp;
            std::uint64_t exit_timestamp;

            std::uint64_t thread_group; // just parent pid

            std::uint32_t parent_id;
            std::uint32_t exit_signal;

            int* tidptr;
            int uid;

            int debug0;
            int debug1;

            int is_debug;

            int sex_abort;

            struct process* next;

        } process_t;

        inline void update_time(itimerval* itimer, std::int64_t* output, int is_tsc) {
            if(itimer->it_value.tv_sec != 0 || itimer->it_value.tv_usec != 0) {
                *output = (itimer->it_value.tv_usec) + (itimer->it_value.tv_sec * (1000*1000)) + (is_tsc ? drivers::tsc::currentus() : 0);
            } else if(itimer->it_interval.tv_sec != 0 || itimer->it_interval.tv_usec != 0) {
                *output = (itimer->it_interval.tv_usec) + (itimer->it_interval.tv_sec * (1000*1000)) + (is_tsc ? drivers::tsc::currentus() : 0);
            }
        }

        inline sigset_t* get_sigset_from_list(process_t* proc, int sig) {

            return 0;
        }

        inline void free_sigset_from_list(process_t* proc) {
            return;
        } 

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
            static process_t* clone3(process_t* proc, clone_args* clarg, int_frame_t* ctx);
            static process_t* by_pid(int pid);
            static void create_kernel_thread(void (*func)(void*),void* arg);
            static void kill(process_t* proc);
            static void wakeup(process_t* proc);
            static int futexwake(process_t* proc, int* lock, int num_to_wake);
            static void futexwait(process_t* proc, int* lock, int val, int* original_lock, std::uint64_t ts);
            static int loadelf(process_t* proc,char* path,char** argv,char** envp,int free_mem);
            static process_t* head_proc_();

            static void sigreturn(process_t* proc);

        };
    }
}

