
#include <stdint.h>
#include <arch/x86_64/scheduling/scheduling.hpp>

#pragma once

#define FD_NONE 0
#define FD_FILE 1
#define FD_PIPE 2

typedef struct {

    char* buffer;
    uint64_t buffer_size;

    _Atomic char is_received;

} __attribute__((packed)) pipe_t;

typedef struct fd_struct {  
    int index;

    process_t* proc;
    struct fd_struct* parent;
    struct fd_struct* next;
    
    long seek_offset;

    char type;
    pipe_t pipe;

    char path_point[2048];

} __attribute__((packed)) fd_t;

class FD {
public:
    static int Create(process_t* proc,char is_pipe);
    static fd_t* Search(process_t* proc,int index);
};