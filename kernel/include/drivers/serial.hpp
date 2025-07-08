
#include <cstdint>
#include <cstddef>

#include <drivers/io.hpp>

#pragma once


#define DEFAULT_SERIAL_PORT 0x3F8

typedef struct {
    std::uint16_t port;
    std::uint8_t is_init;
} serial_mapping_t;

namespace drivers {
    class serial {
private:
        std::uint16_t target_port;
        std::uint8_t is_possible;
public:
        
        static void init(uint16_t port);
        static std::uint8_t is_init(uint16_t port);

        serial(uint16_t port) {
            target_port = port;
            if(!is_init(port))
                init(port);
        }

        void send(std::uint8_t data) {
            drivers::io io;
            while((io.inb(target_port + 5) & 0x20) == 0);
            io.outb(target_port,data);
        }

        void write(std::uint8_t* data,std::size_t len) {
            for(std::size_t i = 0; i < len; i++) {
                send(data[i]);
            }
        }
        
        std::uint8_t fetch() {
            drivers::io io;
            while((io.inb(target_port + 5) & 1) == 0);
            return io.inb(target_port);
        }

    };
};