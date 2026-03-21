#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>

//#define EXT2_ORANGE_TRACE

#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE 0x0002
#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040

#include <stdint.h>

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
    uint32_t s_lastcheck;      
    uint32_t s_checkinterval;   
    uint32_t s_creator_os;     
    uint32_t revision;      
    uint16_t s_def_resuid;     
    uint16_t s_def_resgid;    

    uint32_t s_first_ino;       
    uint16_t inode_size;     
    uint16_t s_block_group_nr; 
    uint32_t s_feature_compat;  
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];      
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;   

    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;

    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;

    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;       
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
} __attribute__((packed));


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
    void* cached_group;
    locks::mutex lock;
    disk* target_disk;
    char* temp_buffer2;
    char* buffer;
};

struct ext4_extent_header {
    uint16_t eh_magic;     
    uint16_t eh_entries;   
    uint16_t eh_max;       
    uint16_t eh_depth;    
    uint32_t eh_generation;
};

struct ext4_extent {
    uint32_t ee_block;    
    uint16_t ee_len;   
    uint16_t ee_start_hi;  
    uint32_t ee_start_lo;  
};

struct ext4_extent_idx {
    uint32_t ei_block;    
    uint32_t ei_leaf_lo;   
    uint16_t ei_leaf_hi;   
    uint16_t ei_unused;
};

#define EXT2_MAGIC 0xEF53

namespace drivers {
    namespace ext2 {
        void init(disk* target_disk, std::uint64_t lba_start);
    }
}