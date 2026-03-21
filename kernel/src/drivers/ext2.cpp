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

inline static std::uint64_t inode_to_block_group(ext2_superblock* sb, std::uint64_t inode) {
    return (inode - 1) / sb->s_inodes_per_group;
}

inline static std::uint64_t inode_size(ext2_superblock* sb) {
    return sb->revision >= 1 ? sb->inode_size : 128;
}

inline static std::uint64_t inode_to_index(ext2_superblock* sb, std::uint64_t inode) {
    return div_remain(inode - 1, sb->s_inodes_per_group);
}

inline static std::uint64_t inode_to_block(ext2_superblock* sb, std::uint64_t inode) {
    std::uint64_t index = inode_to_index(sb, inode);
    return (index * inode_size(sb)) / (1024 << sb->s_log_block_size);
}

inline static std::uint64_t ext2_blocks_count(ext2_superblock* sb) {
    if(sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        return sb->s_blocks_count | ((std::uint64_t)sb->s_blocks_count_hi << 32);
    } else 
        return sb->s_blocks_count;
    return 0;
}


inline static bool ext2_test_bit(std::uint8_t* bitmap, std::uint32_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

inline static void ext2_set_bit(std::uint8_t* bitmap, std::uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

inline static void ext2_clear_bit(std::uint8_t* bitmap, std::uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

ext2_inode ext2_get_inode(ext2_partition* partition, std::uint64_t inode_num, std::int32_t* status = nullptr) {
    ext2_superblock* sb = partition->sb;
    std::uint32_t block_size = 1024 << sb->s_log_block_size;

    std::uint64_t group = inode_to_block_group(sb, inode_num);

    char* bgdt = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());

    bytes_to_block_res res;

    ext2_group_desc* gd = (ext2_group_desc*)((std::uint64_t)partition->cached_group + group * sizeof(ext2_group_desc));
    uint32_t inode_table_block = gd->bg_inode_table;

    uint32_t inode_byte_offset = inode_to_index(sb, inode_num) * inode_size(sb);

    uint32_t final_inode_block = gd->bg_inode_table + (inode_byte_offset / block_size);
    uint32_t final_offset_in_block = div_remain(inode_byte_offset, block_size);

    res = bytes_to_blocks(final_inode_block * block_size, block_size, partition->target_disk->lba_size);
    partition->target_disk->read(partition->target_disk->arg, bgdt, res.lba + partition->lba_start, res.size_in_blocks);

    ext2_inode actual_inode = {};
    klibc::memcpy(&actual_inode, bgdt + res.offset + final_offset_in_block, sizeof(ext2_inode));

#ifdef EXT2_ORANGE_TRACE
    klibc::printf("ext2: inode_size %lli inode_to_block_group %lli inode_to_block %lli index %lli inode %lli inode per group %lli bgdt start %lli block size %lli block offset %lli block internal offset %lli inode_table_block %lli byte offset %lli inode block %lli offset inode in block %lli\r\n",inode_size(sb), inode_to_block_group(partition->sb,inode_num),inode_to_block(sb, inode_num), inode_to_index(sb, inode_num), inode_num, sb->s_inodes_per_group,bgdt_start_block, block_size, 0, 0, inode_table_block, inode_byte_offset, final_inode_block, final_offset_in_block);
#else
    (void)inode_to_block;
    (void)inode_table_block;
#endif

    pmm::freelist::free((std::uint64_t)bgdt - etc::hhdm());

    if(status != nullptr)
        *status = 0;

    return actual_inode;
}

uint32_t read_indirect_ptr(ext2_partition* partition, std::uint64_t table_block, std::uint64_t index) {
    if (table_block == 0) return 0;
    std::uint32_t* buffer = (std::uint32_t*)(pmm::freelist::alloc_4k() + etc::hhdm());
    std::uint64_t saved_index = 0;
    std::uint64_t block_size = 1024 << partition->sb->s_log_block_size;
    bytes_to_block_res b = bytes_to_blocks(table_block * block_size, block_size / 4,  partition->target_disk->lba_size);
    partition->target_disk->read(partition->target_disk->arg, (char*)buffer, b.lba + partition->lba_start, b.size_in_blocks);
    std::uint32_t* table = (std::uint32_t*)((std::uint64_t)buffer + b.offset);
    saved_index = table[index];
    pmm::freelist::free((std::uint64_t)buffer - etc::hhdm());
    return saved_index;
}

std::uint64_t get_extent_address(uint16_t hi, uint32_t lo) {
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

void ext2_read_block(ext2_partition* partition, std::uint64_t block, char* buffer) {
    bytes_to_block_res b = bytes_to_blocks(block * (1024 << partition->sb->s_log_block_size), 1024 << partition->sb->s_log_block_size,  partition->target_disk->lba_size);
    assert(b.offset == 0, "uhhhhh r :(");
    partition->target_disk->read(partition->target_disk->arg, buffer, b.lba + partition->lba_start, b.size_in_blocks);
}

void ext2_memcpy_block(ext2_partition* part, std::uint64_t block, void* buffer, std::size_t c) {
    ext2_read_block(part, block, part->temp_buffer2);
    klibc::memcpy(buffer, part->temp_buffer2, c);
}

void ext2_write_block(ext2_partition* partition, std::uint64_t block, char* buffer) {
    bytes_to_block_res b = bytes_to_blocks(block * (1024 << partition->sb->s_log_block_size), 1024 << partition->sb->s_log_block_size,  partition->target_disk->lba_size);
    assert(b.offset == 0, "uhhhhh w :(");
    partition->target_disk->write(partition->target_disk->arg, buffer + b.offset, b.lba + partition->lba_start, b.size_in_blocks);
}

std::uint64_t ext4_get_phys_block_extents(ext2_partition* part, ext4_extent_header* header, std::uint32_t logical_block) {

    assert(header->eh_magic == 0xF30A, "uhh :((");
    if (header->eh_magic != 0xF30A) return 0;

    if (header->eh_depth > 0) {
        ext4_extent_idx* idx = (ext4_extent_idx*)((std::uint8_t*)header + sizeof(ext4_extent_header));
        int target_idx = -1;

        for (int i = 0; i < header->eh_entries; i++) {
            if (logical_block >= idx[i].ei_block) {
                target_idx = i;
            } else {
                break; 
            }
        }

        if (target_idx == -1) return 0;

        std::uint64_t next_node_phys = get_extent_address(idx[target_idx].ei_leaf_hi, idx[target_idx].ei_leaf_lo);
        
        std::uint32_t block_size = 1024 << part->sb->s_log_block_size;
        char* local_buf = new char[block_size];
        ext2_memcpy_block(part, next_node_phys, local_buf, block_size);
        
        std::uint64_t res = ext4_get_phys_block_extents(part, (ext4_extent_header*)local_buf, logical_block);
        delete[] local_buf;
        return res;
    } else {
        ext4_extent* ext = (ext4_extent*)((std::uint8_t*)header + sizeof(ext4_extent_header));
        
        for (int i = 0; i < header->eh_entries; i++) {
            std::uint32_t start_lblock = ext[i].ee_block;
            std::uint32_t len = ext[i].ee_len;
            if (len > 32768) len -= 32768;

            if (logical_block >= start_lblock && logical_block < start_lblock + len) {
                std::uint64_t phys_start = get_extent_address(ext[i].ee_start_hi, ext[i].ee_start_lo);
                return phys_start + (logical_block - start_lblock);
            }
        }
    }

    return 0; 
}


std::uint64_t get_phys_block(ext2_partition* partition, ext2_inode *node, std::uint32_t logical_block) {

    if(partition->sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) {
        return ext4_get_phys_block_extents(partition, (ext4_extent_header*)node->i_block, logical_block);
    }

    std::uint32_t block_size = 1024 << partition->sb->s_log_block_size;
    std::uint64_t ptrs_per_block = block_size / 4; 

    if (logical_block < 12) {
        return node->i_block[logical_block];
    }
    logical_block -= 12;

    if (logical_block < ptrs_per_block) {
        return read_indirect_ptr(partition, node->i_block[12], logical_block);
    }
    logical_block -= ptrs_per_block;

    std::uint64_t doubly_limit = ptrs_per_block * ptrs_per_block;
    if (logical_block < doubly_limit) {
        std::uint64_t a = logical_block / ptrs_per_block; 
        std::uint64_t b = logical_block % ptrs_per_block;
        
        std::uint64_t first_level_ptr = read_indirect_ptr(partition, node->i_block[13], a);
        return read_indirect_ptr(partition, first_level_ptr, b);
    }
    logical_block -= doubly_limit;

    std::uint64_t triply_limit = ptrs_per_block * ptrs_per_block * ptrs_per_block;
    if (logical_block < triply_limit) {
        std::uint64_t a = logical_block / (ptrs_per_block * ptrs_per_block);
        std::uint64_t remaining = logical_block % (ptrs_per_block * ptrs_per_block);
        std::uint64_t b = remaining / ptrs_per_block;
        std::uint64_t c = remaining % ptrs_per_block;

        std::uint64_t L1_ptr = read_indirect_ptr(partition, node->i_block[14], a);
        std::uint64_t L2_ptr = read_indirect_ptr(partition, L1_ptr, b);
        return read_indirect_ptr(partition, L2_ptr, c);
    }
    
    return 0; 
}


std::uint64_t ext2_find_child(ext2_partition* part, ext2_inode* dir_inode, const char* name) {
    std::uint32_t block_size = 1024 << part->sb->s_log_block_size;
    char* block_buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    
    std::uint64_t num_blocks = dir_inode->i_size / block_size;
    for (std::uint64_t i = 0; i < num_blocks; i++) {
        std::uint64_t phys_block = get_phys_block(part, dir_inode, i);
        if (phys_block == 0) continue;

        ext2_read_block(part, phys_block, block_buffer);

        ext2_dir_entry* entry = (ext2_dir_entry*)block_buffer;
        std::uint64_t offset = 0;

        while (offset < block_size && entry->inode != 0) {
            if (klibc::strlen(name) == entry->name_len && 
                klibc::memcmp(name, entry->name, entry->name_len) == 0) {
                
                std::uint64_t found_inode = entry->inode;
                pmm::freelist::free((std::uint64_t)block_buffer - etc::hhdm());
                return found_inode;
            }

            offset += entry->rec_len;
            entry = (ext2_dir_entry*)(block_buffer + offset);
        }
    }

    pmm::freelist::free((std::uint64_t)block_buffer - etc::hhdm());
    return 0; 
}

std::int32_t ext2_lookup(ext2_partition* part, const char* path, ext2_inode* out, std::uint64_t* inode_out) {
    uint32_t current_inode_num = 2;
    ext2_inode current_inode = ext2_get_inode(part, 2);

    char path_copy[1024];
    klibc::memcpy(path_copy, path, klibc::strlen(path));

    char* saveptr;
    char* token = klibc::strtok(&saveptr, path_copy, "/");

#ifdef EXT2_ORANGE_TRACE
    klibc::printf("ext2: trying to lookup %s\r\n",path);
#endif

    while (token != nullptr) {
        if ((current_inode.i_mode & 0xF000) != 0x4000) {
            return -ENOTDIR;
        }

        current_inode_num = ext2_find_child(part, &current_inode, token);
        if (current_inode_num == 0) {
            return -ENOENT;
        }

        current_inode = ext2_get_inode(part, current_inode_num);
        token = klibc::strtok(&saveptr, nullptr, "/");
    }


#ifdef EXT2_ORANGE_TRACE
    klibc::printf("ext2: return inode %lli (%s)\r\n", current_inode_num, path);
#endif

    *inode_out = current_inode_num;
    *out = current_inode;
    return 0;
}

std::int64_t ext2_alloc_blocks(ext2_partition* part, uint32_t count, uint64_t* out) {
    uint64_t blocks_per_group = part->sb->s_blocks_per_group;
    uint64_t total_groups = (ext2_blocks_count(part->sb) + blocks_per_group - 1) / blocks_per_group;
    
    uint8_t* bitmap = (uint8_t*)(pmm::freelist::alloc_4k() + etc::hhdm());
    uint64_t allocated_so_far = 0;

    for (uint32_t g = 0; g < total_groups; g++) {
        ext2_group_desc* gd = &((ext2_group_desc*)part->cached_group)[g];
        if (gd->bg_free_blocks_count < count) continue;

        ext2_read_block(part, gd->bg_block_bitmap, (char*)bitmap);

        for (uint32_t i = 0; i <= blocks_per_group - count; i++) {
            bool found = true;
            for (uint32_t j = 0; j < count; j++) {
                if (ext2_test_bit(bitmap, i + j)) {
                    found = false;
                    break;
                }
            }

            if (found) {
                for (uint32_t j = 0; j < count; j++) {
                    ext2_set_bit(bitmap, i + j);
                    out[j] = (uint64_t)g * blocks_per_group + i + j + part->sb->s_first_data_block;
                }
                ext2_write_block(part, gd->bg_block_bitmap, (char*)bitmap);
                gd->bg_free_blocks_count -= count;
                pmm::freelist::free((uint64_t)bitmap - etc::hhdm());
                return 0;
            }
        }
    }

    for (uint32_t g = 0; g < total_groups && allocated_so_far < count; g++) {
        ext2_group_desc* gd = &((ext2_group_desc*)part->cached_group)[g];
        if (gd->bg_free_blocks_count == 0) continue;

        ext2_read_block(part, gd->bg_block_bitmap, (char*)bitmap);
        bool changed = false;

        for (uint32_t i = 0; i < blocks_per_group && allocated_so_far < count; i++) {
            if (!ext2_test_bit(bitmap, i)) {
                ext2_set_bit(bitmap, i);
                out[allocated_so_far++] = (uint64_t)g * blocks_per_group + i + part->sb->s_first_data_block;
                gd->bg_free_blocks_count--;
                changed = true;
            }
        }
        if (changed) ext2_write_block(part, gd->bg_block_bitmap, (char*)bitmap);
    }

    pmm::freelist::free((uint64_t)bitmap - etc::hhdm());
    return (allocated_so_far == count) ? 0 : -ENOSPC;
}

void ext2_free_blocks(ext2_partition* part, uint32_t count, uint64_t* blocks) {
    uint32_t blocks_per_group = part->sb->s_blocks_per_group;
    uint8_t* bitmap = (uint8_t*)(pmm::freelist::alloc_4k() + etc::hhdm());

    for (uint32_t i = 0; i < count; i++) {
        uint64_t abs_block = blocks[i] - part->sb->s_first_data_block;
        uint32_t group = abs_block / blocks_per_group;
        uint32_t index = abs_block % blocks_per_group;

        ext2_group_desc* gd = &((ext2_group_desc*)part->cached_group)[group];
        
        ext2_read_block(part, gd->bg_block_bitmap, (char*)bitmap);
        if (ext2_test_bit(bitmap, index)) {
            ext2_clear_bit(bitmap, index);
            gd->bg_free_blocks_count++;
            ext2_write_block(part, gd->bg_block_bitmap, (char*)bitmap);
        }
    }
    pmm::freelist::free((uint64_t)bitmap - etc::hhdm());
}

signed long ext2_read(file_descriptor* file, void* buffer, signed long count) {
    if (count <= 0) return 0;

    file->vnode.fs->lock.lock();

    std::int32_t status = 0;
    ext2_partition* part = (ext2_partition*)(file->vnode.fs->fs_specific.partition);
    ext2_inode inode = ext2_get_inode(part, file->fs_specific.ino, &status);

    if (status != 0) {
        file->vnode.fs->lock.unlock();
        return status; 
    }

    if (file->offset >= inode.i_size) {
        file->vnode.fs->lock.unlock();
        return 0; 
    }

    if (file->offset + count > inode.i_size) {
        count = inode.i_size - file->offset;
    }

    std::uint32_t block_size = 1024 << part->sb->s_log_block_size;
    char* temp_block = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    
    signed long total_read = 0;
    char* out_ptr = (char*)buffer;

    while (total_read < count) {
        std::uint64_t logical_block = (file->offset + total_read) / block_size;
        std::uint64_t block_offset = (file->offset + total_read) % block_size;
        
        std::uint64_t phys_block = get_phys_block(part, &inode, logical_block);
        
        if (phys_block == 0) {
            klibc::memset(temp_block, 0, block_size);
        } else {
            ext2_read_block(part, phys_block, temp_block);
        }

        std::uint32_t to_copy = block_size - block_offset;
        if (to_copy > (std::uint32_t)(count - total_read)) {
            to_copy = count - total_read;
        }

        klibc::memcpy(out_ptr + total_read, temp_block + block_offset, to_copy);
        
        total_read += to_copy;
    }

    file->offset += total_read;

    pmm::freelist::free((std::uint64_t)temp_block - etc::hhdm());
    file->vnode.fs->lock.unlock();

    return total_read;
}

uint32_t set_indirect_ptr(ext2_partition* part, uint32_t table_block, uint32_t index, uint32_t new_val) {
    uint32_t* buffer = (uint32_t*)(pmm::freelist::alloc_4k() + etc::hhdm());
    ext2_read_block(part, table_block, (char*)buffer);
    buffer[index] = new_val;
    ext2_write_block(part, table_block, (char*)buffer);
    pmm::freelist::free((uint64_t)buffer - etc::hhdm());
    return new_val;
}

uint64_t get_or_alloc_phys_block(ext2_partition* part, ext2_inode* node, uint32_t logical_block, bool* dirty) {
    uint32_t block_size = 1024 << part->sb->s_log_block_size;
    uint32_t ptrs_per_block = block_size / 4;

    auto alloc_one = [&]() {
        uint64_t b = 0;
        ext2_alloc_blocks(part, 1, &b);
        char* zero = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
        klibc::memset(zero, 0, block_size);
        ext2_write_block(part, b, zero);
        pmm::freelist::free((uint64_t)zero - etc::hhdm());
        *dirty = true;
        return (uint32_t)b;
    };

    if (logical_block < 12) {
        if (!node->i_block[logical_block]) node->i_block[logical_block] = alloc_one();
        return node->i_block[logical_block];
    }
    logical_block -= 12;

    if (logical_block < ptrs_per_block) {
        if (!node->i_block[12]) node->i_block[12] = alloc_one();
        uint32_t phys = read_indirect_ptr(part, node->i_block[12], logical_block);
        if (!phys) phys = set_indirect_ptr(part, node->i_block[12], logical_block, alloc_one());
        return phys;
    }
    logical_block -= ptrs_per_block;

    uint64_t doubly_limit = ptrs_per_block * ptrs_per_block;
    if (logical_block < doubly_limit) {
        if (!node->i_block[13]) node->i_block[13] = alloc_one();
        uint32_t a = logical_block / ptrs_per_block;
        uint32_t b = logical_block % ptrs_per_block;

        uint32_t L1 = read_indirect_ptr(part, node->i_block[13], a);
        if (!L1) L1 = set_indirect_ptr(part, node->i_block[13], a, alloc_one());
        
        uint32_t phys = read_indirect_ptr(part, L1, b);
        if (!phys) phys = set_indirect_ptr(part, L1, b, alloc_one());
        return phys;
    }
    logical_block -= doubly_limit;

    if (!node->i_block[14]) node->i_block[14] = alloc_one();
    uint32_t a = logical_block / (ptrs_per_block * ptrs_per_block);
    uint32_t rem = logical_block % (ptrs_per_block * ptrs_per_block);
    uint32_t b = rem / ptrs_per_block;
    uint32_t c = rem % ptrs_per_block;

    uint32_t L1 = read_indirect_ptr(part, node->i_block[14], a);
    if (!L1) L1 = set_indirect_ptr(part, node->i_block[14], a, alloc_one());
    uint32_t L2 = read_indirect_ptr(part, L1, b);
    if (!L2) L2 = set_indirect_ptr(part, L1, b, alloc_one());
    uint32_t phys = read_indirect_ptr(part, L2, c);
    if (!phys) phys = set_indirect_ptr(part, L2, c, alloc_one());

    return phys;
}

void ext2_write_inode(ext2_partition* part, uint64_t ino, ext2_inode* inode) {
    ext2_superblock* sb = part->sb;
    uint32_t block_size = 1024 << sb->s_log_block_size;
    uint32_t group = inode_to_block_group(sb, ino);
    ext2_group_desc* gd = (ext2_group_desc*)((uint64_t)part->cached_group + group * sizeof(ext2_group_desc));
    
    uint32_t inode_off = inode_to_index(sb, ino) * inode_size(sb);
    uint32_t block = gd->bg_inode_table + (inode_off / block_size);
    uint32_t off_in_block = inode_off % block_size;

    char* buf = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    ext2_read_block(part, block, buf);
    klibc::memcpy(buf + off_in_block, inode, sizeof(ext2_inode));
    ext2_write_block(part, block, buf);
    pmm::freelist::free((uint64_t)buf - etc::hhdm());
}

signed long ext2_write(file_descriptor* file, void* buffer, signed long count) {
    if (count <= 0) return 0;
    file->vnode.fs->lock.lock();

    ext2_partition* part = (ext2_partition*)(file->vnode.fs->fs_specific.partition);
    ext2_inode inode = ext2_get_inode(part, file->fs_specific.ino, nullptr);
    uint32_t block_size = 1024 << part->sb->s_log_block_size;
    char* temp_block = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());

    signed long total_written = 0;
    const char* in_ptr = (const char*)buffer;
    bool inode_dirty = false;

    while (total_written < count) {
        uint64_t logical_block = (file->offset) / block_size;
        uint64_t block_offset = (file->offset) % block_size;
        uint32_t to_write = block_size - block_offset;
        if (to_write > (uint32_t)(count - total_written)) to_write = count - total_written;

        uint64_t phys_block = get_or_alloc_phys_block(part, &inode, logical_block, &inode_dirty);
        
        if (to_write < block_size) {
            ext2_read_block(part, phys_block, temp_block);
        }
        
        klibc::memcpy(temp_block + block_offset, in_ptr + total_written, to_write);
        ext2_write_block(part, phys_block, temp_block);

        file->offset += to_write;
        total_written += to_write;
        
        if (file->offset > inode.i_size) {
            inode.i_size = file->offset;
            inode_dirty = true;
        }
    }

    if (inode_dirty) {
        ext2_write_inode(part, file->fs_specific.ino, &inode);
    }

    pmm::freelist::free((uint64_t)temp_block - etc::hhdm());
    file->vnode.fs->lock.unlock();
    return total_written;
}

std::uint32_t ext2_open(filesystem* fs, void* file_desc, char* path) {
    fs->lock.lock();
    ext2_inode res;
    std::uint64_t inode_num = 0;
    std::uint32_t status = ext2_lookup((ext2_partition*)(fs->fs_specific.partition), path, &res, &inode_num);
    if(status != 0) { fs->lock.unlock();
        return status; }
    file_descriptor* fd = (file_descriptor*)file_desc;
    fd->fs_specific.ino = inode_num;
    fd->vnode.fs = fs;
    fd->vnode.read = ext2_read;
    fd->vnode.write = ext2_write;
    fs->lock.unlock();
    return 0;
}

std::int32_t ext2_stat(file_descriptor* file, stat* out) {
    std::int32_t status = 0;
    ext2_partition* part = (ext2_partition*)(file->vnode.fs->fs_specific.partition);
    ext2_inode inode = ext2_get_inode(part, file->fs_specific.ino, &status);

    if (status != 0) {
        file->vnode.fs->lock.unlock();
        return status; 
    }

    out->st_atim.tv_nsec = 0;
    out->st_atim.tv_sec = inode.i_atime;
    out->st_ctim.tv_nsec = 0;
    out->st_ctim.tv_sec = inode.i_ctime;
    out->st_mtim.tv_nsec = 0;
    out->st_mtim.tv_sec = inode.i_mtime;
    out->st_blksize = 1024 << part->sb->s_log_block_size;
    out->st_blocks = ALIGNUP(inode.i_size, out->st_blksize) / out->st_blksize;
    out->st_size = inode.i_size;
    out->st_uid = inode.i_uid;
    out->st_gid = inode.i_gid;
    out->st_dev = 0;
    out->st_rdev = 0; // should be filled by vfs 
    out->st_nlink = inode.i_links_count;
    out->st_mode = inode.i_mode;

    return 0;
}

void ext2_load_group_descriptors(ext2_partition* part) {
    uint32_t block_size = 1024 << part->sb->s_log_block_size;
    
    uint32_t bgdt_block = (block_size == 1024) ? 2 : 1;

    uint32_t blocks_per_group = part->sb->s_blocks_per_group;
    uint32_t groups_count = (ext2_blocks_count(part->sb) + blocks_per_group - 1) / blocks_per_group;
    
    uint32_t table_size_bytes = groups_count * sizeof(ext2_group_desc);
    uint32_t table_size_blocks = (table_size_bytes + block_size - 1) / block_size;

    for (uint32_t i = 0; i < table_size_blocks; i++) {
        ext2_read_block(part, bgdt_block + i, (char*)((uint64_t)part->cached_group + (i * block_size)));
    }
}

static inline int isprint(int c) {
    return (c >= 0x20 && c <= 0x7E);
}


static inline void print_buffer(const unsigned char *buffer, std::size_t size) {
    
     for (std::size_t i = 0; i < size; i++) {

        if(buffer[i] == '\0')
            continue;

         if (isprint(buffer[i])) {
            klibc::printf("%c ", buffer[i]);
        } else {
             klibc::printf("0x%02X ", buffer[i]);
        }
    }
   klibc::printf("\r\n");
}

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
        klibc::printf("ext2: detected features %s %s %s\r\n",(sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE) ? "EXT2_FEATURE_RO_COMPAT_LARGE_FILE" : "", (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? "EXT4_FEATURE_INCOMPAT_64BIT" : "", (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) ? "EXT4_FEATURE_INCOMPAT_EXTENTS" : "");
    }

    uint32_t groups_count = (ext2_blocks_count(sb) + sb->s_blocks_per_group - 1);

    ext2_partition* part = new ext2_partition;
    part->buffer = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    part->lba_start = lba_start;
    part->lock.unlock();
    part->sb = sb;
    part->target_disk = target_disk;
    part->cached_group = (void*)(pmm::buddy::alloc(groups_count * sizeof(ext2_group_desc)).phys + etc::hhdm());

    ext2_load_group_descriptors(part);

    ext2_inode root = ext2_get_inode(part, 2);
    klibc::printf("inode 2 size %lli links count %lli mode 0x%p block_size %lli\r\n",root.i_size, root.i_links_count, root.i_mode, 1024 << part->sb->s_log_block_size);
    klibc::printf("logic blocks for root first block = %lli, second block = %lli\r\n", get_phys_block(part, &root, 0),get_phys_block(part, &root, 1));
    (void)print_buffer;

    char* buffer2 = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    ext2_read_block(part, get_phys_block(part, &root, 0), buffer2);
    klibc::printf("dumping first root block\r\n");
    print_buffer((const unsigned char*)buffer2, 1024 << part->sb->s_log_block_size);

    const char* file_test = "/test1/meow";
    ext2_inode res = {};
    std::uint64_t in = 0;
    std::int32_t status = ext2_lookup(part, file_test, &res, &in);

    klibc::memset(buffer2, 0, PAGE_SIZE);
    klibc::printf("reading file %s, status %d, size %lli\r\n", file_test, status, res.i_size);
    ext2_read_block(part, get_phys_block(part, &res, 0), buffer2);
    klibc::printf("%s\r\n", buffer2);

    klibc::printf("group count %lli (size %lli)\r\n",groups_count,groups_count * sizeof(ext2_group_desc));
    
}