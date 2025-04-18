
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
    int return_status;

    struct process_struct* next;

    char type;
    char status;
    char user;
    uint64_t* stack;
    uint64_t* stack_start; // to free if process killed

    uint64_t parent_process; // for threads

    uint8_t cs;
    uint8_t ss;
    
    char is_eoi;
    char is_cli;

    int_frame_t ctx;
} __attribute__((packed)) process_t;

extern "C" void schedulingEnd(int_frame_t* ctx,uint64_t* cr3);
extern "C" void schedulingStub();

extern "C" void schedulingSchedule(int_frame_t* frame);

class Process {
public:

    static void Init();

    static process_t* ByID(uint64_t id);

    static uint64_t createProcess(uint64_t rip,char is_thread,char is_user,uint64_t* cr3_parent);

    static void WakeUp(uint64_t id);

    static uint64_t createThread(uint64_t rip,uint64_t parent);

    static void loadELFProcess(uint64_t procid,uint8_t* elf,char** argv,char** envp);

};