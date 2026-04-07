#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
 
// https://dr-emann.github.io/squashfs/

struct squashfs_superblock {
    std::uint32_t magic;
    std::uint32_t inode_count;
    std::uint32_t modification_time;
    std::uint32_t block_size;
    std::uint32_t fragment_entry_count;
    std::uint16_t compression_id;
    std::uint16_t block_log;
    std::uint16_t flags;
    std::uint16_t id_count;
    std::uint16_t version_major;
    std::uint16_t version_minor;
    std::uint64_t root_inode_ref;
    std::uint64_t bytes_used;
    std::uint64_t id_table_start;
    std::uint64_t xattr_id_table_start;
    std::uint64_t inode_table_start;
    std::uint64_t directory_table_start;
    std::uint64_t fragment_table_start;
    std::uint64_t export_table_start;
};

struct squashfs_dir_header {
    uint32_t count;         
    uint32_t start_block;   
    uint32_t inode_number;   
};

struct squashfs_dir_entry {
    uint16_t offset;        
    int16_t  inode_offset;
    uint16_t type;         
    uint16_t size;        
};

#define SQUASHFS_MAGIC 0x73717368

namespace squashfs {
    void init(char* squashfs_archiv, int len, vfs::node* node);
    bool is_valid(char* archive, int len);
}