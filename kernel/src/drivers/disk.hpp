#pragma once
#include <cstdint>

struct disk {
    void* arg;
    std::size_t lba_size;
    std::size_t disk_size;
    bool (*read)(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks);
    bool (*write)(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks);
};