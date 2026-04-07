#pragma once

#include <utils/align.hpp>
#include <klibc/string.hpp>
#include <cstdint>

#define PUT_STACK(dest,value) *--dest = value;
#define PUT_STACK_STRING(dest,string) klibc::memcpy(dest,string,klibc::strlen(string)); 

namespace stackmgr {

    inline uint64_t memcpy(std::uint64_t stack, void* src, uint64_t len) {
        std::uint64_t _stack = stack;
        _stack -= 8;
        _stack -= ALIGNUP(len,8);
        klibc::memcpy((void*)_stack,src,len);
        return (std::uint64_t)_stack;
    }

    inline uint64_t* copy_to_stack(char** arr,uint64_t* stack,char** out, uint64_t len) {

        uint64_t* temp_stack = stack;

        for(uint64_t i = 0;i < len; i++) {
            temp_stack -= ALIGNUP(klibc::strlen(arr[i]),8);
            PUT_STACK_STRING(temp_stack,arr[i])
            out[i] = (char*)temp_stack;
            PUT_STACK(temp_stack,0);
        }

        return temp_stack;

    }

    inline uint64_t* copy_to_stack_no_out(uint64_t* arr,uint64_t* stack,uint64_t len) {

        uint64_t* _stack = stack;

        for(uint64_t i = 0;i < len;i++) {
            PUT_STACK(_stack,arr[i]);
        }   

        return _stack;

    }
}