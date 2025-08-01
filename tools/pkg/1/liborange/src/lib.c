
#include <stdint.h>
#include <stdio.h>

#include <orange/dev.h>

uint64_t liborange_read(int fd, void* buffer, uint64_t count) {

}

void liborange_send(int fd, void* buffer, uint64_t count) {

}

void liborange_setup_ioctl(int fd, void* buf, uint64_t size, int32_t read_req, int32_t write_req) {

}

void liborange_setup_mmap(int fd, uint64_t dma_ddr, uint64_t dma_size) {

}

uint64_t liborange_alloc_dma(uint64_t dma_size) {

}

void liborange_free_dma(uint64_t dma_addr) {

}

uint64_t liborange_map_phys(uint64_t phys_addr) {
    
}