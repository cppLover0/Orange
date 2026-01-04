
#include <generic/vfs/vfs.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <cstdint>

#pragma once

namespace vfs {
    class fdmanager {
    private:
        arch::x86_64::process_t* proc = 0;
    public:

        userspace_fd_t* fd_list = 0;
        std::uint64_t used_counter = 0;

        fdmanager(arch::x86_64::process_t* proc) {
            this->proc = proc;
            this->used_counter++;
        }

        int create() {
            proc->fd_lock.lock();
            userspace_fd_t* current = fd_list;
            while(current) {
                if(current->state == USERSPACE_FD_STATE_UNUSED && current->index > 2) 
                    break;
                current = current->next;
            }

            if(!current) {
                current = new userspace_fd_t;
                memset(current,0,sizeof(userspace_fd_t));
                current->next = fd_list;
                fd_list = current;
                current->index = *proc->fd_ptr;
                *proc->fd_ptr = *proc->fd_ptr + 1;
            }

            current->state = USERSPACE_FD_STATE_FILE;
            current->read_counter = -1;
            current->write_counter = -1;
            current->can_be_closed = 0;
            current->is_cached_path = 0;
            current->is_debug = 0;
            current->pid = proc->id;
            current->uid = proc->uid;

            proc->fd_lock.unlock();
            return current->index;
        }

        void free() {
            proc->fd_lock.lock();
            this->used_counter--;
            if(!proc->is_shared_fd) {
                userspace_fd_t* fd = this->fd_list;
                while(fd) {
                    if(!fd->can_be_closed) {
                        if(fd->state == USERSPACE_FD_STATE_PIPE)
                            fd->pipe->close(fd->pipe_side);
                        else if(fd->state == USERSPACE_FD_STATE_FILE || fd->state == USERSPACE_FD_STATE_SOCKET)
                            vfs::vfs::close(fd);
                        else if(fd->state == USERSPACE_FD_STATE_EVENTFD)
                            fd->eventfd->close();
                    }
                    userspace_fd_t* next = fd->next;
                    delete (void*)fd;
                    fd = next;
                }
                proc->fd_lock.unlock();
                delete this;
                return;
            }
            proc->fd_lock.unlock();
        }

        void duplicate(fdmanager* dest) {
            userspace_fd_t* fd = this->fd_list;
            while(fd) {
                userspace_fd_t* newfd = new userspace_fd_t;
                memcpy(newfd,fd,sizeof(userspace_fd_t));
                
                if(newfd->state == USERSPACE_FD_STATE_PIPE) {
                    newfd->pipe->create(newfd->pipe_side);
                } else if(newfd->state == USERSPACE_FD_STATE_EVENTFD)
                    newfd->eventfd->create();

                newfd->next = dest->fd_list;
                dest->fd_list = newfd;
                fd = fd->next;
            }

        }

        inline static int create(arch::x86_64::process_t* proc) {

            fdmanager* fd = (fdmanager*)proc->fd;
            return fd->create();

        }

        inline static userspace_fd_t* searchlowest(arch::x86_64::process_t* proc,std::uint32_t idx) {

            if(!proc)
                return 0;

            fdmanager* fd = (fdmanager*)proc->fd;
            userspace_fd_t* current = fd->fd_list;
            userspace_fd_t* lowest = 0;

            while(current) {
                if(current->state == USERSPACE_FD_STATE_UNUSED || current->can_be_closed)  {
                    if(!lowest)
                        lowest = current;
                    if(current->index < lowest->index)
                        lowest = current;
                }
                current = current->next;
            }

            return lowest;
        }

        inline static userspace_fd_t* searchlowestfrom(arch::x86_64::process_t* proc,std::uint32_t idx) {

            if(!proc)
                return 0;

            fdmanager* fd = (fdmanager*)proc->fd;
            userspace_fd_t* current = fd->fd_list;
            userspace_fd_t* lowest = 0;

            while(current) {
                if(current->state == USERSPACE_FD_STATE_UNUSED || current->can_be_closed)  {
                    if(!lowest && current->index >= idx)
                        lowest = current;
                    if(current->index < lowest->index && current->index >= idx)
                        lowest = current;
                }
                current = current->next;
            }

            return lowest;
        }

        inline static userspace_fd_t* search(arch::x86_64::process_t* proc,std::uint32_t idx) {

            if(!proc)
                return 0;

            proc->fd_lock.lock();

            fdmanager* fd = (fdmanager*)proc->fd;
            userspace_fd_t* current = fd->fd_list;
            while(current) {
                if(current->index == idx && current->state != USERSPACE_FD_STATE_UNUSED) { proc->fd_lock.unlock();
                    return current; }
                current = current->next;
            }
            proc->fd_lock.unlock();
            return 0;
        }
    };
}