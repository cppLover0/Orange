
#include <stdint.h>
#include <arch/x86_64/interrupts/syscalls/ipc/pipe.hpp>

#pragma once

#define STATUS_OK 0
#define STATUS_NOT_IMPLEMENTED -1
#define STATUS_ERROR -2

#define VFS_TYPE_FILE 0
#define VFS_TYPE_DIRECTORY 1
#define VFS_TYPE_SYMLINK 2

#define SUBSTATUS_TTY 99

typedef struct {
    char i; // not implemented
} disk_t;

typedef struct {
    
    char type;
    char sub_type;

    char* name;

    char* content;

    uint64_t mode;
    uint32_t size;

    int idx;

    uint64_t file_create_date;
    uint64_t file_change_date;

    char fs_prefix1;
    char fs_prefix2;
    char fs_prefix3;

} __attribute__((packed)) filestat_t;

typedef struct {
    int (*readfile)(char* buffer,char* filename,long hint_size);
    int (*writefile)(char* buffer,char* filename,uint64_t size,char is_symlink_path,uint64_t offset);
    int (*touch)(char* filename);
    int (*create)(char* filename,int type);
    int (*rm)(char* filename);
    
    char (*exists)(char* filename);
    int (*stat)(char* filename,char* buffer,char follow_symlinks);
    
    int (*iterate)(filestat_t* stat);

    int (*askforpipe)(char* filename,pipe_t* pipe);
    int (*instantreadpipe)(char* filename, pipe_t* pipe);

    int (*chmod)(char* filename,uint64_t mode);

    int (*count)(char* filename,int idx,int count);

    int (*ioctl)(char* filename,unsigned long request, void *arg, int *result);

    char is_in_ram;
    disk_t* disk;

} filesystem_t;

typedef struct {
    char* loc;
    filesystem_t* fs;
} mount_location_t;

class VFS {
public:
    static void Init();
    static int Read(char* buffer,char* filename,long hint_size);
    static int Write(char* buffer,char* filename,uint64_t size,char is_symlink_path,uint64_t offset);
    static int Touch(char* filename);
    static int Create(char* filename,int type);
    static int Remove(char* filename);
    static char Exists(char* filename);
    static int Stat(char* filename,char* buffer,char follow_symlinks);
    static int AskForPipe(char* filename,pipe_t* pipe);
    static int InstantPipeRead(char* filename,pipe_t* pipe);
    static int Iterate(char* filename,filestat_t* stat);
    static int Chmod(char* filename,uint64_t mode);
    static int Ioctl(char* filename,unsigned long request, void *arg, int *result);
    static int Count(char* filename,int idx,int count);
};