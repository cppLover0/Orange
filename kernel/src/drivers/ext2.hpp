#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>

struct ext2_superblock {
    uint32_t s_inodes_count;      
    uint32_t s_blocks_count;      
    uint32_t s_r_blocks_count;    
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;  
    uint32_t s_log_block_size;   
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;             
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
};

struct ext2_group_desc {
    uint32_t bg_block_bitmap;   
    uint32_t bg_inode_bitmap;     
    uint32_t bg_inode_table;       
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
};

struct ext2_inode {
    uint16_t i_mode;       
    uint16_t i_uid;       
    uint32_t i_size;       
    uint32_t i_atime;      
    uint32_t i_ctime;     
    uint32_t i_mtime;      
    uint32_t i_dtime;     
    uint16_t i_gid;         
    uint16_t i_links_count; 
    uint32_t i_blocks;     
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];  
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
};

struct ext2_dir_entry {
    uint32_t inode;       
    uint16_t rec_len;     
    uint8_t name_len;    
    uint8_t file_type;    
    char name[];      
};

struct ext2_partition {
    std::uint64_t lba_start;
    ext2_superblock* sb;
    locks::mutex lock;
    disk* target_disk;
    char* buffer;
};

#define EXT2_MAGIC 0xEF53

namespace drivers {
    namespace ext2 {
        void init(disk* target_disk, std::uint64_t lba_start);
    }
}