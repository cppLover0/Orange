
#define _GNU_SOURCE

#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#include <stdint.h>

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
        printf("OS pipe test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
        
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
        printf("OS pipe test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
        
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
        printf("OS pipe test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
}

void mmap_test() {
    printf("OS mmap test\n");
    for(int i = 0;i < 5;i++) {
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC,&start);
        void* addr = mmap(NULL, 0x80000, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (addr == MAP_FAILED) {
            perror("mmap failed");
            continue;
        }memset(addr,1,0x80000);
        clock_gettime(CLOCK_MONOTONIC,&end);
        printf("OS mmap test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
        
    }
}

typedef unsigned long long op_t;
#define OPSIZ 8
#define byte char

void * __attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns"))) t_memset (void *dstpp, int c, size_t len)
{
  long int dstp = (long int) dstpp;

  if (len >= 8)
    {
      size_t xlen;
      op_t cccc;

      cccc = (unsigned char) c;
      cccc |= cccc << 8;
      cccc |= cccc << 16;
      if (OPSIZ > 4)
	/* Do the shift in two steps to avoid warning if long has 32 bits.  */
	cccc |= (cccc << 16) << 16;

      /* There are at least some bytes to set.
	 No need to test for LEN == 0 in this alignment loop.  */
      while (dstp % OPSIZ != 0)
	{
	  ((byte *) dstp)[0] = c;
	  dstp += 1;
	  len -= 1;
	}

      /* Write 8 `op_t' per iteration until less than 8 `op_t' remain.  */
      xlen = len / (OPSIZ * 8);
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  ((op_t *) dstp)[1] = cccc;
	  ((op_t *) dstp)[2] = cccc;
	  ((op_t *) dstp)[3] = cccc;
	  ((op_t *) dstp)[4] = cccc;
	  ((op_t *) dstp)[5] = cccc;
	  ((op_t *) dstp)[6] = cccc;
	  ((op_t *) dstp)[7] = cccc;
	  dstp += 8 * OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ * 8;

      /* Write 1 `op_t' per iteration until less than OPSIZ bytes remain.  */
      xlen = len / OPSIZ;
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  dstp += OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ;
    }

  /* Write the last few bytes.  */
  while (len > 0)
    {
      ((byte *) dstp)[0] = c;
      dstp += 1;
      len -= 1;
    }

  return dstpp;
}

int memset_test() {
    for(int i = 0; i < 16;i++) {
        struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC,&start);
   char buf[1024*1024];
   memset(buf,1,1024*1024);
   clock_gettime(CLOCK_MONOTONIC,&end);

   printf("memset test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
    }
    for(int i = 0; i < 16;i++) {
        struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC,&start);
   char buf[1024*1024];
   t_memset(buf,0,1024*1024);
   clock_gettime(CLOCK_MONOTONIC,&end);

   printf("opt memset test %d - %llu ns\n",i,end.tv_nsec - start.tv_nsec);
    }
}

int main() {
    pipe_rw_test();
    rw_test();
    open_test();
    mmap_test();
    memset_test();
}