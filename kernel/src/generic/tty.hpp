#pragma once

#include <cstdint>
#include <generic/scheduling.hpp>
#include <klibc/string.hpp>
#include <utils/linux.hpp>
#include <atomic>
#include <generic/vfs.hpp>

#define set_pgrp 0x5410
#define get_pgrp 21519

struct vt_stat {
    unsigned short v_active;
    unsigned short v_signal; 
    unsigned short v_state;  
};

namespace tty {

    class pipe {
    private:
        
        std::atomic_flag is_received = ATOMIC_FLAG_INIT;
        std::atomic_flag is_n_closed = ATOMIC_FLAG_INIT;
        

    public:

        char* buffer;

        locks::preempt_spinlock lock;

        std::atomic_flag is_closed = ATOMIC_FLAG_INIT;

        std::uint64_t total_size = 0;
        std::atomic<std::int64_t> size = 0;
        std::int64_t write_counter = 0;
        std::int64_t read_counter = 0;

        std::uint32_t connected_to_pipe = 0;
        std::uint32_t connected_to_pipe_write = 0;
        std::uint64_t flags = 0;

        std::uint32_t zero_message_count = 0;
        std::atomic<int> tty_ret = 0;
        termios_t* ttyflags = 0;
        
        int is_closed_socket = 0;

        void* fd_pass = 0;
        pipe(std::uint64_t flags) {
            this->buffer = (char*)(pmm::buddy::alloc(USERSPACE_PIPE_SIZE).phys + etc::hhdm());
            this->total_size = USERSPACE_PIPE_SIZE;
            this->size = 0;
            this->connected_to_pipe = 2; /* syscall which creates pipe should create 2 fds too */ 
            this->connected_to_pipe_write = 1;
            this->flags = flags;

            this->is_closed.clear(std::memory_order_release);

        }

        void close(std::uint8_t side) {
            bool state = this->lock.lock();
            this->connected_to_pipe--;
            
            if(side == PIPE_SIDE_WRITE) {
                this->connected_to_pipe_write--;
                if(this->connected_to_pipe_write == 0) {
                    this->is_received.clear();
                    this->is_closed.test_and_set(std::memory_order_acquire);
                }
            }

            this->lock.unlock(state);

            if(this->connected_to_pipe == 0) {
                delete this;
            }
        }

        void set_tty_ret() {
            this->tty_ret = 1;
        }

        void create(std::uint8_t side) {
            bool state = this->lock.lock();
            this->connected_to_pipe++;
            if(side == PIPE_SIDE_WRITE)
                this->connected_to_pipe_write++;
            this->lock.unlock(state);
        }

        std::uint64_t force_write(const char* src_buffer, std::uint64_t count) {
            if (this->size + count > this->total_size) {
                count = this->total_size - this->size;
            }
            this->size += count;
            klibc::memcpy(this->buffer + (this->size - count), src_buffer, count);
            return count;
        }

        std::uint64_t write(const char* src_buffer, std::uint64_t count) {

            std::uint64_t written = 0;

            while (written < count) {
                bool state = this->lock.lock();

                std::uint64_t space_left = this->total_size - this->size;
                if (space_left == 0) {
                    this->lock.unlock(state);
                    process::yield();
                    continue;
                }

                std::uint64_t to_write = (count - written) < space_left ? (count - written) : space_left;
                if(to_write < 0)
                    to_write = 0;


                force_write(src_buffer + written, to_write);

                written += to_write;

                this->lock.unlock(state);
            }
            return written;
        }

        std::uint64_t ttyread(char* dest_buffer, std::uint64_t count, int is_block) {

            std::uint64_t read_bytes = 0;
            while (true) {
                bool state = this->lock.lock();

                if (this->size == 0) {
                    if (this->is_closed.test(std::memory_order_acquire)) {
                        this->lock.unlock(state);
                        return 0; 
                    }
                    if (flags & O_NONBLOCK || is_block) {
                        this->lock.unlock(state);
                        return 0;
                    }
                    if(ttyflags) {
                        if(!(ttyflags->c_lflag & ICANON) && ttyflags->c_cc[VMIN] == 0) {
                            this->lock.unlock(state);
                            return 0;
                        }
                    }
                    this->lock.unlock(state);
                    process::yield();
                    continue;
                }

                if(ttyflags) {
                    if(!(ttyflags->c_lflag & ICANON)) {
                        if(this->size < ttyflags->c_cc[VMIN]) {
                            this->lock.unlock(state);
                            return 0;
                        }
                    }
                }

                if(ttyflags) {
                    if(ttyflags->c_lflag & ICANON) {
                        if(!tty_ret) {
                            this->lock.unlock(state);
                            process::yield();
                            continue;
                        }
                    }
                }
                
                read_bytes = (count < (std::uint64_t)this->size) ? count : (std::uint64_t)this->size;
                klibc::memcpy(dest_buffer, this->buffer, read_bytes);
                klibc::memmove(this->buffer, this->buffer + read_bytes, this->size - read_bytes);
                this->size -= read_bytes;

                this->write_counter++; 

                if(this->size <= 0) {
                    
                    tty_ret = 0;
                } else
                    this->read_counter++;

                this->lock.unlock(state);
                break;
            }
            return read_bytes;
        }

        ~pipe() {
            pmm::buddy::free((std::uint64_t)this->buffer - etc::hhdm());
        }

    };

    struct tty_arg {
        termios_t termios;
        winsize winsz;
        vfs::pipe* writepipe;
        tty::pipe* readpipe;
        int pgrp;
    };

    void init();
};