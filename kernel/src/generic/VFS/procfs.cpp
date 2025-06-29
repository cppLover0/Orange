
#include <generic/VFS/procfs.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/memory/heap.hpp>
#include <stdint.h>
#include <generic/memory/pmm.hpp>
#include <other/string.hpp>
#include <drivers/hpet/hpet.hpp>
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <generic/memory/pmm.hpp>
#include <other/log.hpp>
#include <arch/x86_64/cpu/data.hpp>

procfs_data_t *head_procdata = 0;
procfs_data_t *last_procdata = 0;

void procfs_reg(char *loc, int (*read)(char *buffer, long hint_size))
{
    if (!head_procdata)
    {
        head_procdata = (procfs_data_t *)PMM::VirtualAlloc();
        String::memcpy(head_procdata->loc, loc, String::strlen(loc));
        head_procdata->read = read;
        last_procdata = head_procdata;
    }
    else
    {
        procfs_data_t *data = (procfs_data_t *)PMM::VirtualAlloc();
        String::memcpy(data->loc, loc, String::strlen(loc));
        data->read = read;
        last_procdata = data;
    }
}

int procfs_read(char *buffer, char *filename, long hint_size)
{
    procfs_data_t *data = head_procdata;
    procfs_data_t *find_data = 0;

    while (data)
    {
        if (!String::strcmp(data->loc, filename))
        {
            find_data = data;
            break;
        }
        data = data->next;
    }

    if (!find_data)
        return ENOENT;

    if (!data->read)
        return ENOENT;

    return data->read(buffer, hint_size);
}

int procfs_uptime(char *buffer, long hint_size)
{
    char buffer0[1024];
    String::memset(buffer0, 0, 1024);
    uint64_t sec = HPET::NanoCurrent() / 1000000000;
    String::itoa(sec, buffer0, 10);
    String::memset(buffer, 0, hint_size);
    String::memcpy(buffer, buffer0, String::strlen(buffer0) <= hint_size ? String::strlen(buffer0) : hint_size);
    *CpuData::Access()->count = String::strlen(buffer0) <= hint_size ? String::strlen(buffer0) : hint_size;
    return 0;
}

char procfs_exists(char *filename)
{
    procfs_data_t *data = head_procdata;
    procfs_data_t *find_data = 0;

    while (data)
    {
        if (!String::strcmp(data->loc, filename))
        {
            find_data = data;
            break;
        }
        data = data->next;
    }

    if (!find_data)
        return 0;

    return 1;
}

int procfs_meminfo(char *buffer, long hint_size)
{

    extern buddy_t buddy;

    int free = 0;
    int nonfree = 0;
    uint64_t free_mem = 0;
    uint64_t total_mem = 0;
    for (uint64_t i = 0; i < buddy.hello_buddy; i++)
    {

        if (buddy.mem[i].information.is_free)
            free_mem += LEVEL_TO_SIZE(buddy.mem[i].information.level);

        if (!buddy.mem[i].information.is_splitted)
            total_mem += LEVEL_TO_SIZE(buddy.mem[i].information.level);

        if (buddy.mem[i].information.is_free)
            free++;
        else if (!buddy.mem[i].information.is_splitted)
            nonfree++;
    }

    char buffer0[1024];

    String::vsprintf(buffer0,"MemTotal:         %d kB\nMemFree:          %d kB\nMemAvailable:     %d kB",total_mem / 1024,free_mem / 1024,(total_mem - free_mem) / 1024);

    String::memset(buffer,0,hint_size);
    String::memcpy(buffer, buffer0, String::strlen(buffer0) <= hint_size ? String::strlen(buffer0) : hint_size);

    *CpuData::Access()->count = String::strlen(buffer0) <= hint_size ? String::strlen(buffer0) : hint_size;
    return 0;
}

int procfs_stat(char* filename,char* buffer,char follow_symlinks) {

    procfs_data_t *data = head_procdata;
    procfs_data_t *find_data = 0;

    while (data)
    {
        if (!String::strcmp(data->loc, filename))
        {
            find_data = data;
            break;
        }
        data = data->next;
    }

    if (!find_data)
        return ENOENT;

    filestat_t* stat = (filestat_t*)buffer;
    stat->content = 0;
    stat->size = 1024; // defaults to 1024
    stat->type = VFS_TYPE_FILE;
    stat->fs_prefix1 = 'P';
    stat->fs_prefix2 = 'R';
    stat->fs_prefix3 = 'C';
    return 0;
}

void ProcFS::Init(filesystem_t *fs)
{
    fs->readfile = procfs_read;
    fs->exists = procfs_exists;
    fs->stat = procfs_stat;
    procfs_reg("/uptime", procfs_uptime);
    procfs_reg("/meminfo", procfs_meminfo);
}