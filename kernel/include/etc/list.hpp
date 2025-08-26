#include <cstdint>
#include <atomic>
#include <generic/locks/spinlock.hpp>
#include <etc/libc.hpp>
#include <etc/etc.hpp>

#include <etc/logging.hpp>

#pragma once

namespace Lists {
    
    typedef struct list_object {
        std::uint64_t value;
        std::uint8_t is_used;
        std::uint64_t next_list_object; /* Pointer to next list_object */
    } __attribute__((packed)) list_object_t;

    class Bitmap {
    private:
        std::uint8_t* pool;
        std::uint32_t pool_size;
    public:
        
        Bitmap(std::uint32_t size) {
            pool_size = (ALIGNUP(size,8) / 8) + 1;
            pool = (std::uint8_t*)malloc(pool_size);
            memset(pool,0,pool_size);
        }

        std::uint8_t test(std::uint32_t idx) {
            return pool[ALIGNDOWN(idx,8) / 8] & (1 << (idx - ALIGNDOWN(idx,8)));
        }

        void clear(std::uint32_t idx) {
            pool[ALIGNDOWN(idx,8) / 8] &= ~(1 << (idx - ALIGNDOWN(idx,8)));
        }

        void set(std::uint32_t idx) {
            pool[ALIGNDOWN(idx,8) / 8] |= (1 << (idx - ALIGNDOWN(idx,8)));
        }

    };
    
    class List {
    private:

        locks::spinlock* list_lock;

        std::uint8_t is_lock_possible;
        
        static std::uint64_t normalize(std::uint64_t value) {
            return value;
        }

        static Lists::list_object_t* iterate(Lists::list_object_t* last) {
            return (Lists::list_object_t*)normalize(last->next_list_object);
        }

        Lists::list_object_t* find_free() {
            Lists::list_object_t* current = list;
            while(current) {
                if(!current->is_used)
                    return current;
                current = iterate(current);
            }
            return 0;
        }

    public:
        Lists::list_object_t* list;

        void lock() {
            list_lock->lock();
        }

        void unlock() {
            list_lock->unlock();
        }

        void insert(std::uint64_t value) {
            list_lock->lock();

            Lists::list_object_t* new_list = find_free();

            if(!new_list) {
                new_list = new Lists::list_object_t;
                new_list->next_list_object = (std::uint64_t)list;
                list = new_list;
            }
                
            new_list->is_used = 1;
            new_list->value = value;

            list_lock->unlock();
        }

        void remove(std::uint64_t value) {
            list_lock->lock();

            Lists::list_object_t* current = list;

            while(current) {
                if(current->value == value) {
                    current->is_used = 0;
                    current->value = 0;
                }
                current = iterate(current);
            }

            list_lock->unlock();
        }

        List() {
            list = new Lists::list_object_t;
            list_lock = new locks::spinlock();
        }
    };

    typedef struct {
        std::uint32_t cycle;
        std::uint64_t value1;
        
    } ring_obj_t;

    typedef struct {
        ring_obj_t* objs;
        int bytelen;
        int tail;
        int size;
        int cycle;
    } ring_buffer_t;


    class Ring {
    private:
    public:
        ring_buffer_t ring;

        locks::spinlock ring_lock;

        void send(std::uint64_t value1) {
            ring_lock.lock();
            ring.objs[ring.tail].cycle = ring.cycle;
            ring.objs[ring.tail].value1 = value1;

            if (++ring.tail == ring.size) {
                ring.tail = 0;
                ring.cycle = !ring.cycle;
            }
            ring_lock.unlock();
        }

        int setup_bytelen(int len) {
            ring.bytelen = len;
            return len;
        }

        int receivevals(void* out, int id, int max, std::uint8_t* cycle, std::uint32_t* queue) {
            ring_lock.lock();
            int len = 0;
            while (ring.objs[*queue].cycle == *cycle && len * ring.bytelen < max) {
                if (ring.bytelen == 1) {
                    ((char*)out)[len] = ring.objs[*queue].value1 & 0xFF;
                } else if (ring.bytelen == 2) {
                    ((short*)out)[len] = ring.objs[*queue].value1 & 0xFFFF;
                } else if (ring.bytelen == 4) {
                    ((int*)out)[len] = ring.objs[*queue].value1 & 0xFFFFFFFF;
                } else if (ring.bytelen == 8) {
                    ((long long*)out)[len] = ring.objs[*queue].value1;
                }
                (*queue)++;
                if (*queue == ring.size) {
                    *queue = 0;
                    *cycle = !(*cycle);
                }
                len++;
            }
            ring_lock.unlock();
            return len * ring.bytelen;
        }

        Ring(std::size_t size_elements) {
            ring.objs = new ring_obj_t[size_elements];
            ring.size = size_elements;
            ring.tail = 0;
            ring.cycle = 1;
            ring.bytelen = 0;
            ring_lock.unlock();
            memset(ring.objs, 0, sizeof(ring_obj_t) * size_elements);
        }

        ~Ring() {
            delete[] ring.objs;
        }

    };

};