#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/hhdm.hpp>
#include <generic/pmm.hpp>
#include <drivers/mbr.hpp>
#include <utils/assert.hpp>
#include <klibc/string.hpp>
#include <drivers/gpt.hpp>
#include <drivers/ext2.hpp>

std::uint8_t linux_fs_signature[16] = {
    0xAF, 0x3D, 0xC6, 0x0F,
    0x83, 0x84,            
    0x72, 0x47,           
    0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 
};

const char* disk_type_to_str(partition_style disk_type) {
    switch(disk_type) {
    case partition_style::err:
        return "Unknown";
    case partition_style::mbr:
        return "MBR";
    case partition_style::gpt:
        return "GPT";
    case partition_style::raw:
        return "Raw";
    }
}

partition_style determine_disk_type(disk* target_disk) {

    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    target_disk->read(target_disk->arg, (char*)buffer, 1, 1);
    
    if(!klibc::memcmp((const char*)buffer, GPT_SIGNATURE, sizeof(GPT_SIGNATURE))) {
        pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
        return partition_style::gpt;
    }

    target_disk->read(target_disk->arg, (char*)buffer, 0, 1);

    if(((mbr_sector*)buffer)->signature == 0xAA55) {
        pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
        return partition_style::mbr;
    }

    assert(0, "Unknown partition style");
    return partition_style::err;
}

bool disk_selftest(disk* target_disk) {
    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    if(target_disk->read(target_disk->arg, buffer, 0, 1) == true) {
        pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
        return true;
    } else {
        klibc::printf("Disk: Failed to do disk selftest\r\n");
        return false;
    }
}

const char* mbr_to_str(std::uint8_t type) {
    switch (type)
    {
        case 0x83:
            return "linux";
        case 0xb:
            return "fat32";
        default:
            return "unknown";
    }
    return "unknown";
}

bool disk_try_linux(disk* new_disk) {
    bytes_to_block_res b = bytes_to_blocks(1024, 1024, new_disk->lba_size);

    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_disk->read(new_disk->arg, buffer, b.lba, b.size_in_blocks);
    ext2_superblock *sb = (ext2_superblock*)((std::uint64_t)buffer + b.offset);

    if (sb->s_magic == EXT2_MAGIC) {
#ifdef MBR_ORANGE_TRACE
        klibc::printf("Disk: Detected ext2\r\n");
#endif
        drivers::ext2::init(new_disk, 0);
        pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
        return true;
    } else {
        pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
        return false;
    }
    return false;
}

void disk_do_mbr_linux(disk* new_disk, std::uint64_t start_lba) {

    bytes_to_block_res b = bytes_to_blocks(1024, 1024, new_disk->lba_size);

    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_disk->read(new_disk->arg, buffer, start_lba + b.lba, b.size_in_blocks);
    ext2_superblock *sb = (ext2_superblock*)((std::uint64_t)buffer + b.offset);

    if (sb->s_magic == EXT2_MAGIC) {
#ifdef MBR_ORANGE_TRACE
        klibc::printf("Disk: Detected ext2\r\n");
#endif
        drivers::ext2::init(new_disk, start_lba);
    }
    pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
}

void disk_do_mbr(disk* new_disk) {
    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_disk->read(new_disk->arg, (char*)buffer, 0, 1);

    mbr_sector* mbr = (mbr_sector*)buffer;

    for(int i = 0; i < 4; i++) {
        if(mbr->partitions[i].partition_type != 0) {
#ifdef MBR_ORANGE_TRACE
            klibc::printf("Disk: Detected mbr partition type %s (%d), lba_start %lli\r\n", mbr_to_str(mbr->partitions[i].partition_type), mbr->partitions[i].partition_type, mbr->partitions[i].lba_start);
#endif
            if(mbr->partitions[i].partition_type == 0x83) {
                disk_do_mbr_linux(new_disk, mbr->partitions[i].lba_start);
            }
        }
    }
    
    pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
}

void print_gpt_guid(uint8_t *g) {

    klibc::printf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\r\n",
           g[3], g[2], g[1], g[0],   
           g[5], g[4],               
           g[7], g[6],              
           g[8], g[9],              
           g[10], g[11], g[12],    
           g[13], g[14], g[15]);
}

void disk_do_gpt(disk* new_disk) {
    char* buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_disk->read(new_disk->arg, (char*)buffer, 1, 1);
    gpt_lba1 lba1 = {};
    klibc::memcpy(&lba1, buffer, sizeof(gpt_lba1));
    new_disk->read(new_disk->arg, (char*)buffer, lba1.partition_lba, 1);

    gpt_partition_entry* part_table = (gpt_partition_entry*)buffer;
    for(std::size_t i = 0;i < lba1.count_partitions; i++) {
        gpt_partition_entry entry = part_table[i];
        if(klibc::memcmp(entry.guid, (uint8_t[16]){0}, 16) == 0) continue;
#ifdef MBR_ORANGE_TRACE
        klibc::printf("gpt: detected fs with start_lba %lli with type ",entry.start_lba);
        print_gpt_guid(entry.guid);
#endif
        if(klibc::memcmp(entry.guid, linux_fs_signature, 16) == 0) {
#ifdef MBR_ORANGE_TRACE
            klibc::printf("gpt: detected linux fs\r\n");
#endif
            disk_do_mbr_linux(new_disk, entry.start_lba);
        }
    }

    pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
}

void drivers::init_disk(disk* new_disk) {
    if(disk_selftest(new_disk) == false)
        return;

    partition_style style = determine_disk_type(new_disk);
    klibc::printf("Disk: Detected %s partition style\r\n", disk_type_to_str(style));

    switch (style)
    {
    case partition_style::mbr:
        disk_do_mbr(new_disk);
        break;
    case partition_style::gpt:
        disk_do_gpt(new_disk);
        break;
    default:
        bool status = disk_try_linux(new_disk);
        if(status == false)
            assert(0,"unsupported disk\r\n");
        break;
    }
}