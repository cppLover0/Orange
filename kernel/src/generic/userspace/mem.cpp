#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/safety.hpp>

long long sys_brk(std::uint64_t brk) {
    (void)brk;
    return 0;
}

long long sys_mmap(std::uint64_t hint, std::uint64_t len, std::uint64_t prot, std::uint64_t flags, int fd, std::uint64_t off) {
    (void)prot;

    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)hint, len)) {
        return -EFAULT;
    }

    file_descriptor* file = nullptr;
    auto manager = (vfs::fdmanager*)current->fd;

    if(!(flags & MAP_ANONYMOUS)) {
        file = manager->search(fd);
        if(file == nullptr)
            return -EBADF;

        if(file->type != file_descriptor_type::file)
            return -EINVAL;
    }

    std::uint64_t allocated = 0;

    if(flags & MAP_FIXED) {
        allocated = current->vmem->alloc_memory(hint, len, true);
    } else {
        allocated = current->vmem->alloc_memory(0, len, false);
    }

    arch::tlb_flush(allocated, len);

    if(!(flags & MAP_ANONYMOUS)) {
        std::uint64_t old = file->offset;
        file->offset = off;
        file->vnode.read(file, (void*)allocated, len);
        file->offset = old;
    }

    return allocated;
}

long long sys_set_tid_address(int* tidptr) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)tidptr, 4096)) {
        return -EFAULT;
    }
    current->tidptr = tidptr;
    return current->id;
}

long long sys_munmap(std::uint64_t addr, std::uint64_t len) {
    return 0;
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)addr, len + PAGE_SIZE)) {
        return -EFAULT;
    }

    current->vmem->unmap(ALIGNPAGEDOWN(addr), ALIGNPAGEUP(len));
    return 0;
}