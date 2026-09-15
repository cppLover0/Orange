#pragma once
#include <klibc/string.hpp>
#include <generic/hhdm.hpp>
#include <generic/scheduling.hpp>

inline static bool is_safe_to_rw(thread* proc, std::uint64_t mem, std::uint64_t len) {
    (void)proc;
    if(mem + len >= etc::hhdm())
        return false;
    return true;
}

inline static int safe_strlen(char* str, int max_len) {
    int len = 0;
    while (str[len] != '\0') {
        if(len > max_len)
            return max_len;
        len++;
    }
    return len;
}

inline static bool fix_userspace_memory(thread* proc, std::uint64_t mem, std::uint64_t len) {
    if(mem + len >= etc::hhdm())
        return false;
    for(std::uint64_t addr = ALIGNPAGEDOWN(mem); addr < mem + len; addr += PAGE_SIZE) {
        vmm_obj* obj = proc->vmem->nlgetlen(addr);
        if(obj == nullptr)
            return false;                     
        if(obj->mmap_info.copied_file_desc != nullptr)
            proc->vmem->inv_lazy_file_alloc_4kb(obj, addr);   
        else
            proc->vmem->inv_lazy_alloc(addr, PAGE_SIZE);      
    }
    arch::tlb_flush(mem, ALIGNPAGEUP(len));
    return true;
}