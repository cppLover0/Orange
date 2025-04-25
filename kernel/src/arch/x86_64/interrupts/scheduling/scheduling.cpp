
#include <stdint.h>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>
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
#include <other/other.hpp>

uint64_t id_ptr = 0;

process_t* head_proc = 0;
process_t* last_proc = 0;

char proc_spinlock;
char proc_spinlock2;
char proc_spinlock3;

extern "C" void schedulingSchedule(int_frame_t* frame) {

    Paging::EnableKernel();

    cpudata_t* data = CpuData::Access();
    int_frame_t* frame1 = &data->temp_frame;
    process_t* proc = data->current;
    
    if(data->current) {

        if(proc->status != PROCESS_STATUS_BLOCKED) {
            if(frame) {
                String::memcpy(&proc->ctx,frame,sizeof(int_frame_t));
                proc->fs_base = __rdmsr(0xC0000100);
            }

            if(proc->status == PROCESS_STATUS_IN_USE)
                data->current->status = PROCESS_STATUS_RUN;
        }

        data->current = 0;

        proc = proc->next;

    } else
        proc = head_proc;

    spinlock_lock(&proc_spinlock); // three level spinlock !!!!
    spinlock_lock(&proc_spinlock2);
    spinlock_lock(&proc_spinlock3);

    while(1) {
        while(proc) {
            if(proc != head_proc) {
                if(proc->status == PROCESS_STATUS_RUN) {
                    proc->status = PROCESS_STATUS_IN_USE;
                    data->current = proc;

                    String::memcpy(frame1,&proc->ctx,sizeof(int_frame_t));
                    __wrmsr(0xC0000100,proc->fs_base);

                    if(proc->is_cli) 
                        frame1->rflags &= ~(1 << 9); // clear IF
                
                    if(frame1->cs & 3)
                        frame1->ss |= 3;
                    
                    if(frame1->ss & 3)
                        frame1->cs |= 3;

                    if(proc->is_eoi)
                        Lapic::EOI(); // for kernel mode proc-s

                    // Log("0x%p 0x%p 0x%p\n",frame1->cs,frame->ss,frame->rip);

                    spinlock_unlock(&proc_spinlock);
                    spinlock_unlock(&proc_spinlock2);
                    spinlock_unlock(&proc_spinlock3);
                    schedulingEnd(frame1,(uint64_t*)frame1->cr3);
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

process_t* get_head_proc() {
    return head_proc;
}

void Process::Init() {
    createProcess(0,0,0,0,0); // just load 0 as rip 
    idt_entry_t* entry = IDT::SetEntry(32,(void*)schedulingStub,0x8E);
    entry->ist = 2;
}

void Process::loadELFProcess(uint64_t procid,char* path,uint8_t* elf,char** argv,char** envp) {    
    
    process_t* proc = ByID(procid);
    uint64_t* vcr3 = (uint64_t*)HHDM::toVirt(proc->ctx.cr3);

    proc->cwd = cwd_get((const char*)path);
    
    char* name = (char*)PMM::VirtualAlloc();
    String::memcpy(name,path,String::strlen(path));
    proc->name = name;

    ELFLoadResult l = ELF::Load((uint8_t*)elf,vcr3,proc->user ? PTE_RW | PTE_PRESENT | PTE_USER : PTE_RW | PTE_PRESENT,proc->stack,argv,envp);

    //proc->ctx.rsp = (uint64_t)l.ready_stack;
    proc->ctx.rip = (uint64_t)l.entry;
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
    uint64_t i = createProcess(rip,1,parent_proc->user,(uint64_t*)HHDM::toVirt(parent_proc->ctx.cr3),parent_proc->id);
    process_t* proc = ByID(i);
    
    char* name = (char*)PMM::VirtualAlloc();
    String::memcpy(name,parent_proc->name,String::strlen(parent_proc->name));
    proc->name = name;
    proc->cwd = cwd_get(name);

    proc->parent_process = parent_proc->id;
    return i;
}

void Process::futexWait(process_t* proc, int* lock,int val) {

    int st = proc->status;
    proc->status = PROCESS_STATUS_BLOCKED;

    if(lock && proc->ctx.cr3) {
        Paging::EnablePaging((uint64_t*)HHDM::toVirt(proc->ctx.cr3)); // enter to userspace paging

        int lock_value = *lock;

        Paging::EnableKernel();

        if(lock_value == val) {
            proc->futex = lock;
            proc->status = PROCESS_STATUS_BLOCKED;
            return;
        }
    }

    proc->status = st; // restore

}

void Process::futexWake(process_t* parent,int* lock) {

    process_t* proc = head_proc;
    while(proc) {
        
        if(proc->futex == lock && parent->id == proc->parent_process) {
            proc->futex = 0;
            proc->status = PROCESS_STATUS_RUN;
            return;
        }

        proc = proc->next;
    }

}

uint64_t Process::createProcess(uint64_t rip,char is_thread,char is_user,uint64_t* cr3_parent,uint64_t parent_id) {

    process_t* proc = (process_t*)PMM::VirtualAlloc();
    pAssert(proc,"No memory :(");
    proc->id = id_ptr++;
    proc->ctx.rip = rip;
    proc->user = is_user;
    proc->type = is_thread;
    if(is_user) {
        proc->ctx.cs = 0x20 | 3;
        proc->ctx.ss = 0x18 | 3;
    } else {
        proc->ctx.cs = 0x08;
        proc->ctx.ss = 0;
    }
    proc->cs = proc->ctx.cs;
    proc->ss = proc->ctx.ss;
    proc->is_eoi = 1;
    proc->ctx.rflags = (1 << 9); // setup IF
    proc->status = PROCESS_STATUS_KILLED;
    proc->parent_process = parent_id;
    proc->futex = 0;

    uint64_t* stack_start = (uint64_t*)PMM::VirtualBigAlloc(PROCESS_STACK_SIZE);
    uint64_t* stack_end = (uint64_t*)((uint64_t)stack_start + (PROCESS_STACK_SIZE * PAGE_SIZE));

    proc->stack_start = stack_start;
    proc->stack = stack_end;
    proc->ctx.rsp = (uint64_t)stack_end;

    fd_t* stdin = (fd_t*)PMM::VirtualAlloc(); //head fd
    stdin->index = 0;
    stdin->pipe.buffer = (char*)PMM::VirtualBigAlloc(16);
    stdin->pipe.buffer_size = 5;
    stdin->pipe.is_received = 1;
    stdin->parent = 0;
    stdin->next = 0;
    stdin->proc = proc;
    stdin->type = FD_PIPE; 

    String::memcpy(stdin->path_point,"/dev/kbd",sizeof("/dev/kbd"));
    String::memcpy(stdin->pipe.buffer,"how are you reading this ???\n",5);

    proc->start_fd = (char*)stdin;
    proc->last_fd = (char*)stdin;

    proc->wait_stack = (uint64_t*)PMM::VirtualAlloc();
    proc->syscall_wait_ctx = (int_frame_t*)PMM::VirtualAlloc();

    proc->fd_ptr = 1;

    int fd = FD::Create(proc,0);
    fd_t* stdout = FD::Search(proc,fd);
    String::memcpy(stdout->path_point,"/dev/tty",sizeof("/dev/tty"));

    fd = FD::Create(proc,0);
    fd_t* stderr = FD::Search(proc,fd);
    String::memcpy(stderr->path_point,"/dev/serial",sizeof("/dev/serial"));

    proc->termios = (termios_t*)PMM::VirtualAlloc();

    String::memset(proc->termios,0,sizeof(termios_t));

    uint64_t* cr3 = (uint64_t*)PMM::VirtualAlloc();

    pAssert(cr3 && stack_start,"No memory :(");

    if(is_thread) {
        PMM::VirtualFree(cr3);
        cr3 = cr3_parent;
    } else {
        Paging::Kernel(cr3);
        Paging::alwaysMappedMap(cr3);
    }


    Paging::HHDMMap(cr3,HHDM::toPhys((uint64_t)proc->syscall_wait_ctx),PTE_PRESENT | PTE_RW);
    Paging::HHDMMap(cr3,HHDM::toPhys((uint64_t)proc->wait_stack),PTE_PRESENT | PTE_RW);
    uint64_t phys_stack = HHDM::toPhys((uint64_t)stack_start);
    for(uint64_t i = 0;i < PROCESS_STACK_SIZE;i++) {
        Paging::HHDMMap(cr3,phys_stack + (i * PAGE_SIZE),proc->user ? PTE_PRESENT | PTE_RW | PTE_USER : PTE_PRESENT | PTE_RW);
    }

    proc->ctx.cr3 = HHDM::toPhys((uint64_t)cr3);
    proc->next = 0;
    __process_load_queue(proc);
    
    return proc->id;

}