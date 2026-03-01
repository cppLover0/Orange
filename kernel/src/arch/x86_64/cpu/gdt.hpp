#pragma once
#include <cstdint>

namespace x86_64 {
    namespace gdt {

        typedef struct __attribute__((packed)) {
            std::uint16_t size;
            std::uint64_t base;
        } gdt_pointer_t;

        typedef struct __attribute__((packed)) {
            std::uint16_t limit;
            std::uint16_t baselow16;
            std::uint8_t basemid8;
            std::uint8_t access;
            std::uint8_t granularity;
            std::uint8_t basehigh8;
        } gdt_entry_t;

        typedef struct __attribute__((packed)) {
            std::uint16_t length;
            std::uint16_t baselow16;
            std::uint8_t basemid8;
            std::uint8_t flags0;
            std::uint8_t flags1;
            std::uint8_t basehigh8;
            std::uint32_t baseup32;
            std::uint32_t reserved;
        } tss_entry_t;

        typedef struct __attribute__((packed)) {
            std::uint32_t reserved0;
            std::uint64_t rsp[3];
            std::uint64_t reserved1;
            std::uint64_t ist[7];
            std::uint32_t reserved2;
            std::uint32_t reserved3;
            std::uint16_t reserved4;
            std::uint16_t iopb_offsset;
        } tss_t;

        typedef struct __attribute__((packed)) {
            gdt_entry_t zero;
            gdt_entry_t _64bitcode;
            gdt_entry_t _64bitdata;
            gdt_entry_t usercode;
            gdt_entry_t userdata;
            tss_entry_t tss;
        } gdt_t;

        extern "C" void load_gdt(void* gdtr);
        extern "C" void load_tss();
        void init();
    }
};