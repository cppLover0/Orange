
#include <generic/vfs/vfs.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <cstdint>

#pragma once

namespace vfs {
    class fdmanager {
    public:

        inline static int create(arch::x86_64::process_t* proc) {
            userspace_fd_t* current = proc->fd;
            while(current) {
                if(current->state == USERSPACE_FD_STATE_UNUSED) 
                    break;
                current = current->next;
            }

            if(!current) {
                current = new userspace_fd_t;
                zeromem(current);
                current->next = proc->fd;
                proc->fd = current;
                current->index = proc->fd_ptr++;
            }

            current->state = USERSPACE_FD_STATE_FILE;

            return current->index;

        }

        inline static userspace_fd_t* search(arch::x86_64::process_t* proc,std::uint32_t idx) {

            if(!proc)
                return 0;

            userspace_fd_t* current = proc->fd;
            while(current) {
                if(current->index == idx && current->state != USERSPACE_FD_STATE_UNUSED) 
                    return current;
                current = current->next;
            }
            return 0;
        }
    };
}