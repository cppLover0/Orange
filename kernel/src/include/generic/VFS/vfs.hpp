
#include <stdint.h>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>

#pragma once

#define STATUS_OK 0
#define STATUS_NOT_IMPLEMENTED -1
#define STATUS_ERROR -2

#define SUBSTATUS_TTY 99

typedef struct {
    char i; // not implemented
} disk_t;

typedef struct {
    int (*readfile)(char* buffer,char* filename,long hint_size);
    int (*writefile)(char* buffer,char* filename,uint64_t size,char is_symlink_path);
    int (*touch)(char* filename);
    int (*create)(char* filename,int type);
    int (*rm)(char* filename);
    
    char (*exists)(char* filename);
    int (*stat)(char* filename,char* buffer);

    int (*askforpipe)(char* filename,pipe_t* pipe);
    int (*instantreadpipe)(char* filename, pipe_t* pipe);

    char is_in_ram;
    disk_t* disk;

} filesystem_t;

typedef struct {
    char* loc;
    filesystem_t* fs;
} mount_location_t;

typedef struct {
    
    char type;
    char sub_type;

    char* name;

    char* content;

    uint32_t size;

    uint64_t file_create_date;
    uint64_t file_change_date;

    char fs_prefix1;
    char fs_prefix2;
    char fs_prefix3;

} __attribute__((packed)) filestat_t;

class VFS {
public:
    static void Init();
    static int Read(char* buffer,char* filename,long hint_size);
    static int Write(char* buffer,char* filename,uint64_t size,char is_symlink_path);
    static int Touch(char* filename);
    static int Create(char* filename,int type);
    static int Remove(char* filename);
    static char Exists(char* filename);
    static int Stat(char* filename,char* buffer);
    static int AskForPipe(char* filename,pipe_t* pipe);
    static int InstantPipeRead(char* filename,pipe_t* pipe);
};