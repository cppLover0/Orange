
#include <cstdint>

#pragma once

#include <arch/x86_64/scheduling.hpp>

#include <etc/libc.hpp>
#include <etc/etc.hpp>

#include <generic/mm/paging.hpp>
#include <generic/mm/pmm.hpp>

#include <config.hpp>

#include <etc/logging.hpp>

typedef struct vmm_obj {
    uint64_t base;
    uint64_t phys;
    uint64_t len;
    uint64_t flags;
    uint8_t is_mapped;

    uint64_t src_len;

    struct vmm_obj* next;
} __attribute__((packed)) vmm_obj_t;

namespace memory {
    class vmm {
    public:

        inline static void initproc(arch::x86_64::process_t* proc) {
            vmm_obj_t* start = new vmm_obj_t;
            vmm_obj_t* end = new vmm_obj_t;
            zeromem(start);
            zeromem(end);
            start->len = 4096;
            start->next = end;
            end->base = (std::uint64_t)Other::toVirt(0) - 4096;
            end->next = start;
            proc->vmm_start = (char*)start;
            proc->vmm_end = (char*)end;
        }
        
        inline static vmm_obj_t* v_alloc(vmm_obj_t* start,std::uint64_t len) {
            vmm_obj_t* current = start->next;
            vmm_obj_t* prev = start;
            uint64_t found = 0;

            uint64_t align_length = ALIGNUP(len,4096);
            
            while(current) {

                if(prev) {

                    uint64_t prev_end = (prev->base + prev->len);
                    if(current->base - prev_end >= align_length) {

                        vmm_obj_t* vmm_new = new vmm_obj_t;
                        vmm_new->base = prev_end;
                        vmm_new->len = align_length;
                        prev->next = vmm_new;
                        vmm_new->next = current;

                        vmm_new->is_mapped = 0;

                        return vmm_new;
                    }
                }

                prev = current;
                current = current->next;
            }

            return 0;
        }

        inline static vmm_obj_t* v_find(vmm_obj_t* start, std::uint64_t base, std::uint64_t len) {
            vmm_obj_t* current = start->next;
            vmm_obj_t* prev = start;
            uint64_t found = 0;

            uint64_t align_length = ALIGNUP(len,4096);
            
            vmm_obj_t* vmm_new = new vmm_obj_t;

            while(current) {

                if(prev) {

                    uint64_t prev_end = (prev->base + prev->len);
                    uint64_t current_end = base + len;

                    if(current->base == base) {
                        delete (void*)vmm_new;
                        vmm_new = current;
                        break;
                    }

                    if(current->base >= current_end && prev_end <= base) {

                        vmm_new->base = base;
                        vmm_new->len = align_length;
                        
                        prev->next = vmm_new;
                        vmm_new->next = current;

                        break;

                    } 
                }

                prev = current;
                current = current->next;
            }

            return vmm_new;
        }

        inline static void* map(arch::x86_64::process_t* proc, std::uint64_t base, std::uint64_t length, std::uint64_t flags) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* new_vmm = v_alloc(current,length);
            new_vmm->flags = flags;
            new_vmm->phys = base;
            new_vmm->src_len = length;
            new_vmm->is_mapped = 1;
            paging::maprangeid(proc->original_cr3,base,new_vmm->base,length,flags,proc->id);
            return (void*)new_vmm->base;
        }

        inline static void* mark(arch::x86_64::process_t* proc, std::uint64_t base, std::uint64_t phys, std::uint64_t length, std::uint64_t flags) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* new_vmm = v_find(current,base,length);
            new_vmm->flags = flags;
            new_vmm->phys = phys;
            new_vmm->src_len = length;
            new_vmm->base = base;
            new_vmm->is_mapped = 0;
            new_vmm->len = ALIGNUP(length,4096);
            return (void*)new_vmm->base;
        }

        inline static vmm_obj_t* get(arch::x86_64::process_t* proc, std::uint64_t base) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

            while(current) {

                if(current->base == base)
                    return current;

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }
        }

        inline static void clone(arch::x86_64::process_t* dest_proc, arch::x86_64::process_t* src_proc) {
            if(dest_proc && src_proc) {

                vmm_obj_t* src_current = (vmm_obj_t*)src_proc->vmm_start;

                while(src_current) {

                    uint64_t phys;

                    if(src_current->phys) {
                        if(src_current->src_len <= 4096 && !src_current->is_mapped)
                            phys = memory::pmm::_physical::alloc(4096);
                        else if(src_current->src_len > 4096 && !src_current->is_mapped)
                            phys = memory::pmm::_physical::alloc(src_current->src_len);
                        else if(src_current->is_mapped)
                            phys = src_current->phys;

                        if(src_current->phys && !src_current->is_mapped)
                            memcpy((void*)Other::toVirt(phys),(void*)Other::toVirt(src_current->phys),src_current->len);

                        mark(dest_proc,src_current->base,phys,src_current->len,src_current->flags);
                        get(dest_proc,src_current->base)->is_mapped = src_current->is_mapped;
                    }

                    if(src_current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                        break;

                    src_current = src_current->next;
                }

            }
        }

        inline static void reload(arch::x86_64::process_t* proc) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            if(proc->ctx.cr3 && proc->original_cr3) { /* We should free all paging memory */
                memory::pmm::_physical::fullfree(proc->id);
                memory::pmm::_physical::free(proc->original_cr3);
            }
            proc->ctx.cr3 = memory::pmm::_physical::alloc(4096);
            proc->original_cr3 = proc->ctx.cr3;
            std::uint64_t cr3 = proc->ctx.cr3;
            memory::paging::mapkernel(cr3,proc->id);
            memory::paging::alwaysmappedmap(cr3,proc->id);
            memory::paging::maprangeid(cr3,Other::toPhys(proc->syscall_stack),(std::uint64_t)proc->syscall_stack,SYSCALL_STACK_SIZE,PTE_USER | PTE_PRESENT | PTE_RW,proc->id);
            while(current) {

                if(current->phys) {
                    memory::paging::maprangeid(cr3,current->phys,current->base,current->len,current->flags,proc->id);
                }

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }
        }

        inline static void modify(arch::x86_64::process_t* proc, std::uint64_t dest_base, std::uint64_t new_phys) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            while (current)
            {

                if(current->base == dest_base) {

                    current->phys = new_phys;
                    memory::paging::maprangeid(proc->original_cr3,current->phys,current->base,current->len,current->flags,proc->id);

                    return;

                }

                current = current->next;
            }
        }

        inline void free(arch::x86_64::process_t* proc) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* next = current->next;

            while (current)
            {
                next = current->next;

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    next = 0;

                if(current->phys && !current->is_mapped) {
                    memory::pmm::_physical::free(current->phys);
                }

                delete current;

                if(!next)   
                    break;

                current = next;
            }

            current = new vmm_obj_t;

            memory::pmm::_physical::fullfree(proc->id);
        }

        inline static void* alloc(arch::x86_64::process_t* proc, std::uint64_t len, std::uint64_t flags) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* new_vmm = v_alloc(current,len);
            new_vmm->flags = flags;
            new_vmm->src_len = len;
            new_vmm->is_mapped = 0;
            std::uint64_t phys;
            if(len < 4096)
                phys = memory::pmm::_physical::alloc(4096);
            else if(len > 4096)
                phys = memory::pmm::_physical::alloc(len);
            new_vmm->phys = phys;
            paging::maprangeid(proc->original_cr3,new_vmm->phys,new_vmm->base,new_vmm->len,flags,proc->id);
            return (void*)new_vmm->base;
        }

        inline static void* customalloc(arch::x86_64::process_t* proc, std::uint64_t virt, std::uint64_t len, std::uint64_t flags) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            std::uint64_t phys;
            if(len < 4096)
                phys = memory::pmm::_physical::alloc(4096);
            else if(len > 4096)
                phys = memory::pmm::_physical::alloc(len);
            void* new_virt = mark(proc,virt,phys,len,flags);
            paging::maprangeid(proc->original_cr3,phys,(std::uint64_t)virt,ALIGNUP(len,4096),flags,proc->id);
            return (void*)virt;
        } 

    };
}