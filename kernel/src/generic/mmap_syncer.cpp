#include <generic/scheduling.hpp>
#include <generic/mmap_syncer.hpp>
#include <klibc/stdio.hpp>
#include <generic/arch.hpp>
#include <generic/vmm.hpp>
#include <generic/time.hpp>

void mmap_syncer_worker(void* arg) {
    (void)arg;

    log("mmap_syncer", "im running !");

    while(1) {
        log("mmap_syncer", "time is %lli", time::current_unix_time.load());
        process::yield();
    }
}

void mmap_syncer::init() {
    thread* worker = process::kthread(mmap_syncer_worker, nullptr);
    worker->scheduling_rate = 5 * 1000 * 1000; 
    process::wakeup(worker);
}

void mmap_syncer::sync(vmm_obj* mem) {  
    (void)mem;
    
}
