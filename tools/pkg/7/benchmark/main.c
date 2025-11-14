
#define _GNU_SOURCE

#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

void pipe_rw_test() {
    int fd[2];
    pipe(fd);
    printf("OS pipe test\n");
    
    char buffer[5];

    for(int i = 0;i < 5;i++) {
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC,&start);
        write(fd[1],"test\n",4);
        read(fd[0],buffer,5);
        clock_gettime(CLOCK_MONOTONIC,&end);
        printf("OS pipe test %d - %d ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
    
}

void rw_test() {
    printf("OS r/w test\n");
    int fd = open("/tmp/testfd",O_RDWR | O_CREAT | O_TRUNC);
    char buffer[5];

    for(int i = 0;i < 5;i++) {
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC,&start);
        write(fd,"test\n",4);
        lseek(fd,0,SEEK_SET);
        read(fd,buffer,5);
        lseek(fd,0,SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC,&end);
        printf("OS pipe test %d - %d ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
}

void open_test() {
    printf("OS open test\n");
    for(int i = 0;i < 5;i++) {
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC,&start);
        int fd = open("/tmp/testfd",O_RDWR | O_CREAT | O_TRUNC);
        close(fd);
        clock_gettime(CLOCK_MONOTONIC,&end);
        printf("OS pipe test %d - %d ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
}

void mmap_test() {
    printf("OS mmap test\n");
    for(int i = 0;i < 5;i++) {
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC,&start);
        void* addr = mmap(0,0x80000,0,MAP_ANONYMOUS,-1,0);
        clock_gettime(CLOCK_MONOTONIC,&end);
        printf("OS pipe test %d - %d ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
}

int main() {
    pipe_rw_test();
    rw_test();
    open_test();
    mmap_test();
}