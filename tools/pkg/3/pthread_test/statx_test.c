#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <linux/stat.h> // Required for struct statx definition and constants

// Helper function to print timestamp in a readable format
void print_timestamp(const char* label, struct statx_timestamp ts) {
    char buf[100];
    time_t t = ts.tv_sec;
    struct tm tm;
    if (localtime_r(&t, &tm) != NULL) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        printf("%s: %s.%09u\n", label, buf, ts.tv_nsec);
    } else {
        printf("%s: [Error converting time]\n", label);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *pathname = argv[1];
    int dirfd = AT_FDCWD; // Use current working directory
    int flags = AT_SYMLINK_NOFOLLOW; // Do not follow symbolic links
    unsigned int mask = STATX_ALL; // Request all available information
    struct statx stxbuf;

    // Zero out the structure before the call
    memset(&stxbuf, 0, sizeof(stxbuf));

    // Perform the statx system call
    long ret = statx(dirfd, pathname, flags, mask, &stxbuf);

    if (ret < 0) {
        perror("statx");
        return EXIT_FAILURE;
    }

    printf("File: %s\n", pathname);
    printf("Mask of returned fields: 0x%x\n", stxbuf.stx_mask);

    if (stxbuf.stx_mask & STATX_SIZE) {
        printf("Size: %llu bytes\n", (unsigned long long)stxbuf.stx_size);
    }

    if (stxbuf.stx_mask & STATX_BLOCKS) {
        printf("Blocks: %llu\n", (unsigned long long)stxbuf.stx_blocks);
    }

    if (stxbuf.stx_mask & STATX_MODE) {
        printf("Mode: 0%o\n", stxbuf.stx_mode);
    }

    if (stxbuf.stx_mask & STATX_UID) {
        printf("UID: %u\n", stxbuf.stx_uid);
    }

    if (stxbuf.stx_mask & STATX_GID) {
        printf("GID: %u\n", stxbuf.stx_gid);
    }

    if (stxbuf.stx_mask & STATX_BTIME) {
        print_timestamp("Birth Time", stxbuf.stx_btime);
    }

    if (stxbuf.stx_mask & STATX_ATIME) {
        print_timestamp("Access Time", stxbuf.stx_atime);
    }

    if (stxbuf.stx_mask & STATX_MTIME) {
        print_timestamp("Modification Time", stxbuf.stx_mtime);
    }

    return EXIT_SUCCESS;
}
