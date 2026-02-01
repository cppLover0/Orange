
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

#include <generic/vfs/fd.hpp>

#include <etc/libc.hpp>
#include <etc/list.hpp>

#include <generic/time.hpp>

#include <etc/logging.hpp>

#include <atomic>

#include <etc/errno.hpp>

#include <config.hpp>

arch::x86_64::process_t* head_proc;
std::uint32_t id_ptr = 0;

locks::spinlock process_lock; 

#define PUT_STACK(dest,value) *--dest = value;
#define PUT_STACK_STRING(dest,string) memcpy(dest,string,strlen(string)); 
    

uint64_t __elf_get_length(char** arr) {
    uint64_t counter = 0;

    while(arr[counter]) {
        counter++;
        char* t = (char*)(arr[counter - 1]);
        if(*t == '\0')
            return counter - 1; // invalid
    } 

    return counter;
}

uint64_t __stack_memcpy(std::uint64_t stack, void* src, uint64_t len) {
    std::uint64_t _stack = stack;
    _stack -= 8;
    _stack -= ALIGNUP(len,8);
    memcpy((void*)_stack,src,len);
    return (std::uint64_t)_stack;
}

uint64_t* __elf_copy_to_stack(char** arr,uint64_t* stack,char** out, uint64_t len) {

    uint64_t* temp_stack = stack;

    for(uint64_t i = 0;i < len; i++) {
        temp_stack -= ALIGNUP(strlen(arr[i]),8);
        PUT_STACK_STRING(temp_stack,arr[i])
        out[i] = (char*)temp_stack;
        PUT_STACK(temp_stack,0);
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
    if(strncmp((const char*)head->e_ident,ELF,4)) {
        return {0,0,0,0,0,ENOENT};
    }

    elfloadresult_t elfload;
    elfload.real_entry = head->e_entry;
    elfload.interp_entry = head->e_entry;
    elfload.phnum = head->e_phnum;
    elfload.phentsize = head->e_phentsize;

    if(head->e_type != ET_DYN) {
        for(int i = 0; i < head->e_phnum; i++) {
            current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
            if (current_head->p_type == PT_LOAD) {
                elf_base = MIN2(elf_base, ALIGNDOWN(current_head->p_vaddr, PAGE_SIZE));
            }
        }
    }

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t start = 0, end = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);

        if(current_head->p_type == PT_PHDR) {
            phdr = current_head->p_vaddr;
            elfload.phdr = phdr;
        } 

        if (current_head->p_type == PT_LOAD) {
            if(head->e_type != ET_DYN) {
                start = ALIGNDOWN(current_head->p_vaddr, PAGE_SIZE);
                end = ALIGNUP(current_head->p_vaddr + current_head->p_memsz, PAGE_SIZE);
                size = MAX2(size, end - elf_base);
            } else {
                end = current_head->p_vaddr - elf_base + current_head->p_memsz;
                size = MAX2(size, end);
            }
        }
    }

    void* elf_vmm;

    if(head->e_type != ET_DYN) {
        elf_vmm = memory::vmm::customalloc(proc, elf_base, size, PTE_PRESENT | PTE_RW | PTE_USER, 0, 0);
    } else {
        elf_vmm = memory::vmm::alloc(proc, size, PTE_PRESENT | PTE_RW | PTE_USER, 0);
    }

    elfload.base = (uint64_t)elf_vmm;

    uint8_t* allocated_elf = (uint8_t*)memory::vmm::get(proc,(uint64_t)elf_vmm)->phys;
    uint64_t phys_elf = (uint64_t)allocated_elf;
    allocated_elf = (uint8_t*)Other::toVirt((uint64_t)allocated_elf);

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t dest = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        
        if(current_head->p_type == PT_LOAD) {
            if(head->e_type != ET_DYN) {
                dest = (uint64_t)allocated_elf + (current_head->p_vaddr - elf_base);
            } else {
                dest = (uint64_t)allocated_elf + current_head->p_vaddr;
            }

            memset((void*)dest, 0, current_head->p_memsz);
            memcpy((void*)dest, (void*)((uint64_t)base + current_head->p_offset), current_head->p_filesz);
        } else if(current_head->p_type == PT_INTERP) {
            vfs::stat_t stat;
            userspace_fd_t fd;
            zeromem(&fd);
            zeromem(&stat);

            memcpy(fd.path, (char*)((uint64_t)base + current_head->p_offset), strlen((char*)((uint64_t)base + current_head->p_offset)));
            int status = vfs::vfs::stat(&fd, &stat);
            if(status) {
                return {0,0,0,0,0,ENOENT};
            }

            char* inter = (char*)memory::pmm::_virtual::alloc(stat.st_size);
            vfs::vfs::read(&fd, inter, stat.st_size);
            elfloadresult_t interpload = __scheduling_load_elf(proc, (std::uint8_t*)inter);
            memory::pmm::_virtual::free(inter);

            elfload.interp_base = interpload.base;
            elfload.interp_entry = interpload.real_entry;
        }
    }

    if(head->e_type != ET_DYN) {
        if(phdr) {
            elfload.phdr = phdr;
            DEBUG(1,"phdr 0x%p",phdr);
        }
    } else {
        if(phdr) {
            phdr += (uint64_t)elf_vmm;
            elfload.phdr = phdr;
        }
        elfload.real_entry = (uint64_t)elf_vmm + head->e_entry;
    }

    elfload.status = 0;
    return elfload;
}

std::atomic<std::uint64_t> znext = 0;
std::uint64_t __zrand() {
    uint64_t t = __rdtsc();
    return t * (++znext);
}

int arch::x86_64::scheduling::loadelf(process_t* proc,char* path,char** argv,char** envp, int free_mem) {
    vfs::stat_t stat;
    userspace_fd_t fd;
    zeromem(&fd);
    zeromem(&stat);

    memcpy(fd.path,path,strlen(path));

    memcpy(proc->name,path,strlen(path) + 1);

    int status = vfs::vfs::stat(&fd,&stat);
    if(status) {
        return status;
    }

    char* inter = (char*)memory::pmm::_virtual::alloc(stat.st_size);

    vfs::vfs::read(&fd,inter,stat.st_size);
    elfloadresult_t elfload = __scheduling_load_elf(proc,(std::uint8_t*)inter);

    memory::pmm::_virtual::free(inter);
    if(elfload.status != 0)
        return elfload.status;

    proc->ctx.rsp = (std::uint64_t)memory::vmm::alloc(proc,USERSPACE_STACK_SIZE,PTE_PRESENT | PTE_USER | PTE_RW,0) + USERSPACE_STACK_SIZE - 4096;
    std::uint64_t* _stack = (std::uint64_t*)proc->ctx.rsp;

    std::uint64_t random_data_aux = (std::uint64_t)memory::vmm::alloc(proc,4096,PTE_PRESENT | PTE_USER | PTE_RW,0);

    std::uint64_t auxv_stack[] = {(std::uint64_t)elfload.real_entry,AT_ENTRY,elfload.phdr,AT_PHDR,elfload.phentsize,AT_PHENT,elfload.phnum,AT_PHNUM,4096,AT_PAGESZ,0,AT_SECURE,random_data_aux,AT_RANDOM,4096,51,elfload.interp_base,AT_BASE};
    std::uint64_t argv_length = __elf_get_length(argv);
    std::uint64_t envp_length = __elf_get_length(envp);

    memory::paging::enablepaging(proc->ctx.cr3);

    std::uint64_t* randaux = (std::uint64_t*)random_data_aux;
    randaux[0] = __zrand();
    randaux[1] = __zrand(); 

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

    return 0;
}

void arch::x86_64::scheduling::wakeup(process_t* proc) {
    proc->lock.unlock(); /* Just clear */
}

arch::x86_64::process_t* arch::x86_64::scheduling::by_pid(int pid) {
    process_t* proc = head_proc_();
    while(proc) {
        if(proc->id == pid) { 
            return proc; }
        proc = proc->next;
    }
    return 0;
}

arch::x86_64::process_t* arch::x86_64::scheduling::clone3(process_t* proc, clone_args* clarg, int_frame_t* ctx) {
    process_t* nproc = create();

    memcpy(&nproc->ctx,ctx,sizeof(int_frame_t));

    if(clarg->flags & CLONE_VM) {
        memory::vmm::free(nproc);
        nproc->ctx.cr3 = ctx->cr3;
        nproc->original_cr3 = ctx->cr3;
        nproc->vmm_end = proc->vmm_end;
        nproc->vmm_start = proc->vmm_start;
        nproc->reversedforid = *proc->vmm_id;
        nproc->vmm_id = &nproc->reversedforid;

        vmm_obj_t* start = (vmm_obj_t*)proc->vmm_start;
        start->lock.lock();
        start->pthread_count++;
        start->lock.unlock();

    } else {
        memory::vmm::clone(nproc,proc);
    }

    nproc->is_cloned = 1;
    nproc->uid = proc->uid;

    if(clarg->flags & CLONE_PARENT) {
        nproc->parent_id = proc->parent_id;
    } else {
        nproc->parent_id = proc->id;
    }

    if(clarg->flags & CLONE_THREAD) {
        nproc->thread_group = proc->thread_group;
    } else {
        nproc->thread_group = nproc->id;
    }
 
    if(clarg->tls) {
        nproc->fs_base = clarg->tls;
    } else {
        nproc->fs_base = proc->fs_base;
    }

    nproc->exit_signal = clarg->exit_signal;
    
    if(clarg->stack) {
        nproc->ctx.rsp = clarg->stack;
    }

    sigset_list* sigsetlist = proc->sigset_list_obj;
    while(sigsetlist) {
        sigset_list* newsigsetlist = (sigset_list*)memory::pmm::_virtual::alloc(sizeof(sigset_list));
        memcpy(&newsigsetlist->sigset,&sigsetlist->sigset,sizeof(sigset_t));
        newsigsetlist->sig = sigsetlist->sig;

        newsigsetlist->next = nproc->sigset_list_obj;
        nproc->sigset_list_obj = newsigsetlist;

        sigsetlist = sigsetlist->next;
    }

    memcpy(&nproc->current_sigset,&proc->current_sigset,sizeof(proc->current_sigset));
    memcpy(nproc->ret_handlers,proc->ret_handlers,sizeof(proc->ret_handlers));
    memcpy(nproc->sig_handlers,proc->sig_handlers,sizeof(proc->sig_handlers));

    memset(nproc->cwd,0,4096);
    memset(nproc->name,0,4096);
    memcpy(nproc->cwd,proc->cwd,strlen(proc->cwd));
    memcpy(nproc->name,proc->name,strlen(proc->name));

    memcpy(nproc->sse_ctx,proc->sse_ctx,arch::x86_64::cpu::sse::size());

    if(clarg->flags & CLONE_FILES) {
        nproc->is_shared_fd = 1;
        nproc->fd_ptr = proc->fd_ptr;
        nproc->fd = proc->fd;

        vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;

        proc->fd_lock.lock();
        fd->used_counter++;
        proc->fd_lock.unlock();
    } else {
        *nproc->fd_ptr = *proc->fd_ptr;
        nproc->fd_ptr = &nproc->alloc_fd;

        vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;
        fd->duplicate((vfs::fdmanager*)nproc->fd);
    }
    nproc->ctx.cr3 = nproc->original_cr3;
    return nproc;
}

arch::x86_64::process_t* arch::x86_64::scheduling::clone(process_t* proc,int_frame_t* ctx) {
    process_t* nproc = create();
    memory::vmm::free(nproc);

    nproc->is_cloned = 1;
    nproc->ctx.cr3 = ctx->cr3;
    nproc->original_cr3 = ctx->cr3;
    nproc->uid = proc->uid;

    nproc->vmm_end = proc->vmm_end;
    nproc->vmm_start = proc->vmm_start;

    nproc->parent_id = proc->id;
    nproc->fs_base = proc->fs_base;
    nproc->reversedforid = *proc->vmm_id;
    nproc->vmm_id = &nproc->reversedforid;

    sigset_list* sigsetlist = proc->sigset_list_obj;
    while(sigsetlist) {
        sigset_list* newsigsetlist = (sigset_list*)memory::pmm::_virtual::alloc(sizeof(sigset_list));
        memcpy(&newsigsetlist->sigset,&sigsetlist->sigset,sizeof(sigset_t));
        newsigsetlist->sig = sigsetlist->sig;

        newsigsetlist->next = nproc->sigset_list_obj;
        nproc->sigset_list_obj = newsigsetlist;

        sigsetlist = sigsetlist->next;
    }

    memcpy(&nproc->current_sigset,&proc->current_sigset,sizeof(proc->current_sigset));
    memcpy(nproc->ret_handlers,proc->ret_handlers,sizeof(proc->ret_handlers));
    memcpy(nproc->sig_handlers,proc->sig_handlers,sizeof(proc->sig_handlers));

    memset(nproc->cwd,0,4096);
    memset(nproc->name,0,4096);
    memcpy(nproc->cwd,proc->cwd,strlen(proc->cwd));
    memcpy(nproc->name,proc->name,strlen(proc->name));

    memcpy(nproc->sse_ctx,proc->sse_ctx,arch::x86_64::cpu::sse::size());

    nproc->is_shared_fd = 1;
    nproc->fd_ptr = proc->fd_ptr;
    nproc->fd = proc->fd;

    vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;

    proc->fd_lock.lock();
    fd->used_counter++;
    proc->fd_lock.unlock();

    // userspace_fd_t* fd = proc->fd;
    // while(fd) {
    //     userspace_fd_t* newfd = new userspace_fd_t;
    //     memcpy(newfd,fd,sizeof(userspace_fd_t));
        
    //     if(newfd->state == USERSPACE_FD_STATE_PIPE) {
    //         newfd->pipe->create(newfd->pipe_side);
    //     } else if(newfd->state == USERSPACE_FD_STATE_EVENTFD)
    //         newfd->eventfd->create();

    //     newfd->next = nproc->fd;
    //     nproc->fd = newfd;
    //     fd = fd->next;
    // }

    memcpy(&nproc->ctx,ctx,sizeof(int_frame_t));

    return nproc;
}

arch::x86_64::process_t* arch::x86_64::scheduling::fork(process_t* proc,int_frame_t* ctx) {
    process_t* nproc = create();
    memory::vmm::clone(nproc,proc);

    nproc->parent_id = proc->id;
    nproc->fs_base = proc->fs_base;
    nproc->uid = proc->uid;

    

    sigset_list* sigsetlist = proc->sigset_list_obj;
    while(sigsetlist) {
        sigset_list* newsigsetlist = (sigset_list*)memory::pmm::_virtual::alloc(sizeof(sigset_list));
        memcpy(&newsigsetlist->sigset,&sigsetlist->sigset,sizeof(sigset_t));
        newsigsetlist->sig = sigsetlist->sig;

        newsigsetlist->next = nproc->sigset_list_obj;
        nproc->sigset_list_obj = newsigsetlist;

        sigsetlist = sigsetlist->next;
    }

    memcpy(&nproc->current_sigset,&proc->current_sigset,sizeof(proc->current_sigset));
    memcpy(nproc->ret_handlers,proc->ret_handlers,sizeof(proc->ret_handlers));
    memcpy(nproc->sig_handlers,proc->sig_handlers,sizeof(proc->sig_handlers));

    memset(nproc->cwd,0,4096);
    memset(nproc->name,0,4096);
    memcpy(nproc->cwd,proc->cwd,strlen(proc->cwd));
    memcpy(nproc->name,proc->name,strlen(proc->name));

    memcpy(nproc->sse_ctx,proc->sse_ctx,arch::x86_64::cpu::sse::size());

    *nproc->fd_ptr = *proc->fd_ptr;
    nproc->fd_ptr = &nproc->alloc_fd;

    vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;
    fd->duplicate((vfs::fdmanager*)nproc->fd);
    
    memcpy(&nproc->ctx,ctx,sizeof(int_frame_t));
    nproc->ctx.cr3 = nproc->original_cr3;

    return nproc;
}
void arch::x86_64::scheduling::kill(process_t* proc) {
    proc->kill_lock.nowaitlock();
    proc->lock.nowaitlock();
    proc->status = PROCESS_STATE_ZOMBIE;
    proc->exit_timestamp = time::counter();
    free_sigset_from_list(proc);
    delete proc->pass_fd;
}

void __scheduling_balance_cpus() {
    int cpu_ptr = 0;
    arch::x86_64::process_t* proc = head_proc;
    extern int how_much_cpus;
    while(proc) {
        if(!proc->kill_lock.test()) {
            proc->target_cpu.store(cpu_ptr++,std::memory_order_release);
            //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"cpu %d to proc %s (%d) cpu_count %d\n",cpu_ptr,proc->name,proc->id,how_much_cpus);
            if(cpu_ptr == how_much_cpus) 
                cpu_ptr = 0;
        }
        proc = proc->next;
    }
}

arch::x86_64::process_t* arch::x86_64::scheduling::create() {
    process_t* proc = (process_t*)memory::pmm::_virtual::alloc(4096);

    proc->cwd = (char*)memory::pmm::_virtual::alloc(4096);
    proc->name = (char*)memory::pmm::_virtual::alloc(4096);
    proc->sse_ctx = (char*)memory::pmm::_virtual::alloc(arch::x86_64::cpu::sse::size());

    proc->syscall_stack = (std::uint64_t)memory::pmm::_virtual::alloc(SYSCALL_STACK_SIZE);

    fpu_head_t* head = (fpu_head_t*)proc->sse_ctx;
    head->dumb5 = 0b0001111110000000;

    proc->ctx.cs = 0x20 | 3;
    proc->ctx.ss = 0x18 | 3;
    proc->ctx.rflags = (1 << 9);
    proc->status = PROCESS_STATE_RUNNING;
    proc->lock.nowaitlock();
    proc->kill_lock.unlock();
    proc->fd_ptr = &proc->alloc_fd;

    proc->thread_group = proc->id;

    *proc->fd_ptr = 3;

    proc->is_debug = 0;
    proc->next_alarm = -1;
    proc->virt_timer = -1;
    proc->prof_timer = -1;

    proc->sig = new signalmanager;

    vfs::fdmanager* z = new vfs::fdmanager(proc);
    proc->fd = (void*)z;

    proc->next = head_proc;
    head_proc = proc;


    proc->id = id_ptr++;
    proc->vmm_id = &proc->id;

    proc->pass_fd = new vfs::passingfd_manager;

    memory::vmm::initproc(proc);
    memory::vmm::reload(proc);

    proc->create_timestamp = time::counter();

    __scheduling_balance_cpus();

    return proc;
}

locks::spinlock futex_lock;

int arch::x86_64::scheduling::futexwake(process_t* proc, int* lock, int num_to_wake) {
    process_t* proc0 = head_proc;
    futex_lock.lock();
    int nums = 0;
    while(proc0 && num_to_wake != 0) {
        // if process vmm is same then they are connected
        if(((proc->vmm_start == proc0->vmm_start)) && proc0->futex == (std::uint64_t)lock) {
            proc0->futex = 0;
            proc0->futex_lock.unlock();
            num_to_wake--;
            DEBUG(proc->is_debug,"futex_wakeup proccess %d from proc %d, futex 0x%p",proc0->id,proc->id,lock);
            nums++;
        } else if((proc0->futex == (std::uint64_t)lock || proc->futex == (std::uint64_t)lock)) {
            DEBUG(proc->is_debug,"same futex 0x%p but calling proc %d is not connected to proc %d",lock,proc->id,proc0->id);
        }
        proc0 = proc0->next;
    }
    futex_lock.unlock();
    return nums;
}

void arch::x86_64::scheduling::futexwait(process_t* proc, int* lock, int val, int* original_lock,std::uint64_t ts) {
    futex_lock.lock();
    int lock_val = *lock;
    proc->futex_lock.lock();
    proc->futex = (std::uint64_t)original_lock;
    proc->ts = ts;
    futex_lock.unlock();
}
int l = 0;


#define SIG_DFL	0
#define SIG_IGN	1

extern "C" void schedulingSchedule(int_frame_t* ctx) {
    extern int is_panic;

    if(ctx) {
        if(ctx->cs != 0x08)
            asm volatile("swapgs");
    }

    if(is_panic == 1)
        asm volatile("hlt");

scheduleagain:
    arch::x86_64::process_t* current = arch::x86_64::cpu::data()->temp.proc;    

    cpudata_t* data = arch::x86_64::cpu::data();

    if(ctx) {
        if(current) {
            if(!current->kill_lock.test() && !current->_3rd_kill_lock.test()) {
                current->ctx = *ctx;
                current->fs_base = __rdmsr(0xC0000100);
                arch::x86_64::cpu::sse::save((std::uint8_t*)current->sse_ctx);
                current->user_stack = data->user_stack; /* Only user stack should be saved */

                if(current->time_start) {
                    // it wants something 
                    std::uint64_t delta = drivers::tsc::currentus() - current->time_start;
                    if(current->virt_timer > 0) {
                        current->virt_timer -= current->virt_timer > delta ? delta : current->virt_timer;
                    } 

                    if(current->prof_timer > 0) {
                        current->prof_timer -= current->prof_timer > delta ? delta : current->prof_timer;
                    }

                    current->time_start = 0;

                }

            }
        }
    }

    if(current) { 
        if(current->_3rd_kill_lock.test() && !current->kill_lock.test()) {
            memory::paging::enablekernel();
            arch::x86_64::scheduling::kill(current);
            
            if(1)
                memory::vmm::free(current); 

            vfs::fdmanager* fd = (vfs::fdmanager*)current->fd;
            fd->free();

            memory::pmm::_virtual::free(current->cwd);
            memory::pmm::_virtual::free(current->name);
            memory::pmm::_virtual::free(current->sse_ctx);
        } else
            current->lock.unlock();
        
        current = current->next; 
    } 

    ctx = 0;

    int current_cpu = data->smp.cpu_id;

    while (true) {
        while (current) {

            if (!current->kill_lock.test() && current->id != 0 && 1) {

                if(1) { // signals are only for user mode, or they must be handled by process itself
                    if(!current->lock.test_and_set()) {

                        // real alarm timer
                        if(current->next_alarm != -1 && current->sig) {
                            if(drivers::tsc::currentus() >= current->next_alarm) {
                                // alarm !!!
                                pending_signal_t alarm_sig;
                                alarm_sig.sig = SIGALRM;
                                current->next_alarm = -1;
                                if(current->itimer.it_interval.tv_sec != 0 || current->itimer.it_interval.tv_usec != 0) {
                                    std::uint64_t offs = current->itimer.it_interval.tv_sec * 1000 * 1000;
                                    offs += current->itimer.it_interval.tv_usec;
                                    current->next_alarm = drivers::tsc::currentus() + offs;
                                }
                                current->sig->push(&alarm_sig);
                            }
                        }

                        if(current->virt_timer != -1 && current->sig) {
                            if(current->virt_timer == 0) {
                                // alarm !!!
                                pending_signal_t alarm_sig;
                                alarm_sig.sig = SIGVTALRM;
                                current->virt_timer = -1;
                                if(current->vitimer.it_interval.tv_sec != 0 || current->vitimer.it_interval.tv_usec != 0) {
                                    std::uint64_t offs = current->vitimer.it_interval.tv_sec * 1000 * 1000;
                                    offs += current->vitimer.it_interval.tv_usec;
                                    current->virt_timer = offs;
                                }
                                current->sig->push(&alarm_sig);
                            }
                        }

                        if(current->prof_timer != -1 && current->sig) {
                            if(current->prof_timer == 0) {
                                // alarm !!!
                                pending_signal_t alarm_sig;
                                alarm_sig.sig = SIGPROF;
                                current->prof_timer = -1;
                                if(current->proftimer.it_interval.tv_sec != 0 || current->proftimer.it_interval.tv_usec != 0) {
                                    std::uint64_t offs = current->proftimer.it_interval.tv_sec * 1000 * 1000;
                                    offs += current->proftimer.it_interval.tv_usec;
                                    current->prof_timer = offs;
                                }
                                current->sig->push(&alarm_sig);
                            }
                        }

                        if(!ctx) {
                            int_frame_t temp_ctx;
                            ctx = &temp_ctx;
                        }

                        int is_possible = 1;
                        if(current->ctx.cs == 0x08) {
                            is_possible = 0;
                        }

                        if(is_possible) {
                            pending_signal_t new_sig;
                            int status = current->sig->popsigset(&new_sig,&current->current_sigset);
                            if(status != -1) {

                                if((std::uint64_t)current->sig_handlers[new_sig.sig] == SIG_IGN || (std::uint64_t)current->sig_handlers[new_sig.sig] == SIG_DFL || new_sig.sig == SIGKILL) {
                                    // sig is blocked, push it back
                                    if(current->sig_handlers[new_sig.sig] == SIG_DFL) {
                                        switch(new_sig.sig) {
                                        case SIGCHLD:
                                        case SIGCONT:
                                        case SIGURG:
                                        case SIGWINCH:
                                            break; //ignore
                                        case SIGSTOP:
                                        case SIGTSTP:
                                        case SIGTTIN:
                                        case SIGTTOU:
                                            Log::SerialDisplay(LEVEL_MESSAGE_WARN,"unimplemented process stop to proc %d, sig %d\n",current->id,new_sig.sig);
                                            break;
                                        
                                        default: {
                                            char* vm_start = current->vmm_start;
                                            arch::x86_64::process_t* currentz = arch::x86_64::scheduling::head_proc_();
                                            memory::paging::enablekernel();
                                            while(currentz) {
                                                if(currentz->vmm_start == vm_start) {
                                                    currentz->exit_code = status;
                                                    arch::x86_64::scheduling::kill(currentz);
                                                    
                                                    if(1)
                                                        memory::vmm::free(currentz); 

                                                    vfs::fdmanager* fd = (vfs::fdmanager*)currentz->fd;
                                                    fd->free();

                                                    memory::pmm::_virtual::free(currentz->cwd);
                                                    memory::pmm::_virtual::free(currentz->name);
                                                    memory::pmm::_virtual::free(currentz->sse_ctx);
                                                }
                                                currentz = currentz->next;
                                            }    
                                            goto scheduleagain;
                                        }
                                        };
                                    }
                                    
                                } else {

                                    if(current->is_restore_sigset) {
                                        // syscall requested restore sigset
                                        memcpy(&current->current_sigset,&current->temp_sigset,sizeof(sigset_t));
                                        current->is_restore_sigset = 0;
                                    }

                                    sigtrace new_sigtrace;

                                    mcontext_t temp_mctx = {0};
                                    int_frame_to_mcontext(&current->ctx,&temp_mctx);

                                    ucontext_t temp_uctx = {0};
                                    memcpy(&temp_uctx.uc_mcontext,&temp_mctx,sizeof(mcontext_t));
                                    temp_uctx.uc_sigmask = current->current_sigset;
                                    temp_uctx.uc_stack.ss_size = USERSPACE_STACK_SIZE;
                                    temp_uctx.uc_stack.ss_flags = 0;
                                    temp_uctx.uc_stack.ss_sp = (void*)temp_mctx.gregs[REG_RSP];
                                    temp_uctx.uc_link = 0;

                                    memory::paging::enablepaging(current->original_cr3);

                                    std::uint64_t* new_stack = (std::uint64_t*)(current->altstack.ss_sp == 0 ? current->ctx.rsp - 8 : (std::uint64_t)current->altstack.ss_sp);
                                    
                                    new_stack = (std::uint64_t*)__stack_memcpy((std::uint64_t)new_stack,&temp_uctx,sizeof(ucontext_t));

                                    new_sigtrace.uctx = (ucontext_t*)new_stack;
                                    --new_stack;

                                    siginfo_t siginfo;
                                    memset(&siginfo,0,sizeof(siginfo));

                                    new_stack = (std::uint64_t*)__stack_memcpy((std::uint64_t)new_stack,&siginfo,sizeof(siginfo_t));
                                    std::uint64_t siginfoz = (std::uint64_t)new_stack;

                                    --new_stack;

                                    new_sigtrace.next = current->sigtrace_obj;

                                    new_stack = (std::uint64_t*)__stack_memcpy((std::uint64_t)new_stack,&new_sigtrace,sizeof(sigtrace));

                                    sigtrace* new_sigstrace_u = (sigtrace*)new_stack;

                                    new_stack = (std::uint64_t*)__stack_memcpy((std::uint64_t)new_stack,current->sse_ctx,arch::x86_64::cpu::sse::size());

                                    new_sigstrace_u->fpu_state = (void*)new_stack;
                                    current->sigtrace_obj = new_sigstrace_u;

                                    --new_stack;
                                    *--new_stack = (std::uint64_t)current->ret_handlers[new_sig.sig];

                                    // now setup new registers and stack

                                    current->ctx.rdi = (std::uint64_t)current->sig_handlers[new_sig.sig];
                                    current->ctx.rsi = (std::uint64_t)siginfoz;
                                    current->ctx.rdx = (std::uint64_t)new_sigtrace.uctx;
                                    current->ctx.rsp = (std::uint64_t)new_stack;
                                    current->ctx.rip = (std::uint64_t)current->sig_handlers[new_sig.sig];
                                    current->ctx.cs = 0x20 | 3;
                                    current->ctx.ss = 0x18 | 3;
                                    current->ctx.rflags = (1 << 9);

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

                                    data->kernel_stack = current->syscall_stack + SYSCALL_STACK_SIZE;
                                    data->user_stack = current->user_stack;
                                    data->temp.proc = current;

                                    int prio_div = current->prio < 0 ? (current->prio * -1) : 1;
                                    prio_div++;

                                    if(current->virt_timer != -1 || current->prof_timer != -1) {
                                        // it will request for time

                                        current->time_start = drivers::tsc::currentus();
                                    }

                                    arch::x86_64::cpu::lapic::eoi();

                                    if(ctx->cs != 0x08)
                                        asm volatile("swapgs");
                                    schedulingEnd(ctx);
                                }
                                
                            }
                        }

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

                        data->kernel_stack = current->syscall_stack + SYSCALL_STACK_SIZE;
                        data->user_stack = current->user_stack;
                        data->temp.proc = current;

                        int prio_div = current->prio < 0 ? (current->prio * -1) : 1;
                        prio_div++;

                        if(current->virt_timer != -1 || current->prof_timer != -1) {
                                        // it will request for time
                            current->time_start = drivers::tsc::currentus();
                        }


                        arch::x86_64::cpu::lapic::eoi();

                        if(ctx->cs != 0x08)
                            asm volatile("swapgs");

                        schedulingEnd(ctx);
                    }
                }
            }
            current = current->next;
        }
        current = head_proc;
    }
}

void arch::x86_64::scheduling::create_kernel_thread(void (*func)(void*), void* arg) {
    process_t* proc = (process_t*)memory::pmm::_virtual::alloc(4096);

    proc->cwd = (char*)memory::pmm::_virtual::alloc(4096);
    proc->name = (char*)memory::pmm::_virtual::alloc(4096);
    proc->sse_ctx = (char*)memory::pmm::_virtual::alloc(arch::x86_64::cpu::sse::size());

    proc->syscall_stack = (std::uint64_t)memory::pmm::_virtual::alloc(SYSCALL_STACK_SIZE);

    fpu_head_t* head = (fpu_head_t*)proc->sse_ctx;
    head->dumb5 = 0b0001111110000000;

    proc->ctx.cs = 0x08;
    proc->ctx.ss = 0;
    proc->ctx.rflags = (1 << 9);
    proc->ctx.rsp = proc->syscall_stack + (SYSCALL_STACK_SIZE - 4096);
    proc->ctx.rip = (std::uint64_t)func;
    proc->ctx.rdi = (std::uint64_t)arg;
    proc->ctx.cr3 = memory::paging::kernelget();
    proc->original_cr3 = memory::paging::kernelget();
    proc->status = PROCESS_STATE_RUNNING;
    proc->lock.nowaitlock();
    proc->kill_lock.unlock();

    proc->id = id_ptr++;

    proc->next = head_proc;
    head_proc = proc;

    proc->create_timestamp = time::counter();

    __scheduling_balance_cpus();

    wakeup(proc);

    return;
}

arch::x86_64::process_t* arch::x86_64::scheduling::head_proc_() {
    return head_proc;
}

void arch::x86_64::scheduling::init() {
    process_t* main_proc = create();
    main_proc->kill_lock.nowaitlock();
    arch::x86_64::interrupts::idt::set_entry((std::uint64_t)schedulingEnter,32,0x8E,2);
}

void arch::x86_64::scheduling::sigreturn(process_t* proc) { // pop values from stack
    sigtrace* st = proc->sigtrace_obj;
    memory::paging::enablepaging(proc->original_cr3);
    if(!st)
        return;
    mcontext_t* mctx = &st->uctx->uc_mcontext;
    proc->current_sigset = st->uctx->uc_sigmask;
    mcontext_to_int_frame(mctx,&proc->ctx);
    proc->ctx.cr3 = proc->original_cr3;
    proc->ctx.cs = 0x20 | 3;
    proc->ctx.ss = 0x18 | 3;
    proc->ctx.rflags |= (1 << 9);
    memcpy(proc->sse_ctx,st->fpu_state,arch::x86_64::cpu::sse::size());
    proc->sigtrace_obj = st->next;
    memory::paging::enablekernel();

    if(proc->ctx.cs != 0x08)
        asm volatile("swapgs");

    schedulingEnd(&proc->ctx);
    __builtin_unreachable();
}