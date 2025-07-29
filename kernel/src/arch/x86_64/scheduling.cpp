
#include <cstdint>
#include <arch/x86_64/scheduling.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/sse.hpp>

#include <generic/locks/spinlock.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>

#include <etc/libc.hpp>
#include <etc/list.hpp>

#include <etc/logging.hpp>

#include <atomic>

#include <config.hpp>

arch::x86_64::process_t* head_proc;
std::uint32_t id_ptr = 0;

locks::spinlock* process_lock;

#define PUT_STACK(dest,value) *--dest = value;
#define PUT_STACK_STRING(dest,string) memcpy(dest,string,strlen(string)); 
    

uint64_t __elf_get_length(char** arr) {
    uint64_t counter = 0;

    while(arr[counter]) 
        counter++;

    return counter;
}

uint64_t* __elf_copy_to_stack(char** arr,uint64_t* stack,char** out, uint64_t len) {

    uint64_t* temp_stack = stack;

    for(uint64_t i = 0;i < len; i++) {
        PUT_STACK_STRING(temp_stack,arr[i])
        out[i] = (char*)temp_stack;
        PUT_STACK(temp_stack,0);
        temp_stack -= ALIGNUP(strlen(arr[i]),8);
    }

    return temp_stack;

}

uint64_t* __elf_copy_to_stack_without_out(uint64_t* arr,uint64_t* stack,uint64_t len) {

    uint64_t* _stack = stack;

    for(uint64_t i = 0;i < len;i++) {
        PUT_STACK(_stack,arr[i]);
    }   

    return _stack;

}

elfloadresult_t __scheduling_load_elf(arch::x86_64::process_t* proc, std::uint8_t* base) {
    elfheader_t* head = (elfheader_t*)base;
    elfprogramheader_t* current_head;
    std::uint64_t elf_base = UINT64_MAX;
    std::uint64_t size = 0;
    std::uint64_t phdr = 0;
    char ELF[4] = {0x7F,'E','L','F'};
    if(strncmp((const char*)head->e_ident,ELF,4))
        return {0,0,0,0,0};

    elfloadresult_t elfload;
    elfload.real_entry = head->e_entry;
    elfload.interp_entry = head->e_entry;
    elfload.phnum = head->e_phnum;
    elfload.phentsize = head->e_phentsize;

    char is_interp = 0;

    for(int i = 0; i < head->e_phnum; i++) {
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);

        if (current_head->p_type == PT_LOAD) {
            elf_base = MIN2(elf_base, current_head->p_vaddr);
        }

    }

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t end = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);

        if(current_head->p_type == PT_PHDR) {
            phdr = current_head->p_vaddr;
            elfload.phdr = phdr;
        } else if(current_head->p_type == PT_INTERP) {

            vfs::stat_t stat;
            userspace_fd_t fd;
            zeromem(&fd);
            zeromem(&stat);

            memcpy(fd.path,(char*)((uint64_t)base + current_head->p_offset),strlen((char*)((uint64_t)base + current_head->p_offset)));

            int status = vfs::vfs::stat(&fd,&stat);
            if(status) {
                return {0,0,0,0,0};
            }

            char* inter = (char*)memory::pmm::_virtual::alloc(stat.st_size);

            vfs::vfs::read(&fd,inter,stat.st_size);
            elfloadresult_t interpload = __scheduling_load_elf(proc,(std::uint8_t*)inter);
            memory::pmm::_virtual::free(inter);

            elfload.interp_entry = interpload.real_entry;

        }


        if (current_head->p_type == PT_LOAD ) {
            end = current_head->p_vaddr - elf_base + current_head->p_memsz;
            size = MAX2(size,end);
        }

        
    }

    uint8_t* elf = (uint8_t*)elf_base;

    void* elf_vmm;

    if(head->e_type != ET_DYN) {
        elf_vmm = memory::vmm::customalloc(proc,elf_base,size,PTE_PRESENT | PTE_RW | PTE_USER);
    } else {
        elf_vmm = memory::vmm::alloc(proc,size,PTE_PRESENT | PTE_RW | PTE_USER);

        if(phdr) {
            phdr += (uint64_t)elf_vmm;
            elfload.phdr = phdr;
        }
        
        elfload.real_entry = (uint64_t)elf_vmm + head->e_entry;
    }

    uint8_t* allocated_elf = (uint8_t*)memory::vmm::get(proc,(uint64_t)elf_vmm)->phys;

    uint64_t phys_elf = (uint64_t)allocated_elf;

    elf_base = (uint64_t)elf_vmm;

    allocated_elf = (uint8_t*)Other::toVirt((uint64_t)allocated_elf);

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t dest = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        if(current_head->p_type == PT_LOAD) {
            
            if(head->e_type != ET_DYN)
                dest = (uint64_t)allocated_elf + (current_head->p_vaddr - elf_base);
            else
                dest = (uint64_t)allocated_elf + current_head->p_vaddr;

            memset((void*)dest,0,current_head->p_filesz);
            memcpy((void*)dest,(void*)((uint64_t)base + current_head->p_offset), current_head->p_filesz);
        }

    }
    
    return elfload;
}

void arch::x86_64::scheduling::loadelf(process_t* proc,char* path,char** argv,char** envp) {
    vfs::stat_t stat;
    userspace_fd_t fd;
    zeromem(&fd);
    zeromem(&stat);

    memcpy(fd.path,path,strlen(path));

    int status = vfs::vfs::stat(&fd,&stat);
    if(status) {
        return;
    }

    char* inter = (char*)memory::pmm::_virtual::alloc(stat.st_size);

    vfs::vfs::read(&fd,inter,stat.st_size);
    elfloadresult_t elfload = __scheduling_load_elf(proc,(std::uint8_t*)inter);
    memory::pmm::_virtual::free(inter);

    proc->ctx.rsp = (std::uint64_t)memory::vmm::alloc(proc,USERSPACE_STACK_SIZE,PTE_PRESENT | PTE_USER | PTE_RW) + USERSPACE_STACK_SIZE - 4096;
    std::uint64_t* _stack = (std::uint64_t*)proc->ctx.rsp;

    memory::vmm::reload(proc);

    std::uint64_t auxv_stack[] = {(std::uint64_t)elfload.real_entry,AT_ENTRY,elfload.phdr,AT_PHDR,elfload.phentsize,AT_PHENT,elfload.phnum,AT_PHNUM,4096,AT_PAGESZ};
    std::uint64_t argv_length = __elf_get_length(argv);
    std::uint64_t envp_length = __elf_get_length(envp);

    memory::paging::enablepaging(proc->ctx.cr3);

    char** stack_argv = (char**)memory::heap::malloc(8 * (argv_length + 1));
    char** stack_envp = (char**)memory::heap::malloc(8 * (envp_length + 1));

    memset(stack_argv,0,8 * (argv_length + 1));
    memset(stack_envp,0,8 * (envp_length + 1));

    _stack = __elf_copy_to_stack(argv,_stack,stack_argv,argv_length);

    for(int i = 0; i < argv_length / 2; ++i) {
        char* tmp = stack_argv[i];
        stack_argv[i] = stack_argv[argv_length - 1 - i];
        stack_argv[argv_length - 1 - i] = tmp;
    }

    _stack = __elf_copy_to_stack(envp,_stack,stack_envp,envp_length);

    PUT_STACK(_stack,0);

    _stack = __elf_copy_to_stack_without_out(auxv_stack,_stack,sizeof(auxv_stack) / 8);
    PUT_STACK(_stack,0);

    _stack = __elf_copy_to_stack_without_out((uint64_t*)stack_envp,_stack,envp_length);
    PUT_STACK(_stack,0);

    _stack = __elf_copy_to_stack_without_out((uint64_t*)stack_argv,_stack,argv_length);
    PUT_STACK(_stack,argv_length);

    memory::paging::enablekernel();

    proc->ctx.rsp = (std::uint64_t)_stack;
    proc->ctx.rip = elfload.interp_entry;

    memory::heap::free(stack_argv);
    memory::heap::free(stack_envp);

    memory::vmm::reload(proc);

}

void arch::x86_64::scheduling::wakeup(process_t* proc) {
    proc->lock.unlock(); /* Just clear */
}

arch::x86_64::process_t* arch::x86_64::scheduling::fork(process_t* proc) {
    process_t* nproc = create();
    memory::vmm::clone(nproc,proc);
    memory::vmm::reload(nproc);
    memcpy(&nproc->ctx,&proc->ctx,sizeof(int_frame_t));
    nproc->parent_id = proc->id;
    userspace_fd_t* fd = proc->fd;
    memcpy(nproc->fd,proc->fd,sizeof(userspace_fd_t));
    nproc->fd = (userspace_fd_t*)memory::pmm::_virtual::alloc(4096);
    while(fd) {
        userspace_fd_t* newfd = (userspace_fd_t*)memory::pmm::_virtual::alloc(4096);
        memcpy(newfd,fd,sizeof(userspace_fd_t));
        
        if(newfd->state == USERSPACE_FD_STATE_PIPE) {
            newfd->pipe->create(newfd->pipe_side);
        }

        newfd->next = nproc->fd;
        nproc->fd = newfd;
        fd = fd->next;
    }
    return nproc;
}

void arch::x86_64::scheduling::kill(process_t* proc) {
    proc->kill_lock.nowaitlock();
    proc->lock.lock();
    proc->status = PROCESS_STATE_KILLED;
    memory::pmm::_virtual::free(proc->cwd);
    memory::pmm::_virtual::free(proc->name);
    memory::pmm::_virtual::free(proc->sse_ctx);
}

arch::x86_64::process_t* arch::x86_64::scheduling::create() {
    process_lock->lock();
    process_t* proc = (process_t*)memory::pmm::_virtual::alloc(4096);

    proc->cwd = (char*)memory::pmm::_virtual::alloc(4096);
    proc->name = (char*)memory::pmm::_virtual::alloc(4096);
    proc->sse_ctx = (char*)memory::pmm::_virtual::alloc(arch::x86_64::cpu::sse::size());

    proc->fd_ptr = 3;

    proc->syscall_stack = (std::uint64_t)memory::pmm::_virtual::alloc(SYSCALL_STACK_SIZE);

    fpu_head_t* head = (fpu_head_t*)proc->sse_ctx;
    head->dumb5 = 0b0001111110000000;

    proc->ctx.cs = 0x20 | 3;
    proc->ctx.ss = 0x18 | 3;
    proc->ctx.rflags = (1 << 9);
    proc->status = PROCESS_STATE_RUNNING;
    proc->lock.nowaitlock();
    proc->kill_lock.unlock();

    proc->next = head_proc;
    head_proc = proc;

    proc->id = id_ptr++;

    memory::vmm::initproc(proc);
    memory::vmm::reload(proc);

    process_lock->unlock();
    return proc;
}

void arch::x86_64::scheduling::futexwake(process_t* proc, int* lock) {
    process_lock->lock();
    process_t* proc0 = head_proc;
    while(proc0) {
        if(proc0->parent_id == proc->id && proc0->futex == (std::uint64_t)lock) {
            proc->futex = 0;
            proc->lock.unlock();
        }
        proc0 = proc->next;
    }
    process_lock->unlock();
}

void arch::x86_64::scheduling::futexwait(process_t* proc, int* lock, int val) {
    int lock_val = *lock;
    process_lock->lock();
    if(lock_val == val) {
        proc->futex = (std::uint64_t)lock;
        proc->lock.nowaitlock();
    }
    process_lock->unlock();
}

extern "C" void schedulingSchedule(int_frame_t* ctx) {
    memory::paging::enablekernel();

    arch::x86_64::process_t* current = arch::x86_64::cpu::data()->temp.proc;    

    /* Actually context saving doesn't need some locking cuz there's already lock in process struct */

    if(ctx) {
        if(current) {
            current->ctx = *ctx;
            current->fs_base = __rdmsr(0xC0000100);
            arch::x86_64::cpu::sse::save((std::uint8_t*)current->sse_ctx);
            current->user_stack = arch::x86_64::cpu::data()->user_stack; /* Only user stack should be saved */
            arch::x86_64::cpu::data()->kernel_stack = 0;
            arch::x86_64::cpu::data()->temp.proc = 0;
            current->lock.unlock();
            process_lock->lock();
            current = current->next; /* DANGER !!! */
            process_lock->unlock();
        }
    }

    process_lock->lock();

    while (true) {
        while (current) {
            if (!current->kill_lock.test() && !current->lock.test_and_set() && current->id != 0) {
                memcpy(ctx, &current->ctx, sizeof(int_frame_t));

                __wrmsr(0xC0000100, current->fs_base);
                arch::x86_64::cpu::sse::load((std::uint8_t*)current->sse_ctx);

                if(ctx->cs & 3)
                    ctx->ss |= 3;
                    
                if(ctx->ss & 3)
                    ctx->cs |= 3;

                if(ctx->cs == 0x20)
                    ctx->cs |= 3;
                    
                if(ctx->ss == 0x18)
                    ctx->ss |= 3;

                arch::x86_64::cpu::data()->kernel_stack = current->syscall_stack + SYSCALL_STACK_SIZE;
                arch::x86_64::cpu::data()->user_stack = current->user_stack;
                arch::x86_64::cpu::data()->temp.proc = current;

                arch::x86_64::cpu::lapic::eoi();
                process_lock->unlock();
                schedulingEnd(ctx);
                __builtin_unreachable();
            }
            current = current->next;
        }
        process_lock->unlock();
        asm volatile("pause");
        process_lock->lock();
        current = head_proc;
    }
}
    

void arch::x86_64::scheduling::init() {
    process_lock = new locks::spinlock;
    process_t* main_proc = create();
    main_proc->kill_lock.nowaitlock();
    process_lock->unlock();
    arch::x86_64::interrupts::idt::set_entry((std::uint64_t)schedulingEnter,32,0x8E,2);
}