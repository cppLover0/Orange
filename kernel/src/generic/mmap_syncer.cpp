#include <generic/scheduling.hpp>
#include <generic/mmap_syncer.hpp>
#include <klibc/stdio.hpp>
#include <generic/arch.hpp>
#include <generic/vmm.hpp>
#include <generic/time.hpp>
#include <generic/vfs.hpp>
#include <generic/scheduling.hpp>

void mmap_syncer_worker(void* arg) {
    (void)arg;

    log("mmap_syncer", "im running !");

    while(1) {
        thread* proc = process::_head_proc();
        while(proc) {
            proc->op_lock.lock();
            if(proc->vmem != nullptr && proc->status == PROCESS_LIVE) {
                bool state = proc->vmem->lock.lock();
                vmm_obj* current = proc->vmem->start;

                while(current) {

                    if(current == proc->vmem->end)
                        break;

                    if(current->base != 0) {
                        if(current->mmap_info.copied_file_desc != nullptr) {
                            mmap_syncer::sync(proc->vmem, current);
                        }
                    }

                    current = current->next;
                }

                proc->vmem->lock.unlock(state);
            }
            proc->op_lock.unlock();
            proc = proc->next;
        }
        process::yield();
    }
}

void mmap_syncer::init() {
    thread* worker = process::kthread(mmap_syncer_worker, nullptr);
    worker->scheduling_rate = 5 * 1000 * 1000; 
    process::wakeup(worker);
}

#define CAST_TO_PAGE(x) (ALIGNPAGEDOWN(x) / PAGE_SIZE)

void mmap_syncer::sync(void* vmem, void* mem) {  
    vmm* vm = (vmm*)vmem;
    vmm_obj* me = (vmm_obj*)mem; 
    file_descriptor file1 = *(file_descriptor*)me->mmap_info.copied_file_desc;
    stat s = {};
    file1.vnode.stat(&file1, &s);

    if(me->mmap_info.off >= s.st_size) {
        log("mmap_syncer", "cant sync %s, st_size %lli mmap_offset %lli", file1.path, s.st_size, me->mmap_info.off);
        return;
    }

    s.st_size -= me->mmap_info.off;

    for(std::size_t i = 0; i < me->len; i += PAGE_SIZE) {
        if(arch::is_dirty_address(vm->root, me->base + i)) {
            //log("mmap_syncer", "can sync 0x%p", me->base + i);
            
            file1.offset = me->mmap_info.off + i;
            std::int64_t page = arch::get_phys_from_page(vm->root, me->base + i);
            if(page == 0 || page == -1)
                continue;

            file1.vnode.write(&file1, (void*)(page + etc::hhdm()), CAST_TO_PAGE(i) == CAST_TO_PAGE(s.st_size) ? (s.st_size % PAGE_SIZE) : PAGE_SIZE);

            arch::clear_dirty_bit(vm->root, me->base + i);
        }
    }
}
