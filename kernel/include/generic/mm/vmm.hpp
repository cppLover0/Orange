
#include <cstdint>

#pragma once

#include <arch/x86_64/scheduling.hpp>

#include <etc/libc.hpp>
#include <etc/etc.hpp>

#include <generic/mm/paging.hpp>
#include <generic/mm/pmm.hpp>
#include <arch/x86_64/syscalls/shm.hpp>

#include <drivers/cmos.hpp>

#include <config.hpp>

#include <etc/logging.hpp>

void shm_rm(shm_seg_t* seg);

typedef struct vmm_obj {
    uint64_t base;
    uint64_t phys;
    uint64_t len;
    uint64_t flags;

    int pthread_count;

    uint8_t is_shared;
    uint8_t is_mapped;

    uint8_t is_free;
    uint8_t is_lazy_alloc;
    uint8_t is_lazy_allocated;

    uint8_t is_even_need_to_free;

    int* how_much_connected; // used for sharedmem
    locks::spinlock lock;

    uint64_t src_len;

    shm_seg_t* shm;

    struct vmm_obj* next;
} __attribute__((packed)) vmm_obj_t;

namespace memory {
    class vmm {
    public:

        inline static void initproc(arch::x86_64::process_t* proc) {
            vmm_obj_t* start = new vmm_obj_t;
            vmm_obj_t* end = new vmm_obj_t;
            start->is_mapped = 1;
            end->is_mapped = 1;
            zeromem(start);
            zeromem(end);
            start->len = 4096;
            start->next = end;
            start->pthread_count = 1;
            end->base = (std::uint64_t)Other::toVirt(0) - 4096;
            end->next = start;
            proc->vmm_start = (char*)start;
            proc->vmm_end = (char*)end;
        }

        inline static vmm_obj_t* find_free(vmm_obj_t* start, std::uint64_t len) {
            return 0;
        }
        
        inline static vmm_obj_t* v_alloc(vmm_obj_t* start,std::uint64_t len) {
            vmm_obj_t* current = start->next;
            vmm_obj_t* prev = start;
            uint64_t found = 0;

            uint64_t align_length = ALIGNUP(len,4096);

            vmm_obj_t* top = find_free(start,len);
            if(top) {
                top->is_lazy_alloc = 0;
                top->is_lazy_allocated = 0;
                top->is_free = 0;
                top->is_shared = 0;
                top->is_mapped = 0;
                return top;
            }
            
            while(current) {

                if(prev) {

                    uint64_t prev_end = (prev->base + prev->len);
                    if(current->base - prev_end >= align_length) {

                        vmm_obj_t* vmm_new = new vmm_obj_t;
                        vmm_new->base = prev_end;
                        vmm_new->len = align_length;
                        vmm_new->is_lazy_alloc = 0;
                        vmm_new->is_lazy_allocated = 0;
                        vmm_new->is_shared = 0;
                        vmm_new->is_free = 0;
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

        inline static vmm_obj_t* v_find(arch::x86_64::process_t* proc,vmm_obj_t* start, std::uint64_t base, std::uint64_t len, int is_fixed) {
        uint64_t align_length = ALIGNUP(len, 4096);
        uint64_t end = base + align_length;

        vmm_obj_t* current = start->next;
        vmm_obj_t* prev = start;

        is_fixed = 0;

        if (is_fixed) {

            while (current) {
                uint64_t curr_start = current->base;
                uint64_t curr_end = current->base + current->len;

                if (!(curr_end <= base || curr_start >= end)) {
                    vmm_obj_t* to_delete = current;
                    prev->next = current->next;
                    current = current->next;
                    
                    //nolockunmap(proc,current->base);
                    continue; 
                }

                if (curr_start >= end) {
                    break;
                }

                prev = current;
                current = current->next;
            }

            vmm_obj_t* vmm_new = new vmm_obj_t;
            vmm_new->base = base;
            vmm_new->len = align_length;
            vmm_new->is_lazy_alloc = 0;
            vmm_new->is_lazy_allocated = 0;
            vmm_new->is_shared = 0;

            vmm_new->next = current;
            prev->next = vmm_new;
            return vmm_new;

        } else {

            while (current) {
                uint64_t gap_start = prev->base + prev->len;
                uint64_t gap_end = current->base;

                uint64_t actual_start = (gap_start < base) ? base : gap_start;

                if (gap_end >= actual_start + align_length) {
                    vmm_obj_t* vmm_new = new vmm_obj_t;
                    vmm_new->base = actual_start;
                    vmm_new->len = align_length;
                    vmm_new->is_lazy_alloc = 0;
                    vmm_new->is_lazy_allocated = 0;
                    vmm_new->is_shared = 0;

                    vmm_new->next = current;
                    prev->next = vmm_new;
                    return vmm_new;
                }

                prev = current;
                current = current->next;
            }
        }

        return nullptr;
    }

        inline static void* map(arch::x86_64::process_t* proc, std::uint64_t base, std::uint64_t length, std::uint64_t flags) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();

            while(current) {

                if(current->phys == base) { ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
                        return (void*)current->base; }

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }

            current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* new_vmm = v_alloc(current,length);
            new_vmm->flags = flags;
            new_vmm->phys = base;
            new_vmm->src_len = length;
            new_vmm->is_mapped = 1;
            paging::maprangeid(proc->original_cr3,base,new_vmm->base,length,flags,*proc->vmm_id);
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return (void*)new_vmm->base;
        }

        inline static void* custom_map(arch::x86_64::process_t* proc, std::uint64_t virt, std::uint64_t phys, std::uint64_t length, std::uint64_t flags) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();
            void* new_virt = mark(proc,virt,phys,length,flags,0,0);
            paging::maprangeid(proc->original_cr3,phys,(std::uint64_t)virt,length,flags,*proc->vmm_id);
            current->lock.unlock();
            vmm_obj_t* new_vmm = get(proc,virt);
            new_vmm->is_mapped = 1;
            new_vmm->src_len = length;

            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();

            return (void*)virt;
        } 

        inline static std::uint64_t shm_map(arch::x86_64::process_t* proc, shm_seg_t* shm, std::uint64_t base) {
            
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();
            
            if(base == 0) {
                vmm_obj_t* new_vmm = v_alloc(current,shm->len);
                new_vmm->flags = PTE_PRESENT | PTE_RW | PTE_USER;
                new_vmm->phys = shm->phys;
                new_vmm->src_len = shm->len;
                new_vmm->is_mapped = 1;
                new_vmm->shm = shm;
                paging::maprangeid(proc->original_cr3,shm->phys,new_vmm->base,shm->len,PTE_PRESENT | PTE_RW | PTE_USER,*proc->vmm_id);
                new_vmm->shm->ctl.shm_atime = getUnixTime();
                new_vmm->shm->ctl.shm_lpid = proc->id;
                new_vmm->shm->ctl.shm_nattch++;
                ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
                return (std::uint64_t)new_vmm->base;
            } else {

                void* new_virt = mark(proc,base,shm->phys,shm->len,PTE_PRESENT | PTE_RW | PTE_USER,0,0);
                paging::maprangeid(proc->original_cr3,shm->phys,(std::uint64_t)base,shm->len,PTE_PRESENT | PTE_RW | PTE_USER,*proc->vmm_id);
                current->lock.unlock();
                vmm_obj_t* new_vmm = get(proc,base);
                new_vmm->is_mapped = 1;
                new_vmm->src_len = shm->len;
                new_vmm->shm = shm;

                new_vmm->shm->ctl.shm_atime = getUnixTime();
                new_vmm->shm->ctl.shm_lpid = proc->id;
                new_vmm->shm->ctl.shm_nattch++;
                ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
                return (std::uint64_t)base;
            }

            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return 0;
        }

        inline static void* mark(arch::x86_64::process_t* proc, std::uint64_t base, std::uint64_t phys, std::uint64_t length, std::uint64_t flags, int is_shared, int is_fixed) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            vmm_obj_t* new_vmm = v_find(proc,current,base,length,is_fixed);
            new_vmm->flags = flags;
            new_vmm->phys = phys;
            new_vmm->src_len = length;
            new_vmm->base = base;
            new_vmm->is_mapped = 0;
            new_vmm->is_free = 0;
            new_vmm->len = ALIGNUP(length,4096);
            new_vmm->is_shared = is_shared;
            return (void*)new_vmm->base;
        }

        inline static vmm_obj_t* get(arch::x86_64::process_t* proc, std::uint64_t base) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();

            while(current) {

                if(current->base == base) { ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
                    return current; }

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return 0;
        }

        inline static vmm_obj_t* nolockgetlen(arch::x86_64::process_t* proc, std::uint64_t addr) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

            while (current) {

                if (addr >= current->base && addr < current->base + current->len) {
                    return current;
                }

                if (current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }
            return 0;
        }


        inline static vmm_obj_t* getlen(arch::x86_64::process_t* proc, std::uint64_t addr) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();

            while (current) {

                if (addr >= current->base && addr < current->base + current->len) {
                    ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
                    return current;
                }

                if (current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                current = current->next;
            }
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return 0;
        }

        inline static void clone(arch::x86_64::process_t* dest_proc, arch::x86_64::process_t* src_proc) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)src_proc->vmm_start;
            current->lock.lock();

            arch::x86_64::process_t* proc = dest_proc;

            proc->ctx.cr3 = memory::pmm::_physical::alloc(4096);
            proc->original_cr3 = proc->ctx.cr3;
            std::uint64_t cr3 = proc->ctx.cr3;
            
            for(int i = 255; i < 512; i++) {
                std::uint64_t* virt_usrcr3 = (std::uint64_t*)Other::toVirt(proc->ctx.cr3);
                std::uint64_t* virt_kercr3 = (std::uint64_t*)Other::toVirt(memory::paging::kernelget());
                virt_usrcr3[i] = virt_kercr3[i];
            }

            if(dest_proc && src_proc) {

                vmm_obj_t* src_current = (vmm_obj_t*)src_proc->vmm_start;

                while(src_current) {

                    uint64_t phys;

                    if(!src_current->is_free && src_current->base != 0 && src_current->base != (std::uint64_t)Other::toVirt(0) - 4096) {

                        if(!src_current->is_lazy_allocated && !src_current->is_lazy_alloc && src_current->phys) {
                            if(src_current->is_shared) {
                                *src_current->how_much_connected = *src_current->how_much_connected + 1;
                                phys = src_current->phys;
                            } else {
                                if(src_current->src_len <= 4096 && !src_current->is_mapped)
                                    phys = memory::pmm::_physical::alloc(4096);
                                else if(src_current->src_len > 4096 && !src_current->is_mapped)
                                    phys = memory::pmm::_physical::alloc(src_current->src_len);
                                else if(src_current->is_mapped)
                                    phys = src_current->phys;

                                if(!src_current->is_mapped)
                                    memcpy((void*)Other::toVirt(phys),(void*)Other::toVirt(src_current->phys),src_current->len);
                            }
                            memory::paging::maprangeid(cr3,phys,src_current->base,src_current->len,src_current->flags,*proc->vmm_id);
                        } else {
                            phys = 0;
                            memory::paging::duplicaterangeifexists(src_proc->original_cr3,dest_proc->original_cr3,src_current->base,src_current->len,src_current->flags);
                        }

                        mark(dest_proc,src_current->base,phys,src_current->src_len,src_current->flags,src_current->is_shared,1);
                        get(dest_proc,src_current->base)->is_mapped = src_current->is_mapped;

                        get(dest_proc,src_current->base)->is_lazy_alloc = src_current->is_lazy_alloc;
                        get(dest_proc,src_current->base)->is_lazy_allocated = src_current->is_lazy_allocated;

                    }

                    if(src_current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                        break;

                    src_current = src_current->next;
                }

            }
            ((vmm_obj_t*)src_proc->vmm_start)->lock.unlock();
        }

        inline static void reload(arch::x86_64::process_t* proc) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            if(proc->ctx.cr3 && proc->original_cr3) { /* We should free all paging memory */
                memory::pmm::_physical::fullfree(*proc->vmm_id);
                memory::pmm::_physical::free(proc->original_cr3);
            }
            proc->ctx.cr3 = memory::pmm::_physical::alloc(4096);
            proc->original_cr3 = proc->ctx.cr3;
            std::uint64_t cr3 = proc->ctx.cr3;
            
            for(int i = 255; i < 512; i++) {
                std::uint64_t* virt_usrcr3 = (std::uint64_t*)Other::toVirt(proc->ctx.cr3);
                std::uint64_t* virt_kercr3 = (std::uint64_t*)Other::toVirt(memory::paging::kernelget());
                virt_usrcr3[i] = virt_kercr3[i];
            }
            
            //memory::paging::mapkernel(cr3,*proc->vmm_id);
            //memory::paging::alwaysmappedmap(cr3,*proc->vmm_id);
            //memory::paging::maprangeid(cr3,Other::toPhys(proc->syscall_stack),(std::uint64_t)proc->syscall_stack,SYSCALL_STACK_SIZE,PTE_USER | PTE_PRESENT | PTE_RW,*proc->vmm_id);
            while(current) {

                if(current->phys) {
                    memory::paging::maprangeid(cr3,current->phys,current->base,current->len,current->flags,*proc->vmm_id);
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
                    memory::paging::maprangeid(proc->original_cr3,current->phys,current->base,current->len,current->flags,*proc->vmm_id);

                    return;

                }

                current = current->next;
            }
        }

        inline static void unmap(arch::x86_64::process_t* proc, std::uint64_t virt) {
            vmm_obj_t* current = getlen(proc,virt);
            if(!current)
                return;
            vmm_obj_t* start = (vmm_obj_t*)proc->vmm_start;
            start->lock.lock();
            vmm_obj_t* prev = start;
            while(prev) {
                if(prev->next == current)
                    break;
                prev = prev->next;
            }
            prev->next = current->next;

            if(!current->shm) {
                if(current->is_lazy_alloc) {
                    memory::paging::destroyrange(proc->original_cr3,current->base,current->len);
                    memory::paging::zerorange(proc->original_cr3,(std::uint64_t)current->base,current->len);
                } else {
                    memory::pmm::_physical::free(current->phys);
                    memory::paging::zerorange(proc->original_cr3,(std::uint64_t)current->base,current->len);
                }
            } else {
                current->shm->ctl.shm_dtime = getUnixTime();
                current->shm->ctl.shm_lpid = proc->id;
                current->shm->ctl.shm_nattch--;
                memory::paging::zerorange(proc->original_cr3,(std::uint64_t)current->base,current->len);

                if(current->shm->ctl.shm_nattch == 0 && current->shm->is_pending_rm) {
                    shm_rm(current->shm);
                }

                current->shm = 0;

            }

            current->is_free = 1;

            if(current->how_much_connected)
                delete current->how_much_connected;
            delete current;
            
            start->lock.unlock();
        }

        inline static void free(arch::x86_64::process_t* proc) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

            current->lock.lock();
            current->pthread_count--;

            if(current->pthread_count != 0) { current->lock.unlock(); 
                return; }

            if(!current) 
                return;

            vmm_obj_t* next = current->next;

            while (current)
            {
                next = current->next;
                current->next = 0;

                if(current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    next = 0;

                if(!current->is_free) {
                    if(current->base != 0 && current->base != (std::uint64_t)Other::toVirt(0) - 4096) {
                        if(!current->is_lazy_allocated && !current->is_lazy_alloc) {
                            if(!current->is_mapped && !current->is_shared && !current->is_lazy_alloc) {
                                memory::pmm::_physical::free(current->phys);
                            } else if(current->is_shared && !current->is_mapped) {
                                if(*current->how_much_connected == 1) {
                                    memory::pmm::_physical::free(current->phys);
                                    delete (void*)current->how_much_connected;
                                } else
                                    *current->how_much_connected = *current->how_much_connected - 1;
                            }
                        } else {
                            memory::paging::destroyrange(proc->original_cr3,current->base,current->len);
                        }
                    }
                }
 
                delete current;

                if(!next)   
                    break;

                current = next;
            }

            memory::paging::destroy(proc->original_cr3);

            proc->original_cr3 = 0;

            proc->vmm_start = 0;
    
        }

        inline static void* alloc(arch::x86_64::process_t* proc, std::uint64_t len, std::uint64_t flags, int is_shared) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();
            vmm_obj_t* new_vmm = v_alloc(current,len);
            new_vmm->flags = flags;
            new_vmm->src_len = len;
            new_vmm->is_mapped = 0;
            std::uint64_t phys = memory::pmm::_physical::alloc(len);
            new_vmm->phys = phys;

            if(is_shared) {
                new_vmm->is_shared = 1;
                new_vmm->how_much_connected = new int;
            }

            paging::maprangeid(proc->original_cr3,new_vmm->phys,new_vmm->base,new_vmm->len,flags,*proc->vmm_id);
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return (void*)new_vmm->base;
        }

        inline static void* lazy_alloc(arch::x86_64::process_t* proc, std::uint64_t len, std::uint64_t flags, int is_shared) {
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();
            vmm_obj_t* new_vmm = v_alloc(current,len);
            new_vmm->flags = flags;
            new_vmm->src_len = len;
            new_vmm->is_mapped = 0;
            new_vmm->is_lazy_alloc = 1;
            new_vmm->is_lazy_allocated = 1;
            new_vmm->phys = 0;
            memory::paging::zerorange(proc->original_cr3,(std::uint64_t)new_vmm->base,new_vmm->len);
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return (void*)new_vmm->base;
        }

        inline static vmm_obj_t* changemem(arch::x86_64::process_t* proc, std::uint64_t flags) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();

            while (current) {

                if (current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

                if(current->base != 0) {
                    memory::paging::changerange(proc->original_cr3,current->base,current->len,flags | ((current->flags) & ~(PTE_RW)));
                }

                current = current->next;
            }
            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();
            return 0;
        }

        inline static void* customalloc(arch::x86_64::process_t* proc, std::uint64_t virt, std::uint64_t len, std::uint64_t flags, int is_shared, int is_fixed) {
            asm volatile("cli"); // bug if they are enabled
            vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
            current->lock.lock();
            std::uint64_t phys = memory::pmm::_physical::alloc(len);
            void* new_virt = mark(proc,virt,phys,len,flags,is_shared,is_fixed);
            paging::maprangeid(proc->original_cr3,phys,(std::uint64_t)new_virt,len,flags,*proc->vmm_id);
            current->lock.unlock();
            vmm_obj_t* new_vmm = get(proc,(std::uint64_t)new_virt);
            if(is_shared) {
                new_vmm->is_shared = 1;
                new_vmm->how_much_connected = new int;
            }

            ((vmm_obj_t*)proc->vmm_start)->lock.unlock();

            return (void*)new_virt;
        } 

    };
}