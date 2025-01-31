
#include <stdint.h>

extern "C" {

typedef struct __attribute__((packed)) {
    uint16_t size;
    uint64_t base;
} gdt_pointer_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint16_t baselow16;
    uint8_t basemid8;
    uint8_t access;
    uint8_t granularity;
    uint8_t basehigh8;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t length;
    uint16_t baselow16;
    uint8_t basemid8;
    uint8_t flags0;
    uint8_t flags1;
    uint8_t basehigh8;
    uint32_t baseup32;
    uint32_t reserved;
} tss_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint32_t reserved2;
    uint32_t reserved3;
    uint16_t reserved4;
    uint16_t iopb_offsset;
} tss_t;

typedef struct __attribute__((packed)) {
    gdt_entry_t zero;
    gdt_entry_t _64bitcode;
    gdt_entry_t _64bitdata;
    gdt_entry_t usercode;
    gdt_entry_t userdata;
    tss_entry_t tss;
} gdt_t;

}

class GDT {
public:

    gdt_t gdt;
    tss_t tss;
    gdt_pointer_t gdtr;

    static void Init();

};

#pragma once