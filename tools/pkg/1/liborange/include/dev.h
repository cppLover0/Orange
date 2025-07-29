
// liborange dev.h 

#include <stdint.h>

#ifndef LIBORANGE_DEV_H
#define LIBORANGE_DEV_H

uint64_t liborange_read(int fd, void* buffer, uint64_t count);
void liborange_send(int fd, void* buffer, uint64_t count);


#endif