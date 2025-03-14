
#include <stdint.h>

#pragma once

typedef struct {
    unsigned char e_ident[4];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff; 
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize; 
    uint16_t e_phnum; 
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elfheader_t;
  
typedef struct {
    uint32_t p_type;
    uint32_t p_offset; 
    uint32_t p_vaddr; 
    uint32_t p_paddr;
    uint32_t p_filesz; 
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elfprogramheader_t;

class ELF {
public:
    static void Load(uint8_t* base);
};