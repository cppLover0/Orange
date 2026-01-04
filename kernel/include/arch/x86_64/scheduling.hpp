
#include <cstdint>

#pragma once

#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/vfs/vfs.hpp>

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

            std::atomic<int> target_cpu;

            std::uint64_t fs_base;

            locks::spinlock lock;
            locks::spinlock kill_lock; /* Never should be setup by not kill() function */

            locks::spinlock _3rd_kill_lock;
            locks::spinlock futex_lock;

            locks::spinlock fd_lock;

            std::uint32_t alloc_fd;
            std::uint32_t* fd_ptr;

            int is_shared_fd;

            std::uint32_t reversedforid;
            std::uint32_t* vmm_id;

            userspace_fd_t* fd_pool;
            vfs::passingfd_manager* pass_fd;

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

