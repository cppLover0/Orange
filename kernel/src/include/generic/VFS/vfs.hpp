
#include <stdint.h>

#pragma once

#define STATUS_OK 0
#define STATUS_NOT_IMPLEMENTED -1
#define STATUS_ERROR -2

typedef struct {
    char i; // not implemented
} disk_t;

typedef struct {
    int (*readfile)(char* buffer,char* filename);
    int (*writefile)(char* buffer,char* filename,uint64_t size);
    int (*touch)(char* filename);
    int (*create)(char* filename,int type);
    int (*rm)(char* filename);
    
    char (*exists)(char* filename);

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
    static int Read(char* buffer,char* filename);
    static int Write(char* buffer,char* filename,uint64_t size);
    static int Touch(char* filename);
    static int Create(char* filename,int type);
    static int Remove(char* filename);
    static char Exists(char* filename);
};