#include <drivers/ext2.hpp>
#include <drivers/disk.hpp>
#include <generic/vfs.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <utils/assert.hpp>
#include <klibc/stdio.hpp>
#include <utils/math.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <cstdint>

void drivers::ext2::init(disk* target_disk, std::uint64_t lba_start) {
    bytes_to_block_res b = bytes_to_blocks(1024, 1024, target_disk->lba_size);
    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    target_disk->read(target_disk->arg, buffer, lba_start + b.lba, b.size_in_blocks);
    ext2_superblock *sb = (ext2_superblock*)((std::uint64_t)buffer + b.offset);

    assert(sb->s_magic == EXT2_MAGIC,"its not ext2 partition !");

    if((1024 << sb->s_log_block_size) > PAGE_SIZE) {
        klibc::printf("ext2: partition block size is bigger than page_size ! (todo mb) \r\n");
        return;
    }

    if(sb->revision >= 1) {
        log("ext2", "detected features %s %s %s",(sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE) ? "EXT2_FEATURE_RO_COMPAT_LARGE_FILE" : "", (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? "EXT4_FEATURE_INCOMPAT_64BIT" : "", (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) ? "EXT4_FEATURE_INCOMPAT_EXTENTS" : "");
    }

}