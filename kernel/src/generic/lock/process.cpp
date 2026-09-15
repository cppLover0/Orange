
#include <generic/userspace/current_proc.hpp>
#include <generic/lock/process.hpp>

int process::id() {
    return current_proc == nullptr ? 0 : current_proc->id;
}

void process::lock() {

    if(current_proc == nullptr)
        return;

    current_proc->is_holding_lock = true;
}

void process::unlock() {

    if(current_proc == nullptr)
        return;

    current_proc->is_holding_lock = false;
}

long process::last_sys() {
    return current_proc->last_syscall;
}