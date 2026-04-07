#include <cstdint>
#include <generic/squashfs.hpp>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>

char* squashfs_archive = nullptr;
int squashfs_len = 0;

squashfs_superblock* squashfs_sb = nullptr;

bool squashfs::is_valid(char* archive, int len) {
    if (len < 4) return false;
    uint32_t magic;
    klibc::memcpy(&magic, archive, sizeof(uint32_t));
    return (magic == SQUASHFS_MAGIC);
}

void squashfs::init(char* squashfs_archiv, int len, vfs::node* node) {
    (void)squashfs_archiv;
    (void)len;
    (void)node;
    log("squashfs", "todo: do squashfs later");
}