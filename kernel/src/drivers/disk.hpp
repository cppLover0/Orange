#pragma once
#include <cstdint>
#include <klibc/stdio.hpp>
#include <utils/align.hpp>

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
    std::uint64_t offset;
};

inline static bytes_to_block_res bytes_to_blocks(std::uint64_t start, std::uint64_t size, std::uint64_t lba_size) {
    bytes_to_block_res result;
    result.lba = ALIGNDOWN(start,lba_size) / lba_size;
    result.size_in_blocks = (ALIGNUP(size,lba_size) / lba_size) == 0 ? 1 : (ALIGNUP(size,lba_size) / lba_size);
    result.offset = start - ALIGNDOWN(start, lba_size);
    return result;
}

namespace drivers {
    void init_disk(disk* new_disk);
};