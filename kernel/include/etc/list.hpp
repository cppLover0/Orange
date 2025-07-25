#include <cstdint>
#include <atomic>
#include <generic/locks/spinlock.hpp>
#include <etc/libc.hpp>
#include <etc/etc.hpp>

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

        Lists::list_object_t* list;
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

        void insert(std::uint64_t value) {
            list_lock->lock();

            Lists::list_object_t* new_list = find_free();

            if(!new_list)
                new_list = new Lists::list_object_t;

            new_list->is_used = 1;
            new_list->value = value;
            new_list->next_list_object = (std::uint64_t)list;
            list = new_list;

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
};