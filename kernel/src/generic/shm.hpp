#include <cstdint>
#include <generic/lock/spinlock.hpp>

#pragma once

typedef unsigned short ushort;

#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2
#define IPC_INFO 3

struct ipc_perm {
    std::uint32_t  key; 
    ushort uid;   
    ushort gid; 
    ushort cuid;  
    ushort cgid; 
    ushort mode; 
    ushort seq;   
};

struct shmid_ds {
    struct ipc_perm shm_perm;   
    int             shm_segsz;  
    long          shm_atime;  
    long          shm_dtime;
    long          shm_ctime;   
    unsigned short  shm_cpid;  
    unsigned short  shm_lpid;   
    short           shm_nattch;  
};

typedef struct shm_seg {
    std::uint32_t key;
    std::uint32_t id;
    std::uint64_t phys;
    std::uint64_t len;
    int is_pending_rm;
    struct shmid_ds ctl;
    struct shm_seg* next;
} shm_seg_t;

namespace shm {

    inline locks::spinlock lock;

    shm_seg_t* shm_find_by_key(int key);
    shm_seg_t* shm_find(int id);
    void shm_rm(shm_seg_t* seg);
    shm_seg_t* shm_create(int key, std::size_t size);
    
}