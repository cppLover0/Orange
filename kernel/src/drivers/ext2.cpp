#include <drivers/ext2.hpp>
#include <drivers/disk.hpp>
#include <generic/vfs.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <utils/assert.hpp>
#include <klibc/stdio.hpp>
#include <cstdint>

ext2_inode ext2_get_inode(ext2_partition* partition, std::uint64_t inode_num) {

    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());

    ext2_superblock* sb = partition->sb;
    std::uint32_t group = (inode_num - 1) / sb->s_inodes_per_group;
    std::uint32_t index = (inode_num - 1) % sb->s_inodes_per_group;

    std::uint32_t bgdt_block = (sb->s_log_block_size == 0) ? 2 : 1;
    std::uint32_t block_size = 1024 << sb->s_log_block_size;
    
    bytes_to_block_res result = bytes_to_blocks(block_size * bgdt_block, block_size, partition->target_disk->lba_size);
    partition->target_disk->read(partition->target_disk->arg, buffer, result.lba, result.size_in_blocks);
    
    struct ext2_group_desc *gd = (struct ext2_group_desc *)((std::uint64_t)buffer + group);

    uint32_t inode_size = 128; 
    uint32_t byte_offset = index * inode_size;
    uint32_t block_in_table = gd->bg_inode_table + (byte_offset / block_size);
    uint32_t internal_offset = byte_offset % block_size;

    result = bytes_to_blocks(block_size * block_in_table, block_size, partition->target_disk->lba_size);

    char* inode_buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    partition->target_disk->read(partition->target_disk->arg, inode_buffer, result.lba, result.size_in_blocks);

    pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
    return *(struct ext2_inode*)((std::uint64_t)inode_buffer + internal_offset);
}

void drivers::ext2::init(disk* target_disk, std::uint64_t lba_start) {
    bytes_to_block_res b = bytes_to_blocks(1024, 1024, target_disk->lba_size);
    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    target_disk->read(target_disk->arg, buffer, lba_start + b.lba, b.size_in_blocks);
    ext2_superblock *sb = (ext2_superblock*)buffer;

    assert(sb->s_magic == EXT2_MAGIC,"its not ext2 partition !");
}