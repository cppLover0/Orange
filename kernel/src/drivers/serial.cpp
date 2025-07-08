
#include <cstdint>
#include <cstddef>

#include <drivers/serial.hpp>
#include <drivers/io.hpp>

#include <etc/libc.hpp>

serial_mapping_t __serial_map[32];
int __serial_map_ptr = 0;

void drivers::serial::init(uint16_t port) {
    
    if(__serial_map_ptr == 0)
        memset(__serial_map,0,sizeof(serial_mapping_t));
    
    drivers::io io;
    io.outb(DEFAULT_SERIAL_PORT + 1, 0x00);    
    io.outb(DEFAULT_SERIAL_PORT + 3, 0x80);  
    io.outb(DEFAULT_SERIAL_PORT + 0, 0x03);   
    io.outb(DEFAULT_SERIAL_PORT + 1, 0x00); 
    io.outb(DEFAULT_SERIAL_PORT + 3, 0x03);  
    io.outb(DEFAULT_SERIAL_PORT + 2, 0xC7);    
    io.outb(DEFAULT_SERIAL_PORT + 4, 0x0B);   
    io.outb(DEFAULT_SERIAL_PORT + 4, 0x1E);  
    io.outb(DEFAULT_SERIAL_PORT + 0, 0xAE);  

    if(io.inb(DEFAULT_SERIAL_PORT + 0) != 0xAE) {
        __serial_map[__serial_map_ptr].port = port;
        __serial_map[__serial_map_ptr++].is_init = 0;
    }

    io.outb(DEFAULT_SERIAL_PORT + 4, 0x0F);
    __serial_map[__serial_map_ptr].port = port;
    __serial_map[__serial_map_ptr++].is_init = 1;
}

std::uint8_t drivers::serial::is_init(uint16_t port) {
    
    for(int i = 0;i < __serial_map_ptr;i++) {
        if(__serial_map[i].port == port && __serial_map[i].is_init == 1)
            return 1;
    } 
    return 0;
}