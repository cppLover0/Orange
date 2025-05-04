
#include <stdint.h>
#include <generic/elf/elf.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
#include <other/assert.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <config.hpp>
#include <other/hhdm.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/memory/heap.hpp>
#include <other/string.hpp>
#include <generic/memory/vmm.hpp>
#include <other/assembly.hpp>

#define PUT_STACK(dest,value) *--dest = value;
#define PUT_STACK_STRING(dest,string) String::memcpy(dest,string,String::strlen(string)); 
    

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
        temp_stack -= CALIGNPAGEUP(String::strlen(arr[i]),8);
    }

    Log("CXC");

    return temp_stack;

}

uint64_t* __elf_copy_to_stack_without_out(uint64_t* arr,uint64_t* stack,uint64_t len) {

    uint64_t* _stack = stack;

    for(uint64_t i = 0;i < len;i++) {
        PUT_STACK(_stack,arr[i]);
    }   

    return _stack;

}

ELFLoadResult ELF::Load(uint8_t* base,uint64_t* cr3,uint64_t flags,uint64_t* stack,char** argv,char** envp,process_t* proc) {
    elfheader_t* head = (elfheader_t*)base;

    elfprogramheader_t* current_head;

    uint64_t elf_base = UINT64_MAX;
    uint64_t size = 0;
    uint64_t phdr = 0;
    ELFLoadResult res;

    char elf_first[4] = {0x7F,'E','L','F'};
    if(String::strncmp((char*)head->e_ident,elf_first,4))
        return res;

    res.entry = (void (*)())head->e_entry;

    for (int i = 0; i < head->e_phnum; i++) {
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
        } else if(current_head->p_type == PT_INTERP) {

            filestat_t stat;

            int status = VFS::Stat((char*)((uint64_t)base + current_head->p_offset),(char*)&stat);

            if(status) {
                res.entry = 0;
                return res;
            }

            char* inter = (char*)PMM::VirtualBigAlloc(CALIGNPAGEUP(stat.size,4096) / 4096);

            VFS::Read(inter,stat.name,0);

            ELFLoadResult inter_r = ELF::Load((uint8_t*)inter,cr3,flags,(uint64_t*)1,0,0,proc);

            res.entry = inter_r.entry;

        }


        if (current_head->p_type == PT_LOAD ) {
            end = current_head->p_vaddr - elf_base + current_head->p_memsz;
            size = MAX2(size,end);
        }

        
    }

    uint8_t* elf = (uint8_t*)elf_base;

    uint64_t aligned_size = ALIGNPAGEUP(size) / PAGE_SIZE;

    void* elf_vmm = VMM::Alloc(proc,elf_base + size,PTE_PRESENT | PTE_RW | PTE_USER);

    uint8_t* allocated_elf = (uint8_t*)VMM::Get(proc,(uint64_t)elf_vmm)->phys;
    uint64_t phys_elf = (uint64_t)allocated_elf;

    allocated_elf = (uint8_t*)HHDM::toVirt((uint64_t)allocated_elf);

    Log("phys_elf: 0x%p, virt_elf: 0x%p\n",phys_elf,elf_base);

    for(uint64_t i = 0;i < size; i += PAGE_SIZE) 
        Paging::Map(cr3,phys_elf + i,elf_base + i,flags);


    res.phys_cr3 = (uint64_t*)HHDM::toPhys((uint64_t)cr3);

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t dest = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        if(current_head->p_type == PT_LOAD) {
            dest = (uint64_t)allocated_elf + (current_head->p_vaddr - elf_base);
            //Log("Dest: 0x%p, calc: 0x%p 0x%p 0x%p\n",dest,(current_head->p_vaddr - elf_base),current_head->p_vaddr,(uint64_t)allocated_elf + (current_head->p_vaddr - elf_base));
            String::memset((void*)dest,0,current_head->p_filesz);
            String::memcpy((void*)dest,(void*)((uint64_t)base + current_head->p_offset), current_head->p_filesz);
        }

    }

    if(!stack)  {

        stack = (uint64_t*)VMM::Alloc(proc,(PROCESS_STACK_SIZE + 1) * PAGE_SIZE,PTE_RW | PTE_PRESENT | PTE_USER);

        proc->user_stack_start = (uint64_t)stack;
    
        uint64_t* prepare_cr3 = Paging::KernelGet();

        uint64_t virt = (uint64_t)proc->user_stack_start;
        uint64_t phys = VMM::Get(proc,(uint64_t)stack)->phys;

        //Log("ddd\n");
        for(uint64_t i = 0;i < PROCESS_STACK_SIZE + 1;i++) {
            Paging::Map(prepare_cr3,phys + (i * PAGE_SIZE),virt + (i * PAGE_SIZE),PTE_PRESENT | PTE_RW);
            __invlpg(virt + (i * PAGE_SIZE));
        }

        stack = (uint64_t*)((uint64_t)stack + (PROCESS_STACK_SIZE * PAGE_SIZE));

        Log("stack: 0x%p\n",stack);
    }

    if(stack && argv && envp && ((uint64_t)stack != 1)) {

        uint64_t* _stack = stack;

        uint64_t auxv_stack[] = {(uint64_t)head->e_entry,AT_ENTRY,phdr,AT_PHDR,head->e_phentsize,AT_PHENT,head->e_phnum,AT_PHNUM,PAGE_SIZE,AT_PAGESZ};
        uint64_t argv_length = __elf_get_length(argv);
        uint64_t envp_length = __elf_get_length(envp);

        char** stack_argv = (char**)KHeap::Malloc(8 * argv_length);
        char** stack_envp = (char**)KHeap::Malloc(8 * envp_length);
        Log("CXC");

        _stack = __elf_copy_to_stack(argv,_stack,stack_argv,argv_length);
        Log("CXC");
        _stack = __elf_copy_to_stack(envp,_stack,stack_envp,envp_length);

        Log("CXC");

        PUT_STACK(_stack,0);

        _stack = __elf_copy_to_stack_without_out(auxv_stack,_stack,sizeof(auxv_stack) / 8);
        PUT_STACK(_stack,0);

        _stack = __elf_copy_to_stack_without_out((uint64_t*)stack_envp,_stack,envp_length);
        PUT_STACK(_stack,0);

        _stack = __elf_copy_to_stack_without_out((uint64_t*)stack_argv,_stack,argv_length);
        PUT_STACK(_stack,argv_length);

        res.ready_stack = _stack;

        KHeap::Free(stack_argv);
        KHeap::Free(stack_envp);

    } else {
        res.ready_stack = stack;
        res.argv = 0;
        res.envp = 0;
        res.argc = 0;
    }

    Log("Copying st1ack\n");

    return res;

}