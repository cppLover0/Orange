#pragma once
#include <cstdint>

struct __attribute__((packed)) gpt_lba1 {
    std::uint8_t signature[8];
    std::uint32_t revision;
    std::uint32_t size;
    std::uint32_t crc32;
    std::uint32_t reserved;
    std::uint64_t current_lba;
    std::uint64_t other_lba;
    std::uint64_t first_usable;
    std::uint64_t last_usable;
    std::uint8_t guid[16];
    std::uint64_t partition_lba;
    std::uint32_t count_partitions;
    std::uint32_t size_of_partition;
};

struct __attribute__((packed)) gpt_partition_entry {
    std::uint8_t guid[16];
    std::uint8_t unique_guid[16];
    std::uint64_t start_lba;
    std::uint64_t end_lba;
    std::uint64_t attributes;
    std::uint8_t name[72];
};

static_assert(sizeof(gpt_partition_entry) == (0x38 + 72));
static_assert(sizeof(gpt_lba1) == 0x58);

#define GPT_SIGNATURE "EFI PART"