
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <drivers/cmos.hpp>

#include <etc/libc.hpp>

#include <drivers/tsc.hpp>

#include <etc/errno.hpp>

#include <drivers/hpet.hpp>
#include <drivers/kvmtimer.hpp>
#include <generic/time.hpp>
#include <generic/vfs/vfs.hpp>

#include <etc/bootloaderinfo.hpp>
#include <generic/locks/spinlock.hpp>

static_assert(sizeof(shm_seg_t) < 4096, "shm_seg is bigger than page size !");

locks::spinlock shm_lock;
shm_seg_t* shm_head;

int shm_id_ptr = 0;

shm_seg_t* shm_find_by_key(int key) {
    shm_seg_t* current = shm_head;
    while(current) {
        if(current->key == key)
            return current;
    }
    return 0;
}

shm_seg_t* shm_find(int id) {
    shm_seg_t* current = shm_head;
    while(current) {
        if(current->id == id)
            return current;
    }
    return 0;
}

void shm_rm(shm_seg_t* seg) {
    shm_seg_t* prev = shm_head;
    while(prev) {
        if(prev->next == seg)
            break;
        prev = prev->next;
    }

    if(shm_head == seg)
        shm_head = seg->next;
    else if(prev)
        prev->next = seg->next;
        
    memory::pmm::_physical::free(seg->phys);
    delete (void*)seg;
}

shm_seg_t* shm_create(int key, size_t size) {
    shm_seg_t* new_seg = new shm_seg_t;
    memset(new_seg,0,sizeof(shm_seg_t));
    new_seg->next = shm_head;
    shm_head = new_seg;
    new_seg->key = key;
    new_seg->id = shm_id_ptr++;
    new_seg->len = size;
    new_seg->phys = memory::pmm::_physical::alloc(new_seg->len);
    return new_seg;
}

#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000

syscall_ret_t sys_shmget(int key, size_t size, int shmflg) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    std::uint64_t src_size = size;

    if(size < 4096) { 
        size = 4096; 
    } else { 
        size = ALIGNUP(size,4096); 
    }

    shm_lock.lock();

    shm_seg_t* seg = shm_find_by_key(key);

    shm_lock.unlock();

    if(seg && ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)))
        return {1,EEXIST,0};

    if(!seg && !(shmflg & IPC_CREAT))
        return {1,ENOENT,0};
    
    shm_lock.lock();

    if(shmflg & IPC_CREAT) {
        seg = shm_create(key,size); 
        seg->ctl.shm_segsz = src_size;
        seg->ctl.shm_ctime = getUnixTime();
        seg->ctl.shm_cpid = proc->id;
        seg->ctl.shm_perm.cuid = proc->uid;
        seg->ctl.shm_perm.uid = proc->uid;
        seg->ctl.shm_perm.mode = shmflg & 0x1FF;
    }

    DEBUG(proc->is_debug,"Creating/getting shm for proc %d, id %d, size %lli, src_size %d, shmflg %d",proc->id,seg->id,size,src_size,shmflg);

    shm_lock.unlock();
        
    return {1,0,seg->id};
}

// inline static void* map(arch::x86_64::process_t* proc, std::uint64_t base, std::uint64_t length, std::uint64_t flags) {

// inline static void* custom_map(arch::x86_64::process_t* proc, std::uint64_t virt, std::uint64_t phys, std::uint64_t length, std::uint64_t flags) {

syscall_ret_t sys_shmat(int shmid, std::uint64_t hint, int shmflg) {

    shm_lock.lock();

    shm_seg_t* seg = shm_find(shmid);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!seg) { shm_lock.unlock();
        return {1,EINVAL,0}; }

    std::uint64_t new_hint = hint;

    new_hint = memory::vmm::shm_map(proc,seg,hint);

    memory::paging::enablepaging(proc->original_cr3); // try to reset tlb

    DEBUG(proc->is_debug,"Attaching shm %d to 0x%p, shmflg %d from proc %d",shmid,new_hint,shmflg,proc->id);

    shm_lock.unlock();
    return {1,0,new_hint};
}

syscall_ret_t sys_shmdt(std::uint64_t base) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    shm_lock.lock();
    vmm_obj_t* vmm_new = memory::vmm::getlen(proc,base);

    if(!vmm_new) { shm_lock.unlock();
        return {0,EINVAL,0}; }

    if(!vmm_new->shm) {
        shm_lock.unlock();
        return {0,EINVAL,0};
    }

    shm_seg_t* seg = vmm_new->shm;
    //memory::vmm::unmap(proc,vmm_new->base);

    DEBUG(proc->is_debug,"Removing shm %d, base 0x%p from proc %d",seg->id,base,proc->id);

    shm_lock.unlock();
    return {0,0,0};
}

syscall_ret_t sys_shmctl(int shmid, int cmd, struct shmid_ds *buf) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    shm_lock.lock();

    shm_seg_t* seg = shm_find(shmid);

    if(!seg) { shm_lock.unlock();
        return {0,EINVAL,0}; }

    if(!buf) { shm_lock.unlock();
        return {0,EINVAL,0};
    }

    switch(cmd) {
    case IPC_RMID: {
        seg->is_pending_rm = 1;
        if(seg->ctl.shm_nattch == 0 && seg->is_pending_rm) {
            shm_rm(seg);
        }
        break;
    }
    case IPC_STAT:
        memcpy(buf,&seg->ctl,sizeof(shmid_ds));
        break;
    case IPC_SET:
        seg->ctl.shm_perm.uid = buf->shm_perm.uid;
        seg->ctl.shm_perm.gid = buf->shm_perm.gid;
        seg->ctl.shm_ctime = getUnixTime();
        seg->ctl.shm_perm.mode = buf->shm_perm.mode;
        break;
    default:
        Log::SerialDisplay(LEVEL_MESSAGE_WARN,"Unknown shmctl cmd %d for id %d from proc %d\n",cmd,shmid,proc->id);
        break;
    }

    DEBUG(proc->is_debug,"shmctl id %d cmd %d buf 0x%p from proc %d\n",shmid,cmd,buf,proc->id);

    shm_lock.unlock();
    return {0,0,0};
}