#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/safety.hpp>
#include <generic/shm.hpp>

#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000

long long sys_shmget(int key, size_t size, int shmflg) {

    thread* proc = current_proc;
    std::uint64_t src_size = size;

    if(size < 4096) { 
        size = 4096; 
    } else { 
        size = ALIGNUP(size,4096); 
    }

    shm::lock.lock();
    shm_seg_t* seg = shm::shm_find_by_key(key);

    if (seg) {

        if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
            shm::lock.unlock();
            return -EEXIST;
        }

        if (src_size > (std::uint64_t)seg->ctl.shm_segsz) {
            shm::lock.unlock();
            return -EINVAL;
        }

        klibc::debug_printf("Found existing shm for proc %d, id %d\n", proc->id, seg->id);
        shm::lock.unlock();
        return seg->id;
    }

    if (!(shmflg & IPC_CREAT)) {
        shm::lock.unlock();
        return -ENOENT;
    }
    
    seg = shm::shm_create(key, size); 
    if (!seg) {
        shm::lock.unlock();
        return -ENOMEM;
    }

    seg->ctl.shm_segsz = src_size;
    seg->ctl.shm_ctime = time::current_unix_time;
    seg->ctl.shm_cpid = proc->id;
    seg->ctl.shm_perm.cuid = proc->uid;
    seg->ctl.shm_perm.uid = proc->uid;
    seg->ctl.shm_perm.mode = shmflg & 0x1FF;
    seg->ctl.shm_nattch = 0; 

    klibc::debug_printf("Creating new shm for proc %d, id %d, size %lli, src_size %d, shmflg %d\n", proc->id, seg->id, size, src_size, shmflg);

    shm::lock.unlock();
    return seg->id;
}

long long sys_shmat(int shmid, std::uint64_t hint, int shmflg) {

    shm::lock.lock();

    shm_seg_t* seg = shm::shm_find(shmid);

    thread* proc = current_proc;

    if(!seg) { shm::lock.unlock(); //log("shm", "failed to find id %d", shmid);
        return -EINVAL; }

    if(!is_safe_to_rw(proc, (std::uint64_t)hint, seg->len)) {
        return -EFAULT;
    }

    std::uint64_t new_hint = hint;

    new_hint = proc->vmem->map_memory(hint, 0, 0, 0, hint == 0 ? false : true, seg);

    seg->ctl.shm_atime = time::current_unix_time;
    seg->ctl.shm_lpid = proc->id;
    seg->ctl.shm_nattch++;

    arch::tlb_flush(0,0);

    klibc::debug_printf("Attaching shm %d to 0x%p, shmflg %d from proc %d phys 0x%p",shmid,new_hint,shmflg,proc->id, seg->phys);

    shm::lock.unlock();
    return new_hint;
}

long long sys_shmdt(std::uint64_t base) {
    thread* proc = current_proc;

    shm::lock.lock();

    sys_munmap(base, PAGE_SIZE); // munmap must unmap ALL shm

    klibc::debug_printf("Removing base 0x%p from proc %d",base,proc->id);

    shm::lock.unlock();
    return 0;
}

long long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf) {

    thread* current = current_proc;

    shm::lock.lock();

    shm_seg_t* seg = shm::shm_find(shmid);

    if(!seg) { shm::lock.unlock(); klibc::debug_printf("no seg\n");
        return -EINVAL; }

    switch(cmd) {
    case IPC_RMID: {
        seg->is_pending_rm = 1;
        if(seg->ctl.shm_nattch == 0 && seg->is_pending_rm) {
            shm::shm_rm(seg);
        }
        break;
    }
    case IPC_STAT:

        if(!buf) { shm::lock.unlock();
            return -EINVAL;
        }

        klibc::memcpy(buf,&seg->ctl,sizeof(shmid_ds));
        break;
    case IPC_SET:

        if(!buf) { shm::lock.unlock();
            return -EINVAL;
        }

        seg->ctl.shm_perm.uid = buf->shm_perm.uid;
        seg->ctl.shm_perm.gid = buf->shm_perm.gid;
        seg->ctl.shm_ctime = time::current_unix_time;
        seg->ctl.shm_perm.mode = buf->shm_perm.mode;
        break;
    default:
        assert(0," meeeeeeeewowww");
        break;
    }

    klibc::debug_printf("shmctl id %d cmd %d buf 0x%p from proc %d\n",shmid,cmd,buf,current->id);

    shm::lock.unlock();
    return 0;
}