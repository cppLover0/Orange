
#include <stdint.h>
#include <generic/elf/elf.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
#include <other/assert.hpp>

void ELF::Load(uint8_t* base) {
    elfheader_t* head = (elfheader_t*)base;

    elfprogramheader_t* current_head;

    uint64_t elf_base = UINT64_MAX;
    char elf_first[4] = {0x7F,'E','L','F'};
    Assert(!String::strncmp((char*)head->e_ident,elf_first,4),"Invalid ELF binary\n");

    Log("%d\n",head->e_phnum);

    for (int i=0; i<head->e_phnum; i++) {
        current_head = (elfprogramheader_t*)((uint64_t)base + head->e_phoff + head->e_phentsize*i);
        elf_base = MIN2(elf_base, current_head->p_vaddr);
        Log("bas: 0x%p\n",current_head->p_vaddr);
    }

    Log("ELF Base: 0x%p\n",elf_base);

}