#pragma once
#include <cstdint>
#include <klibc/stdio.hpp>

enum class partition_style : std::uint8_t {
    err = 0,
    mbr = 1,
    gpt = 2,
    raw = 5
};

struct disk {
    void* arg;
    std::size_t lba_size;
    std::size_t disk_size;
    bool (*read)(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks);
    bool (*write)(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks);
};

struct bytes_to_block_res {
    std::uint64_t lba;
    std::uint64_t size_in_blocks;
};

inline static bytes_to_block_res bytes_to_blocks(std::uint64_t start, std::uint64_t size, std::uint64_t lba_size) {
    bytes_to_block_res result;
    result.lba = start / lba_size;
    result.size_in_blocks = (size / lba_size) == 0 ? 1 : (size / lba_size);
    return result;
}

namespace drivers {
    void init_disk(disk* new_disk);
};