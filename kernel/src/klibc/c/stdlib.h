#pragma once

#include <klibc/string.hpp>
#include <klibc/stdlib.hpp>

using namespace klibc;

#define realloc(ptr, new_size) \
    ({ \
        void *__new_ptr = NULL; \
        size_t __new_size = (new_size); \
        \
        if (__new_size == 0) { \
            /* Если размер 0 - free и возвращаем NULL */ \
            if (ptr) free(ptr); \
            __new_ptr = NULL; \
        } else { \
            /* Выделяем новую память */ \
            __new_ptr = malloc(__new_size); \
            if (__new_ptr && ptr) { \
                /* Копируем старые данные (размер неизвестен, копируем сколько влезет) */ \
                /* Примечание: old_size неизвестен, поэтому копируем весь новый размер */ \
                /* Это безопасно только если старый размер >= нового */ \
                memcpy(__new_ptr, ptr, __new_size); \
            } \
            /* Освобождаем старый указатель */ \
            if (ptr) { \
                free(ptr); \
            } \
        } \
        __new_ptr; \
    })