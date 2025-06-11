
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <generic/locks/spinlock.hpp>
#include <generic/memory/pmm.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/debug.hpp>
#include <other/string.hpp>

char fd_lock = 0;

int FD::Create(process_t* proc,char is_pipe) {

    spinlock_lock(&fd_lock);

    // search for non allocated fd

    fd_t* last = (fd_t*)proc->last_fd;

    fd_t* current = (fd_t*)proc->start_fd;

    // char is_success = 0;

    // while(current) {
    //     if(current->type == FD_NONE) {
    //         is_success = 1;
    //         break;
    //     }
    //     current = current->next;
    // }

    current = (fd_t*)PMM::VirtualAlloc();
    current->index = last->index + 1;
    last->next = current;
    current->parent = last;
    proc->last_fd = (char*)current;

    current->pipe.buffer = 0;
    current->pipe.buffer_size = 0;
    current->pipe.is_received = 0;
    current->pipe.type = PIPE_WAIT;
    current->is_pipe_pointer = 0;

    current->pipe.is_used = 0;

    String::memset(&current->pipe.termios,0,sizeof(termios_t));
    
    current->type = is_pipe ? FD_PIPE : FD_FILE;
    current->proc = proc;
    current->seek_offset = 0;

    if(is_pipe) {
        current->pipe.buffer = (char*)PMM::VirtualBigAlloc(16);
        String::memset(current->pipe.buffer,0,16 * 4096);
    }
    
    spinlock_unlock(&fd_lock);
    return current->index;

}

fd_t* FD::Search(process_t* proc,int index) {
    spinlock_lock(&fd_lock);
    fd_t* current = (fd_t*)proc->start_fd;
    while(current) {
        if(current->index == index) {
            spinlock_unlock(&fd_lock);
            return current;
        }
        current = current->next;
    }
    spinlock_unlock(&fd_lock);
    return 0;
}