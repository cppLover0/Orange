#pragma once
#include <cstdint>

typedef uint64_t  dev_t;   
typedef uint64_t  ino_t;     

typedef uint32_t  mode_t;    
typedef uint64_t  nlink_t;    
typedef uint32_t  uid_t;    
typedef uint32_t  gid_t;     

typedef int64_t   off_t;    
typedef int64_t   blksize_t;  
typedef int64_t   blkcnt_t;   

typedef int64_t   time_t;   

struct timespec {
    time_t  tv_sec;   
    long    tv_nsec; 
};