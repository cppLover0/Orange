
#include <stdint.h>
#include <generic/elf/elf.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
#include <other/assert.hpp>

void ELF::Load(uint8_t* base) {
    elfheader_t* head = (elfheader_t*)base;
    
    char elf_first[4] = {0x7F,'E','L','F'};
    Assert(!String::strncmp((char*)head->e_ident,elf_first,4),"Invalid ELF binary\n");

}