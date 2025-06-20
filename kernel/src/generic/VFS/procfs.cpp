
#include <generic/VFS/procfs.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/memory/heap.hpp>
#include <stdint.h>
#include <generic/memory/pmm.hpp>
#include <other/string.hpp>
#include <drivers/hpet/hpet.hpp>
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>
#include <generic/limineA/limineinfo.hpp>

procfs_data_t* head_procdata = 0;
procfs_data_t* last_procdata = 0;

void procfs_reg(char* loc,int (*read)(char* buffer,long hint_size)) {
    if(!head_procdata) {
        head_procdata = (procfs_data_t*)PMM::VirtualAlloc();
        String::memcpy(head_procdata->loc,loc,String::strlen(loc));
        head_procdata->read = read;
        last_procdata = head_procdata;
    } else {
        procfs_data_t* data = (procfs_data_t*)PMM::VirtualAlloc();
        String::memcpy(data->loc,loc,String::strlen(loc));
        data->read = read;
        last_procdata = data;
    }
}

int procfs_read(char* buffer,char* filename,long hint_size) {
    procfs_data_t* data = head_procdata;
    procfs_data_t* find_data = 0;

    while(data) {
        if(!String::strcmp(data->loc,filename)) {
            find_data = data;
            break;
        }
        data = data->next;
    }

    if(!find_data)
        return ENOENT;
    
    if(!data->read)
        return ENOENT;

    return data->read(buffer,hint_size);

}

int procfs_uptime(char* buffer,long hint_size) {
    char buffer0[1024];
    String::memset(buffer0,0,1024);
    uint64_t sec = HPET::NanoCurrent() / 1000000000;
    String::itoa(sec,buffer0,10);
    String::memset(buffer,0,hint_size);
    String::memcpy(buffer,buffer0,String::strlen(buffer0) <= hint_size ? String::strlen(buffer0) : hint_size);
    return 0;
}

char procfs_exists(char* filename) {
    procfs_data_t* data = head_procdata;
    procfs_data_t* find_data = 0;

    while(data) {
        if(!String::strcmp(data->loc,filename)) {
            find_data = data;
            break;
        }
        data = data->next;
    }

    if(!find_data)
        return 0;

    return 1;
}

void ProcFS::Init(filesystem_t* fs) {
    fs->readfile = procfs_read;
    fs->exists = procfs_exists;
    procfs_reg("/uptime",procfs_uptime);
}