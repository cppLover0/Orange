#pragma once
#include <cstdint>

namespace utf8 {
    inline bool is_continuation(unsigned char c) {
        return (c & 0xC0) == 0x80;
    }

    inline std::size_t sequence_length(unsigned char c) {
        if ((c & 0x80) == 0)    return 1; 
        if ((c & 0xE0) == 0xC0) return 2; 
        if ((c & 0xF0) == 0xE0) return 3; 
        if ((c & 0xF8) == 0xF0) return 4; 
        return 1; 
    }

    std::size_t bytes_to_backspace(const char* buffer, std::size_t size) {
        if (size == 0) return 0;

        std::size_t bytes_to_remove = 1;
        
        while (bytes_to_remove < size && 
               bytes_to_remove < 4 && 
               is_continuation(static_cast<unsigned char>(buffer[size - bytes_to_remove]))) 
        {
            bytes_to_remove++;
        }

        return bytes_to_remove;
    }
}
