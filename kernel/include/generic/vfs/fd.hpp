
#include <generic/vfs/vfs.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <etc/list.hpp>

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

        int find_min_free_index() {
            int max_index = -1;
            
            userspace_fd_t* current = fd_list;
            while(current) {
                if(current->index > max_index) {
                    max_index = current->index;
                }
                current = current->next;
            }
            
            if(max_index < 3) {
                return 3;
            }
            
            bool used_indices[max_index + 2];
            memset(used_indices, 0, max_index + 2);
            
            current = fd_list;
            while(current) {
                if(current->index >= 0 && current->index <= max_index && 
                   current->state != USERSPACE_FD_STATE_UNUSED) {
                    used_indices[current->index] = true;
                }
                current = current->next;
            }
            
            int free_index = 3;
            for(; free_index <= max_index + 1; free_index++) {
                if(!used_indices[free_index]) {
                    break;
                }
            }
            
            return free_index;
        }

        void close(int idx) {
            proc->fd_lock.lock();
            fdmanager* fd = (fdmanager*)proc->fd;
            userspace_fd_t* current = fd->fd_list;
            userspace_fd_t* prev = 0;
            while(current) {
                if(current->index == idx && current->state != USERSPACE_FD_STATE_UNUSED) { //proc->fd_lock.unlock();
                    break; }
                prev = current;
                current = current->next;
            }

            if(!current) {
                
                proc->fd_lock.unlock();
                return; 
            }

            if(current == fd->fd_list) 
                fd->fd_list = current->next;
            else if(prev)
                prev->next = current->next;

            // current->next = 0;
            // memory::pmm::_virtual::free((void*)current);

            proc->fd_lock.unlock();
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
                current = (userspace_fd_t*)memory::pmm::_virtual::alloc(4096);
                memset(current,0,sizeof(userspace_fd_t));
                current->next = fd_list;
                fd_list = current;
                while(1) {
                    if(search(proc,*proc->fd_ptr)) {
                        *proc->fd_ptr = *proc->fd_ptr + 1;
                    } else
                        break;
                }
                current->index = *proc->fd_ptr;
                *proc->fd_ptr = *proc->fd_ptr + 1;
            }

            current->state = USERSPACE_FD_STATE_FILE;
            current->read_counter = -1;
            current->write_counter = -1;
            current->can_be_closed = 0;
            current->is_cached_path = 0;
            current->is_debug = 0;
            current->is_cloexec = 0;
            current->pid = proc->id;
            current->uid = proc->uid;

            proc->fd_lock.unlock();
            return current->index;
        }

        void free() {
            proc->fd_lock.lock();
            this->used_counter--;
            if(this->used_counter == 0) {
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
                    memory::pmm::_virtual::free((void*)fd);
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
                userspace_fd_t* newfd = (userspace_fd_t*)memory::pmm::_virtual::alloc(4096);
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

        void cloexec() {
            userspace_fd_t* fd = this->fd_list;
            while(fd) {
                userspace_fd_t* fd_s = fd;
                if(fd_s->is_cloexec) {
                    if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
                        fd_s->pipe->close(fd_s->pipe_side);
                    } else if(fd_s->state == USERSPACE_FD_STATE_FILE || fd_s->state == USERSPACE_FD_STATE_SOCKET)
                        vfs::vfs::close(fd_s); 
                    else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD)
                        fd_s->eventfd->close();

                    if(!fd_s->is_a_tty && fd_s->index > 2)
                        fd_s->state = USERSPACE_FD_STATE_UNUSED;
                }
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
                    else if(current->index < lowest->index && current->index >= idx)
                        lowest = current;
                }
                current = current->next;
            }

            return lowest;
        }

        inline static userspace_fd_t* search(arch::x86_64::process_t* proc,std::uint32_t idx) {

            if(!proc)
                return 0;

            //proc->fd_lock.lock();

            fdmanager* fd = (fdmanager*)proc->fd;
            userspace_fd_t* current = fd->fd_list;
            while(current) {
                if(current->index == idx && current->state != USERSPACE_FD_STATE_UNUSED) { //proc->fd_lock.unlock();
                    return current; }
                current = current->next;
            }
            //proc->fd_lock.unlock();
            return 0;
        }
    };
}