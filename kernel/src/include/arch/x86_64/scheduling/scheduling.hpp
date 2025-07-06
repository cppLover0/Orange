
#include <stdint.h>
#include <arch/x86_64/interrupts/idt.hpp>

#pragma once

#define PROCESS_TYPE_PROCESS 0
#define PROCESS_TYPE_THREAD 1 
#define PROCESS_TYPE_STACK_DEALLOCATED 2

#define PROCESS_STATUS_RUN 0
#define PROCESS_STATUS_IN_USE 1
#define PROCESS_STATUS_KILLED 2
#define PROCESS_STATUS_BLOCKED 3
#define PROCESS_STATUS_WAITING 4

typedef struct process_struct {
    uint64_t id;
    int return_status;

    struct process_struct* next;
    struct process_struct* parent;

    _Atomic char type;
    char is_blocked;
    _Atomic char status;
    char user;
    char* cwd;
    char* name;

    char* vmm_start;
    char* vmm_last;

    uint64_t* stack;
    uint64_t* stack_start; // to free if process killed

    uint64_t user_stack_start;

    uint64_t* wait_stack; // to waitings in syscall
    int_frame_t* syscall_wait_ctx;

    uint64_t fs_base;

    uint64_t parent_process; // for threads

    int* futex; // for futex

    int fd_ptr;
    char* start_fd; // i cant include loop sooo i just put char*
    char* last_fd; // optimizations

    uint8_t cs;
    uint8_t ss;

    uint16_t target_cpu;

    char* wait_pipe;
    
    char is_eoi;
    char is_cli;

    char is_waitpid_used;

    uint64_t nah_cr3;

    int_frame_t ctx;
    char* sse_ctx;
} process_t;

typedef struct process_queue_run_list {
    struct process_queue_run_list* next;
    process_t* proc;
    char is_used;
} process_queue_run_list_t;

extern "C" void schedulingEnd(int_frame_t* ctx,uint64_t* cr3);
extern "C" void schedulingStub();

extern "C" void schedulingSchedule(int_frame_t* frame);
extern "C" void schedulingScheduleStack(int_frame_t* frame);

process_t* get_head_proc();

class Process {
public:

    static void Init();

    static process_t* ByID(uint64_t id);

    static uint64_t createProcess(uint64_t rip,char is_thread,char is_user,uint64_t* cr3_parent,uint64_t parent_id);

    static void WakeUp(uint64_t id);

    static uint64_t createThread(uint64_t rip,uint64_t parent);

    static int loadELFProcess(uint64_t procid,char* path,uint8_t* elf,char** argv,char** envp);

    static void futexWait(process_t* proc, int* lock,int val);

    static void futexWake(process_t* parent,int* lock);

    static void Kill(process_t* proc,int return_status);

};