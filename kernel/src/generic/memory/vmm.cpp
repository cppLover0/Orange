
#include <generic/memory/vmm.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/heap.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <lib/limineA/limine.h>
#include <generic/limineA/limineinfo.hpp>
#include <other/log.hpp>
#include <other/hhdm.hpp>
#include <other/string.hpp>
#include <generic/memory/pmm.hpp>
#include <other/assembly.hpp>

vmm_obj_t* vmm_main = 0;
vmm_obj_t* vmm_last = 0;

void VMM::Init(process_t* proc) {

    LimineInfo info;
    vmm_obj_t* start = new vmm_obj_t;
    vmm_obj_t* end = new vmm_obj_t;
    start->base = 0; // ill place userspace allocations here
    start->flags = 0;
    start->len = PAGE_SIZE;
    start->next = end;

    end->base = info.hhdm_offset - PAGE_SIZE;
    end->flags = 0;
    end->len = PAGE_SIZE;
    end->next = start;

    if(proc) {
        proc->vmm_start = (char*)start;
        proc->vmm_last = (char*)end;
    } else {
        vmm_main = start;
        vmm_last = end;
    }

}

// i'm understand what is vmm from here https://github.com/dreamportdev/Osdev-Notes/blob/master/04_Memory_Management/04_Virtual_Memory_Manager.md
vmm_obj_t* __vmm_alloc(vmm_obj_t* vmm_start,uint64_t length) {

    vmm_obj_t* current = vmm_start->next;
    vmm_obj_t* prev = vmm_start;
    uint64_t found = 0;

    uint64_t align_length = ALIGNPAGEUP(length);
    
    while(current) {

        if(prev) {

            uint64_t prev_end = (prev->base + prev->len);

            //Log("Checking memory 0x%p-0x%p, length: 0x%p\n",current->base,current->base + current->len,align_length);

            if(current->base - prev_end >= align_length) {

                vmm_obj_t* vmm_new = new vmm_obj_t;
                vmm_new->base = prev_end;
                vmm_new->len = align_length;
                prev->next = vmm_new;
                vmm_new->next = current;

                return vmm_new;
            }
        }

        prev = current;
        current = current->next;
    }

    return 0;

}

vmm_obj_t* __vmm_find(vmm_obj_t* vmm_start,uint64_t base,uint64_t length) {

    vmm_obj_t* current = vmm_start->next;
    vmm_obj_t* prev = vmm_start;
    uint64_t found = 0;

    uint64_t align_length = ALIGNPAGEUP(length);
    
    vmm_obj_t* vmm_new = new vmm_obj_t;

    while(current) {

        if(prev) {

            uint64_t prev_end = (prev->base + prev->len);
            uint64_t current_end = base + length;

            if(current->base == base) {
                KHeap::Free(vmm_new);
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

void __vmm_map(uint64_t cr3_phys,uint64_t virt,uint64_t phys,uint64_t flags,uint64_t length) {
    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(cr3_phys);

    //Log("Trying to map 0x%p with len 0x%p to 0x%p\n",phys,length,virt);

    for(uint64_t i = 0; i <= length;i += PAGE_SIZE) {
        Paging::Map(cr3,phys + i,virt + i,flags);
    }

}

void* VMM::Map(process_t* proc,uint64_t base,uint64_t length,uint64_t flags) {
    vmm_obj_t* current = 0;

    if(proc)
        current = (vmm_obj_t*)proc->vmm_start;
    else
        current = vmm_main;

    vmm_obj_t* vmm_new = __vmm_alloc(current,length);
    vmm_new->flags = flags;
    vmm_new->phys = base;
    vmm_new->src_len = length;

    if(proc)
        __vmm_map(proc->ctx.cr3,vmm_new->base,base,flags,length);

    return (void*)vmm_new->base;

}

void* VMM::Alloc(process_t* proc,uint64_t length,uint64_t flags) {
    
    vmm_obj_t* current = 0;

    if(proc)
        current = (vmm_obj_t*)proc->vmm_start;
    else
        current = vmm_main;

    vmm_obj_t* vmm_new = __vmm_alloc(current,length);
    vmm_new->flags = flags;
    vmm_new->src_len = length;

    uint64_t phys = 0;

    if(length < PAGE_SIZE)
        phys = PMM::Alloc();
    else if(length > PAGE_SIZE)
        phys = PMM::BigAlloc(ALIGNPAGEUP(length) / PAGE_SIZE);

    vmm_new->phys = phys;
        
    if(proc)
        __vmm_map(proc->ctx.cr3,vmm_new->base,phys,flags,length);

    return (void*)vmm_new->base;


}

void* VMM::Mark(process_t* proc,uint64_t base,uint64_t phys,uint64_t length,uint64_t flags) {
    vmm_obj_t* current = 0;

    if(proc)
        current = (vmm_obj_t*)proc->vmm_start;
    else
        current = vmm_main;

    //Log("no yay\n");
    vmm_obj_t* vmm_new = __vmm_find(current,base,length);
    //Log("yay\n");

    //Log("0x%p 0x%p yay\n",vmm_new->base,base);

    vmm_new->flags = flags;
    vmm_new->phys = phys;
    vmm_new->src_len = length;
    vmm_new->len = ALIGNPAGEUP(length);
    vmm_new->base = base;

    return (void*)vmm_new->base;
}

void VMM::Modify(process_t* proc,uint64_t dest_base,uint64_t new_phys) {

    vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
    while (current)
    {

        if(current->base == dest_base) {

            current->phys = new_phys;
            //Log("Found ! 0x%p 0x%p\n",current->phys,new_phys);
            __vmm_map(proc->ctx.cr3,current->base,current->phys,current->flags,current->len);

            return;

        }

        current = current->next;
    }
    

}

void VMM::Free(process_t* proc) {
    
    vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
    vmm_obj_t* next = current->next;
    while (current)
    {
        next = current->next;

        PMM::BigFree(current->phys,current->len / PAGE_SIZE);
        KHeap::Free(current);

        current = next;
    }

    current = new vmm_obj_t;

    VMM::Init(proc);
}

void VMM::Clone(process_t* dest_proc,process_t* src_proc) {

    LimineInfo info;

    if(dest_proc && src_proc) {

        vmm_obj_t* src_current = (vmm_obj_t*)src_proc->vmm_start;

        while(src_current) {

            uint64_t phys;

            if(src_current->src_len < PAGE_SIZE)
                phys = PMM::Alloc();
            else if(src_current->src_len > PAGE_SIZE)
                phys = PMM::BigAlloc(ALIGNPAGEUP(src_current->src_len) / PAGE_SIZE);

            if(src_current->phys)
            String::memcpy((void*)HHDM::toVirt(phys),(void*)HHDM::toVirt(src_current->phys),src_current->len);

            Mark(dest_proc,src_current->base,phys,src_current->len,src_current->flags);

            if(src_current->base == info.hhdm_offset - PAGE_SIZE)
                break;

            src_current = src_current->next;
        }

    }

}

vmm_obj_t* VMM::Get(process_t* proc,uint64_t base) {

    LimineInfo info;

    vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

    while(current) {

        if(current->base == base)
            return current;

        if(current->base == info.hhdm_offset - PAGE_SIZE)
            break;

        current = current->next;
    }

}

void VMM::Dump(process_t* proc) {

    if(proc) {
        NLog("\nDumping proccess %d virtual space\n",proc->id);

        LimineInfo info;

        vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;
        vmm_obj_t* prev = 0;

        while(current) {

            vmm_obj_t* next = current->next;
            NLog("mem 0x%p-0x%p:len: 0x%p,flag: 0x%p,ph: 0x%p,base: 0x%p\n",current->base,current->base + current->len,current->len,current->flags,current->phys,current->base,current->next,next ? next->base : 0,next ? next->base + next->len : 0);

            if(current->base == info.hhdm_offset - PAGE_SIZE)
                break;

            prev = current;
            current = current->next;
        }
    }

}

void* VMM::CustomAlloc(process_t* proc,uint64_t virt,uint64_t length,uint64_t flags) {
    vmm_obj_t* current = 0;
    uint64_t phys = 0;

    if(!proc)  
        return 0;

    current = vmm_main;

    if(length < PAGE_SIZE)
        phys = PMM::Alloc();
    else if(length > PAGE_SIZE)
        phys = PMM::BigAlloc(ALIGNPAGEUP(length) / PAGE_SIZE);

    //Log("0x%p\n",phys);

    Mark(proc,virt,phys,length,flags);
    __vmm_map(proc->ctx.cr3,virt,phys,flags,length);

    return (void*)virt;

}

void VMM::Reload(process_t* proc) {

    LimineInfo info;

    vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

    proc->ctx.cr3 = PMM::Alloc();

    uint64_t* virt = (uint64_t*)HHDM::toVirt(proc->ctx.cr3);

    Paging::Kernel(virt);
    Paging::alwaysMappedMap(virt);

    Paging::HHDMMap(virt,HHDM::toPhys((uint64_t)proc->syscall_wait_ctx),PTE_PRESENT | PTE_RW);
    Paging::HHDMMap(virt,HHDM::toPhys((uint64_t)proc->wait_stack),PTE_PRESENT | PTE_RW);

    while(current) {

        __vmm_map(proc->ctx.cr3,current->base,current->phys,current->flags,current->len);

        if(current->base == info.hhdm_offset - PAGE_SIZE)
            break;

        current = current->next;
    }
}