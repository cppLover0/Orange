
#include <cstdint>

typedef long time_t;
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
    time_t          shm_atime;  
    time_t          shm_dtime;
    time_t          shm_ctime;   
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