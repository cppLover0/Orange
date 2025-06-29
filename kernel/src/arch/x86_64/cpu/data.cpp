
#include <stdint.h>
#include <other/assembly.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <other/string.hpp>

cpudata_t* CpuData::Access() {
    cpudata_t* data = (cpudata_t*)__rdmsr(0xC0000101);
    if(!data) {
        data = new cpudata_t;
        String::memset(data,0,sizeof(cpudata_t));
        __wrmsr(0xC0000101,(uint64_t)data);
    }
    return data;
}