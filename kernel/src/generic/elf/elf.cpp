
#include <stdint.h>
#include <generic/elf/elf.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
#include <other/assert.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <config.hpp>
#include <other/hhdm.hpp>

ELFLoadResult ELF::Load(uint8_t* base,uint64_t* cr3,uint64_t flags,uint64_t* stack) {
    elfheader_t* head = (elfheader_t*)base;

    elfprogramheader_t* current_head;

    uint64_t elf_base = UINT64_MAX;
    uint64_t size = 0;
    uint64_t phdr = 0;
    ELFLoadResult res;
    res.entry = 0;

    char elf_first[4] = {0x7F,'E','L','F'};
    if(String::strncmp((char*)head->e_ident,elf_first,4))
        return res;

    for (int i = 0; i < head->e_phnum; i++) {
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        elf_base = MIN2(elf_base, current_head->p_vaddr);
    }

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t end = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);

        if(current_head->p_type == PT_PHDR) {
            phdr = current_head->p_vaddr;
        }

        end = current_head->p_vaddr - elf_base + current_head->p_memsz;
        size = MAX2(size,end);
    }

    uint8_t* elf = (uint8_t*)elf_base;

    uint64_t aligned_size = ALIGNPAGEUP(size) / PAGE_SIZE;
    uint8_t* allocated_elf = (uint8_t*)PMM::VirtualBigAlloc(aligned_size);
    uint64_t phys_elf = HHDM::toPhys((uint64_t)allocated_elf);

    if(!allocated_elf) 
        return res;

    for(uint64_t i = 0;i < size; i += PAGE_SIZE)
        Paging::Map(cr3,phys_elf + i,elf_base + i,flags);

    for(int i = 0;i < head->e_phnum; i++) {
        uint64_t dest = 0;
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        dest = (uint64_t)allocated_elf + current_head->p_vaddr;
        String::memset((void*)dest,0,current_head->p_filesz);
        String::memcpy((void*)dest,(void*)((uint64_t)base + current_head->p_offset), current_head->p_filesz);

    }

    uint64_t auxv_stack[] = {9,head->e_entry,3,phdr,};

    res.entry = (void (*)(int argc,char** argv))head->e_entry;

    return res;

}