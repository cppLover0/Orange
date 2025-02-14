
#include <stdint.h>

#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <other/string.hpp>
#include <config.hpp>

gdt_t original_gdt = {
    {0,0,0,0,0,0}, // 0x0 null
    {0,0,0,0x9a,0xa2,0}, // 0x08 64 bit code
    {0,0,0,0x92,0xa0,0}, // 0x10 64 bit data
    {0,0,0,0xFA,0x20,0}, // 0x18 user code
    {0,0,0,0xF2,0,0}, // 0x20 user data
    {0x68,0,0,0x89,0x20,0,0,0} // 0x28 tss
};

extern "C" {

extern void loadTSS();
extern void loadGDT(void* gdtr);

}

void GDT::Init() {
    cpudata_t* data = CpuData::Access();
    String::memcpy(&data->gdt.gdt,&original_gdt,sizeof(gdt_t));
    gdt_t* gdt = &data->gdt.gdt;
    tss_t* tss = &data->gdt.tss;
    uint64_t stack_1 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for rsp[0]
    uint64_t stack_2 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for ist 1 (exceptions)
    uint64_t stack_3 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for ist 2 (some irqs)
    uint64_t stack_4 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for ist 3 (timer)
    tss->rsp[0] = stack_1 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    tss->ist[0] = stack_2 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    tss->ist[1] = stack_3 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    tss->ist[2] = stack_4 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    tss->iopb_offsset = sizeof(tss);
    gdt->tss.baselow16 = (uint64_t)tss & 0xFFFF;
    gdt->tss.basemid8 = ((uint64_t)tss >> 16) & 0xFF;
    gdt->tss.basehigh8 = ((uint64_t)tss >> 24) & 0xFF;
    gdt->tss.baseup32 = (uint64_t)tss >> 32;
    Paging::alwaysMappedAdd(stack_1,TSS_STACK_IN_PAGES * PAGE_SIZE);
    Paging::alwaysMappedAdd(stack_2,TSS_STACK_IN_PAGES * PAGE_SIZE);
    gdt_pointer_t* gdtr = &data->gdt.gdtr;
    gdtr->size = sizeof(gdt_t) -1;
    gdtr->base = (uint64_t)gdt;
    loadGDT(gdtr);
    loadTSS();
}