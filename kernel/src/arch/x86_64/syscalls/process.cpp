
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <etc/errno.hpp>

syscall_ret_t sys_tcb_set(std::uint64_t fs) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    __wrmsr(0xC0000100,fs);
    return {0,0,0};
}

syscall_ret_t sys_libc_log(const char* msg) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    char buffer[2048];
    copy_in_userspace_string(proc,buffer,(void*)msg,2048);
    Log::Display(LEVEL_MESSAGE_INFO,"libc_log from proc %d: %s\n",proc->id,buffer);
    return {0,0,0};
}

syscall_ret_t sys_exit(int status) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->exit_code = status;
    arch::x86_64::scheduling::kill(proc);
    schedulingSchedule(0);
    __builtin_unreachable();
}

syscall_ret_t sys_mmap(std::uint64_t hint, std::uint64_t size, int fd0, int_frame_t* ctx) {
    std::uint64_t flags = ctx->r8;
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(flags & MAP_ANONYMOUS) {
        std::uint64_t new_hint = hint;
        if(!new_hint)
            new_hint = (std::uint64_t)memory::vmm::alloc(proc,size,PTE_PRESENT | PTE_USER | PTE_RW);
        else 
            memory::vmm::customalloc(proc,new_hint,size,PTE_PRESENT | PTE_RW | PTE_USER);
        return {1,0,(std::int64_t)new_hint};
    } else {
        std::uint64_t mmap_base = 0;
        std::uint64_t mmap_size = 0;
        userspace_fd_t* fd = vfs::fdmanager::search(proc,fd0);
        if(!fd)
            return {1,EBADF,0};

        int status = vfs::vfs::mmap(fd,&mmap_base,&mmap_size);
        if(status != 0)
            return {1,status,0};

        std::uint64_t new_hint_hint = (std::uint64_t)memory::vmm::map(proc,mmap_base,mmap_size,PTE_PRESENT | PTE_USER | PTE_RW);
        return {1,0,(std::int64_t)new_hint_hint};
    }
}

syscall_ret_t sys_free(void *pointer, size_t size) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    std::uint64_t phys = memory::vmm::get(proc,(std::uint64_t)pointer)->phys;
    if(!memory::vmm::get(proc,(std::uint64_t)pointer)->is_mapped)
        memory::pmm::_physical::free(phys);
    memory::vmm::get(proc,(std::uint64_t)pointer)->phys = 0;
    return {0,0,0};
}