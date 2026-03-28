#pragma once

#include <cstdint>
#include <klibc/string.hpp>
#include <generic/lock/spinlock.hpp>

#include <atomic>

namespace utils {
    template <typename T>
    struct buffer_obj_t {
        std::uint32_t cycle;
        T data;
    };

    template <typename T>
    class ring_buffer {
    private:
        buffer_obj_t<T>* objs;
        int tail;
        int size;
        int cycle;

        locks::preempt_spinlock lock;

    public:
        explicit ring_buffer(std::size_t size_elements) {
            size = static_cast<int>(size_elements);
            objs = new buffer_obj_t<T>[size];
            tail = 0;
            cycle = 1;
            
            klibc::memset(objs, 0, sizeof(buffer_obj_t<T>) * size);
        }

        ~ring_buffer() {
            delete[] objs;
        }

        ring_buffer(const ring_buffer&) = delete;
        ring_buffer& operator=(const ring_buffer&) = delete;

        void send(const T& item) {
            bool state = lock.lock();
            
            objs[tail].cycle = cycle;
            objs[tail].data = item;

            write_counter++;

            if (++tail == size) {
                tail = 0;
                cycle = !cycle;
            }
            
            lock.unlock(state);
        }

        bool is_not_empty(int reader_queue, int reader_cycle) const {
            return objs[reader_queue].cycle == (std::uint32_t)reader_cycle;
        }

        int receive(T* out, int max, int* c_io, int* q_io) {
            bool state = lock.lock();
            
            int count = 0;
            int q = *q_io;
            int c = *c_io;

            while (count < max && objs[q].cycle == (std::uint32_t)c) {
                out[count] = objs[q].data;
                read_counter++;

                if (++q == size) {
                    q = 0;
                    c = !c;
                }
                count += sizeof(T);
            }

            *q_io = q;
            *c_io = c;
            
            lock.unlock(state);
            return count;
        }

        std::uint64_t get_write_count() const { return write_counter; }
    };

}