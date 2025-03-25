
#include <stdint.h>
#include <arch/x86_64/interrupts/idt.hpp>

#pragma once

#define PROCESS_TYPE_PROCESS 0
#define PROCESS_TYPE_THREAD 1 
#define PROCESS_TYPE_STACK_DEALLOCATED 2

#define PROCESS_STATUS_RUN 0
#define PROCESS_STATUS_IN_USE 1
#define PROCESS_STATUS_KILLED 2

typedef struct process_struct {
    uint64_t id;

    struct process_struct* next;

    char type;
    char status;
    char user;
    uint64_t* stack;
    uint64_t* stack_start; // to free if process killed

    uint64_t parent_process; // for threads

    int_frame_t ctx;
} __attribute__((packed)) process_t;

extern "C" void schedulingEnd(int_frame_t* ctx);
extern "C" void schedulingStub();

class Process {
public:

    static void Init();

    static process_t* ByID(uint64_t id);

    static uint64_t createProcess(uint64_t rip,char is_thread,char is_user,uint64_t* cr3_parent);

    static void WakeUp(uint64_t id);

    static uint64_t createThread(uint64_t rip,uint64_t parent);

    static void loadELFProcess(process_t* proc,uint8_t* base,char** argv,char** envp);

};