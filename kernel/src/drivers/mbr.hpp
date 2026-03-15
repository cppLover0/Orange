#pragma once
#include <cstdint>

// mbr headers

#define MBR_ORANGE_TRACE

struct __attribute__((packed)) mbr_partition {
    std::uint8_t  drive_status;     
    std::uint8_t  chs_start[3];     
    std::uint8_t  partition_type;   
    std::uint8_t  chs_end[3];       
    std::uint32_t lba_start;      
    std::uint32_t sector_count;     
};

struct __attribute__((packed)) mbr_sector {
    std::uint8_t bootstrap[446];
    mbr_partition partitions[4];
    std::uint16_t signature;    
};