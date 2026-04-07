#pragma once

// i told clanker to write it :P

#include <cstdint>
#include <klibc/string.hpp>

class cmdline_parser {
public:
    cmdline_parser(const char* buf) : buffer(buf) {}

    bool find(const char* key, char* out_value, std::size_t max_len) {
        if (!buffer || !key) return false;

        const char* ptr = buffer;
        std::size_t key_len = klibc::strlen(key);

        while (*ptr) {
            while (*ptr == ' ') ptr++;
            if (*ptr == '\0') break;

            const char* start = ptr;
            while (*ptr && *ptr != ' ' && *ptr != '=') ptr++;

            std::size_t len = ptr - start;

            if (len == key_len && klibc::memcmp(start, key, len) == 0) {
                if (*ptr == '=') {
                    ptr++;
                    const char* val_start = ptr;

                    if (*ptr == '"') {
                        ptr++;
                        val_start = ptr;
                        while (*ptr && *ptr != '"') ptr++;
                    } else {
                        while (*ptr && *ptr != ' ') ptr++;
                    }

                    std::size_t val_len = ptr - val_start;
                    if (val_len >= max_len) val_len = max_len - 1;

                    klibc::memcpy(out_value, val_start, val_len);
                    out_value[val_len] = '\0';
                    return true;
                } else {
                    out_value[0] = '\0';
                    return true;
                }
            }

            while (*ptr && *ptr != ' ') ptr++;
        }

        return false;
    }

    bool contains(const char* key) {
        char dummy[1];
        return find(key, dummy, 1);
    }

private:
    const char* buffer;
};

#include <generic/bootloader/bootloader.hpp>

namespace cmdline {
    inline cmdline_parser* parser;

    inline void init() {
        parser = new cmdline_parser((const char*)bootloader::bootloader->get_cmdline());
    }
}