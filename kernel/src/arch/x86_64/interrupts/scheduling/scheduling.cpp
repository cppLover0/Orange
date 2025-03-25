
#include <stdint.h>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/heap.hpp>
#include <generic/locks/spinlock.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/elf/elf.hpp>
#include <other/assert.hpp>
#include <other/hhdm.hpp>
#include <other/assembly.hpp>
#include <config.hpp>
#include <other/log.hpp>
#include <other/string.hpp>

uint64_t id_ptr = 0;

process_t* head_proc = 0;
process_t* last_proc = 0;

char proc_spinlock;

extern "C" void schedulingSchedule(int_frame_t* frame) {

    Paging::EnableKernel();

    cpudata_t* data = CpuData::Access();
    int_frame_t* frame1 = &data->temp_frame;
    process_t* proc = data->current;

    spinlock_lock(&proc_spinlock);

    data->current->status = PROCESS_STATUS_RUN;

    if(frame) 
        String::memcpy(&proc->ctx,frame,sizeof(int_frame_t));

    proc = proc->next;

    while(1) {
        while(proc) {
            if(proc != head_proc) {
                if(proc->ctx.rip && proc->ctx.rsp && proc->ctx.ss && proc->ctx.cs && proc->ctx.rflags && proc->status == PROCESS_STATUS_RUN) {
                    proc->status = PROCESS_STATUS_IN_USE;
                    data->current = proc;
                    String::memcpy(frame1,&proc->ctx,sizeof(int_frame_t));
                    spinlock_unlock(&proc_spinlock);
                    Lapic::EOI();
                    schedulingEnd(frame1);
                }
            }
            proc = proc->next;
        }
        proc = head_proc;
    }

}

void __process_load_queue(process_t* proc) {
    spinlock_lock(&proc_spinlock);
    if(!head_proc) {
        head_proc = proc;
        last_proc = head_proc;
        proc->status = PROCESS_STATUS_KILLED;
    } else {
        last_proc->next = proc;
        last_proc = proc;
    }
    
    spinlock_unlock(&proc_spinlock);
}

void Process::Init() {
    createProcess(0,0,0,0); // just load 0 as rip 
    idt_entry_t* entry = IDT::SetEntry(32,(void*)schedulingStub,0x8E);
    entry->ist = 2;
}

void Process::loadELFProcess(process_t* proc,uint8_t* base,char** argv,char** envp) {

    if(proc->type == PROCESS_TYPE_THREAD)
        return; 

    spinlock_lock(&proc_spinlock);

    while(proc->status == PROCESS_STATUS_RUN) // wait
        __nop();
    
    ELFLoadResult result = ELF::Load(base,(uint64_t*)HHDM::toVirt(proc->ctx.cr3),proc->user ? PTE_RW | PTE_PRESENT | PTE_USER : PTE_RW | PTE_PRESENT,proc->stack,argv,envp);
    proc->ctx.rsp = (uint64_t)result.ready_stack;
    proc->ctx.rip = (uint64_t)result.entry;
    proc->ctx.rdi = result.argc;
    proc->ctx.rsi = (uint64_t)result.argv;
    proc->ctx.rdx = (uint64_t)result.envp;
    spinlock_unlock(&proc_spinlock);
}

process_t* Process::ByID(uint64_t id) {
    
    process_t* proc = head_proc;
    while(proc) {
        if(proc->id == id)
            return proc;
        proc = proc->next;
    }

    return 0;

}

void Process::WakeUp(uint64_t id) {
    process_t* proc = ByID(id);
    proc->status = PROCESS_STATUS_RUN;
}

uint64_t Process::createThread(uint64_t rip,uint64_t parent) {
    process_t* parent_proc = ByID(parent);
    uint64_t i = createProcess(rip,1,parent_proc->user,(uint64_t*)parent_proc->ctx.cr3);
    process_t* proc = ByID(i);
    proc->parent_process = parent_proc->id;
    return i;
}

uint64_t Process::createProcess(uint64_t rip,char is_thread,char is_user,uint64_t* cr3_parent) {

    process_t* proc = (process_t*)PMM::VirtualAlloc();
    pAssert(proc,"No memory :(");
    proc->id = id_ptr++;
    proc->ctx.rip = rip;
    proc->user = is_user;
    proc->type = is_thread;
    proc->ctx.cs = is_user ? 0x18 : 0x08;
    proc->ctx.ss = is_user ? 0x20 : 0x10;
    proc->ctx.rflags = (1 << 9); // setup IF
    proc->status = PROCESS_STATUS_KILLED;

    uint64_t* stack_start = (uint64_t*)PMM::VirtualBigAlloc(PROCESS_STACK_SIZE);
    uint64_t* stack_end = (uint64_t*)((uint64_t)stack_start + (PROCESS_STACK_SIZE * PAGE_SIZE));

    proc->stack_start = stack_start;
    proc->stack = stack_end;
    proc->ctx.rsp = (uint64_t)stack_end;

    uint64_t* cr3;

    if(!is_thread) {
    
        cr3 = (uint64_t*)PMM::VirtualAlloc();

        pAssert(cr3 && stack_start,"No memory :(");

        Paging::MemoryEntry(cr3,LIMINE_MEMMAP_FRAMEBUFFER,PTE_PRESENT | PTE_RW | PTE_WC);
        Paging::MemoryEntry(cr3,LIMINE_MEMMAP_EXECUTABLE_AND_MODULES,PTE_PRESENT | PTE_RW);
        Paging::alwaysMappedMap(cr3);
        Paging::Kernel(cr3);

    } else {
        cr3 = (uint64_t*)HHDM::toVirt((uint64_t)cr3_parent);
    }

    uint64_t phys_stack = HHDM::toPhys((uint64_t)stack_start);
    for(uint64_t i = 0;i < PROCESS_STACK_SIZE;i++) {
        Paging::HHDMMap(cr3,phys_stack + (i * PAGE_SIZE),proc->user ? PTE_PRESENT | PTE_RW | PTE_USER : PTE_PRESENT | PTE_RW);
    }


    proc->ctx.cr3 = HHDM::toPhys((uint64_t)cr3);
    proc->next = 0;
    __process_load_queue(proc);
    
    return proc->id;

}