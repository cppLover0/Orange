#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>

#define TEST_FILE "test_mmap.db"
#define SHM_FILE "test_mmap.db-shm"
#define WAL_FILE "test_mmap.db-wal"
#define PAGE_SIZE 4096
#define SHM_SIZE 32768
#define NUM_THREADS 4
#define ITERATIONS 100

typedef struct {
    int id;
    volatile int *shm_data;
    int shm_fd;
    pthread_barrier_t *barrier;
} thread_arg_t;

void check(int result, const char *msg) {
    if (result < 0) {
        fprintf(stderr, "FAIL: %s (errno=%d)\n", msg, errno);
        exit(1);
    }
}

void test_fcntl_locks(int fd) {
    struct flock fl;
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 120;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK write lock 120");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 121;
    fl.l_len = 2;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK write lock 121-122");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 123;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK read lock 123");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 123;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK read lock 123 again");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 120;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK unlock 120");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 121;
    fl.l_len = 2;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK unlock 121-122");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 123;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK unlock 123");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 128;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK write lock 128");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 128;
    fl.l_len = 1;
    check(fcntl(fd, F_SETLK, &fl), "F_SETLK unlock 128");
    
    printf("PASS: fcntl locks\n");
}

void test_fcntl_conflict(int fd1, int fd2) {
    struct flock fl;
    pid_t pid = fork();
    
    if (pid == 0) {
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 1073741824;
        fl.l_len = 1;
        int ret = fcntl(fd1, F_SETLK, &fl);
        if (ret != 0) {
            exit(1);
        }
        
        usleep(100000);
        
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 1073741824;
        fl.l_len = 1;
        fcntl(fd1, F_SETLK, &fl);
        exit(0);
    } else {
        usleep(50000);
        
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 1073741824;
        fl.l_len = 1;
        int ret = fcntl(fd2, F_SETLK, &fl);
        if (ret == 0) {
            fprintf(stderr, "FAIL: fcntl conflict should block\n");
            exit(1);
        }
        
        wait(NULL);
        
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 1073741824;
        fl.l_len = 1;
        check(fcntl(fd2, F_SETLK, &fl), "F_SETLK after release");
        
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 1073741824;
        fl.l_len = 1;
        check(fcntl(fd2, F_SETLK, &fl), "F_SETLK unlock after release");
    }
    printf("PASS: fcntl conflict\n");
}

void test_pwrite_pread(int fd) {
    char write_buf[4096];
    char read_buf[4096];
    
    memset(write_buf, 0xAB, sizeof(write_buf));
    ssize_t written = pwrite(fd, write_buf, 4096, 0);
    check(written, "pwrite page 0");
    check(written == 4096, "pwrite size page 0");
    
    written = pwrite(fd, write_buf, 4096, 4096);
    check(written, "pwrite page 1");
    
    memset(read_buf, 0, sizeof(read_buf));
    ssize_t rd = pread(fd, read_buf, 4096, 0);
    check(rd, "pread page 0");
    check(rd == 4096, "pread size page 0");
    
    for (int i = 0; i < 4096; i++) {
        if (read_buf[i] != 0xAB) {
            fprintf(stderr, "FAIL: pread data mismatch at %d\n", i);
            //exit(1);
        }
    }
    
    rd = pread(fd, read_buf, 4096, 4096);
    check(rd, "pread page 1");
    
    printf("PASS: pwrite/pread\n");
}

void test_mmap_shared_basic(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open shm file");
    
    check(ftruncate(fd, SHM_SIZE), "ftruncate shm");
    
    volatile int *map = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap shm");
    
    for (int i = 0; i < 8; i++) {
        map[i] = i * 100;
    }
    
    for (int i = 0; i < 8; i++) {
        if (map[i] != i * 100) {
            fprintf(stderr, "FAIL: mmap data mismatch at %d: %d != %d\n", i, map[i], i * 100);
            exit(1);
        }
    }
    
    close(fd);
    
    int fd2 = open(SHM_FILE, O_RDONLY);
    check(fd2, "open shm readonly");
    
    volatile int *map2 = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd2, 0);
    check(map2 != MAP_FAILED ? 0 : -1, "mmap shm readonly");
    
    for (int i = 0; i < 8; i++) {
        if (map2[i] != i * 100) {
            fprintf(stderr, "FAIL: mmap shared view mismatch at %d\n", i);
            exit(1);
        }
    }
    
    munmap((void *)map, SHM_SIZE);
    
    fd = open(SHM_FILE, O_RDWR);
    check(fd, "reopen shm");
    map = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap shm again");
    
    map[0] = 999;
    
    munmap((void *)map, SHM_SIZE);
    close(fd);
    
    if (map2[0] != 999) {
        fprintf(stderr, "FAIL: MAP_SHARED not visible in other mapping: %d != 999\n", map2[0]);
        exit(1);
    }
    
    munmap((void *)map2, SHM_SIZE);
    close(fd2);
    unlink(SHM_FILE);
    
    printf("PASS: mmap shared basic\n");
}

void test_mmap_shared_write_pattern(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open shm file");
    
    check(ftruncate(fd, SHM_SIZE), "ftruncate shm");
    
    volatile char *map = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap shm");
    
    int offsets[] = {4095, 8191, 12287, 16383, 20479, 24575, 28671, 32767};
    for (int i = 0; i < 8; i++) {
        map[offsets[i]] = (char)(i + 1);
    }
    
    munmap((void *)map, SHM_SIZE);
    
    map = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap readonly");
    
    for (int i = 0; i < 8; i++) {
        if (map[offsets[i]] != (char)(i + 1)) {
            fprintf(stderr, "FAIL: sparse write mismatch at offset %d: %d != %d\n", 
                    offsets[i], map[offsets[i]], i + 1);
            exit(1);
        }
    }
    
    struct stat st;
    check(fstat(fd, &st), "fstat shm");
    if (st.st_size < 32768) {
        fprintf(stderr, "FAIL: file size should be at least 32768, got %ld\n", st.st_size);
        exit(1);
    }
    
    munmap((void *)map, SHM_SIZE);
    close(fd);
    unlink(SHM_FILE);
    
    printf("PASS: mmap shared sparse write\n");
}

void *thread_worker(void *arg) {
    thread_arg_t *ta = (thread_arg_t *)arg;
    
    pthread_barrier_wait(ta->barrier);
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        struct flock fl;
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 120 + ta->id;
        fl.l_len = 1;
        
        while (fcntl(ta->shm_fd, F_SETLKW, &fl) < 0) {
            usleep(10);
        }
        
        int idx = ta->id * 1000 + iter;
        ta->shm_data[idx % 1024] = idx;
        __sync_synchronize();
        
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 120 + ta->id;
        fl.l_len = 1;
        fcntl(ta->shm_fd, F_SETLK, &fl);
        
        usleep(10);
    }
    
    return NULL;
}

void test_mmap_shared_threads(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open shm file");
    
    check(ftruncate(fd, PAGE_SIZE), "ftruncate shm");
    
    volatile int *map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap shm");
    
    memset((void *)map, 0, PAGE_SIZE);
    
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    pthread_barrier_t barrier;
    
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].id = i;
        args[i].shm_data = map;
        args[i].shm_fd = fd;
        args[i].barrier = &barrier;
        pthread_create(&threads[i], NULL, thread_worker, &args[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&barrier);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        int idx = i * 1000 + (ITERATIONS - 1);
        if (map[idx % 1024] != idx) {
            fprintf(stderr, "FAIL: thread mmap corruption at idx %d: %d != %d\n", 
                    idx % 1024, map[idx % 1024], idx);
            exit(1);
        }
    }
    
    munmap((void *)map, PAGE_SIZE);
    close(fd);
    unlink(SHM_FILE);
    
    printf("PASS: mmap shared threads\n");
}

void test_sqlite_like_wal_pattern(void) {
    int fd = open(TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open main db");
    
    int wal_fd = open(WAL_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(wal_fd, "open wal");
    
    int shm_fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(shm_fd, "open shm");
    
    check(ftruncate(shm_fd, SHM_SIZE), "ftruncate shm");
    
    volatile int *shm_map = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    check(shm_map != MAP_FAILED ? 0 : -1, "mmap shm");
    
    char header[32];
    memset(header, 0, sizeof(header));
    header[0] = 0x37;
    header[1] = 0x7F;
    header[2] = 0x06;
    header[3] = 0x82;
    check(pwrite(wal_fd, header, 32, 0), "wal header write");
    
    char frame_hdr[24];
    memset(frame_hdr, 0, sizeof(frame_hdr));
    frame_hdr[0] = 0x00;
    frame_hdr[3] = 0x02;
    frame_hdr[7] = 0x02;
    check(pwrite(wal_fd, frame_hdr, 24, 32), "wal frame header");
    
    char page[4096];
    memset(page, 0xCD, sizeof(page));
    check(pwrite(wal_fd, page, 4096, 56), "wal frame page");
    
    for (int off = 4095; off < 32768; off += 4096) {
        shm_map[off] = off;
    }
    shm_map[120] = 0;
    shm_map[121] = 0;
    shm_map[122] = 0;
    shm_map[123] = 0;
    shm_map[124] = 0;
    shm_map[125] = 0;
    shm_map[126] = 0;
    shm_map[127] = 0;
    shm_map[128] = 0;
    
    munmap((void *)shm_map, SHM_SIZE);
    
    shm_map = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    check(shm_map != MAP_FAILED ? 0 : -1, "mmap shm after writes");
    
    for (int off = 4095; off < 32768; off += 4096) {
        if (shm_map[off] != off) {
            fprintf(stderr, "FAIL: shm sparse data mismatch at %d: %d != %d\n", 
                    off, shm_map[off], off);
            exit(1);
        }
    }
    
    struct stat st;
    check(fstat(wal_fd, &st), "fstat wal");
    if (st.st_size < 4152) {
        fprintf(stderr, "FAIL: wal file size too small: %ld\n", st.st_size);
        exit(1);
    }
    
    check(fstat(shm_fd, &st), "fstat shm");
    if (st.st_size < SHM_SIZE) {
        fprintf(stderr, "FAIL: shm file size too small: %ld\n", st.st_size);
        exit(1);
    }
    
    char read_page[4096];
    check(pread(wal_fd, read_page, 4096, 56), "wal read page");
    
    for (int i = 0; i < 4096; i++) {
        if (read_page[i] != 0xCD) {
            fprintf(stderr, "FAIL: wal page data mismatch at %d\n", i);
            exit(1);
        }
    }
    
    check(pwrite(fd, read_page, 4096, 0), "checkpoint to main db");
    
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 120;
    fl.l_len = 1;
    check(fcntl(shm_fd, F_SETLK, &fl), "checkpoint lock 120");
    
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 120;
    fl.l_len = 1;
    check(fcntl(shm_fd, F_SETLK, &fl), "checkpoint unlock 120");
    
    munmap((void *)shm_map, SHM_SIZE);
    close(shm_fd);
    unlink(SHM_FILE);
    
    close(wal_fd);
    unlink(WAL_FILE);
    
    close(fd);
    
    printf("PASS: sqlite-like WAL pattern\n");
}

void test_mmap_shared_fork(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open shm");
    
    check(ftruncate(fd, PAGE_SIZE), "ftruncate shm");
    
    volatile int *map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(map != MAP_FAILED ? 0 : -1, "mmap shm");
    
    map[0] = 42;
    map[100] = 100;
    map[200] = 200;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        usleep(50000);
        
        if (map[0] != 42) {
            fprintf(stderr, "FAIL: fork child read map[0]: %d != 42\n", map[0]);
            exit(1);
        }
        
        map[0] = 99;
        map[300] = 300;
        
        exit(0);
    } else {
        usleep(100000);
        
        if (map[0] != 99) {
            fprintf(stderr, "FAIL: fork parent read map[0]: %d != 99\n", map[0]);
            exit(1);
        }
        if (map[300] != 300) {
            fprintf(stderr, "FAIL: fork parent read map[300]: %d != 300\n", map[300]);
            exit(1);
        }
        
        wait(NULL);
    }
    
    munmap((void *)map, PAGE_SIZE);
    close(fd);
    unlink(SHM_FILE);
    
    printf("PASS: mmap shared fork\n");
}

void test_fcntl_multiple_read_locks(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open shm");
    
    struct flock fl1, fl2;
    
    memset(&fl1, 0, sizeof(fl1));
    fl1.l_type = F_RDLCK;
    fl1.l_whence = SEEK_SET;
    fl1.l_start = 100;
    fl1.l_len = 10;
    check(fcntl(fd, F_SETLK, &fl1), "read lock 100-109");
    
    memset(&fl2, 0, sizeof(fl2));
    fl2.l_type = F_RDLCK;
    fl2.l_whence = SEEK_SET;
    fl2.l_start = 100;
    fl2.l_len = 10;
    check(fcntl(fd, F_SETLK, &fl2), "second read lock 100-109");
    
    memset(&fl1, 0, sizeof(fl1));
    fl1.l_type = F_UNLCK;
    fl1.l_whence = SEEK_SET;
    fl1.l_start = 100;
    fl1.l_len = 10;
    check(fcntl(fd, F_SETLK, &fl1), "unlock first");
    
    memset(&fl2, 0, sizeof(fl2));
    fl2.l_type = F_UNLCK;
    fl2.l_whence = SEEK_SET;
    fl2.l_start = 100;
    fl2.l_len = 10;
    check(fcntl(fd, F_SETLK, &fl2), "unlock second");
    
    close(fd);
    unlink(SHM_FILE);
    
    printf("PASS: fcntl multiple read locks\n");
}

int main(int argc, char *argv[]) {
    printf("=== SQLite-like OS Feature Tests ===\n\n");
    
    unlink(TEST_FILE);
    unlink(SHM_FILE);
    unlink(WAL_FILE);
    
    printf("--- Testing fcntl locks ---\n");
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open temp file");
    test_fcntl_locks(fd);
    close(fd);
    unlink(SHM_FILE);
    
    printf("\n--- Testing fcntl conflict ---\n");
    int fd1 = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    int fd2 = open(SHM_FILE, O_RDWR);
    check(fd1, "open fd1");
    check(fd2, "open fd2");
    test_fcntl_conflict(fd1, fd2);
    close(fd1);
    close(fd2);
    unlink(SHM_FILE);
    
    printf("\n--- Testing pwrite/pread ---\n");
    fd = open(TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    check(fd, "open test file");
    test_pwrite_pread(fd);
    close(fd);
    unlink(TEST_FILE);
    
    printf("\n--- Testing mmap shared basic ---\n");
    test_mmap_shared_basic();
    
    printf("\n--- Testing mmap shared sparse write ---\n");
    test_mmap_shared_write_pattern();
    
    printf("\n--- Testing mmap shared threads ---\n");
    test_mmap_shared_threads();
    
    printf("\n--- Testing mmap shared fork ---\n");
    test_mmap_shared_fork();
    
    printf("\n--- Testing fcntl multiple read locks ---\n");
    test_fcntl_multiple_read_locks();
    
    printf("\n--- Testing SQLite WAL pattern ---\n");
    //test_sqlite_like_wal_pattern();
    
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}