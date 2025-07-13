
#include <etc/assembly.hpp>
#include <drivers/io.hpp>
#include <cstdint>

#include <arch/x86_64/interrupts/irq.hpp>

#pragma once

#define PIC1 0x20		
#define PIC2 0xA0		
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)
#define ICW1_ICW4 0x01		
#define ICW1_SINGLE 0x02		
#define ICW1_INTERVAL4 0x04	
#define ICW1_LEVEL 0x08		
#define ICW1_INIT 0x10	

#define ICW4_8086 0x01		
#define ICW4_AUTO 0x02	
#define ICW4_BUF_SLAVE 0x08		
#define ICW4_BUF_MASTER 0x0C		
#define ICW4_SFNM 0x10	

#define CASCADE_IRQ 2

namespace arch {
    namespace x86_64 {
        namespace interrupts {
            class pic {
            public:
                static inline void eoi(std::uint8_t irq) {
                    drivers::io io;
                    if(irq >= 8)
                        io.outb(PIC2_COMMAND,0x20);
                    io.outb(PIC1_COMMAND,0x20);
                        
                }

                static inline void disable() {
                    drivers::io io;
                    io.outb(PIC1_DATA, 0xff);
                    io.outb(PIC2_DATA, 0xff);
                    arch::x86_64::interrupts::irq::reset();
                }

                static inline void init() {
                    drivers::io io;
                    io.outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  
                    io.wait();
                    io.outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
                    io.wait();
                    io.outb(PIC1_DATA, 0x20);                 
                    io.wait();
                    io.outb(PIC2_DATA, 0x28);                 
                    io.wait();
                    io.outb(PIC1_DATA, 1 << CASCADE_IRQ);        
                    io.wait();
                    io.outb(PIC2_DATA, 2);                       
                    io.wait();
                    
                    io.outb(PIC1_DATA, ICW4_8086);              
                    io.wait();
                    io.outb(PIC2_DATA, ICW4_8086);
                    io.wait();

                    io.outb(PIC1_DATA, 0);
                    io.outb(PIC2_DATA, 0);
                }

                static inline void mask(std::uint8_t irq) {
                    drivers::io io;
                    std::uint16_t port;
                    std::uint8_t value;

                    if(irq < 8) {
                        port = PIC1_DATA;
                    } else {
                        port = PIC2_DATA;
                        irq -= 8;
                    }
                    value = io.inb(port) | (1 << irq);
                    io.outb(port, value);   
                }

                static inline void clear(std::uint8_t irq) {
                    drivers::io io;
                    std::uint16_t port;
                    std::uint8_t value;

                    if(irq < 8) {
                        port = PIC1_DATA;
                    } else {
                        port = PIC2_DATA;
                        irq -= 8;
                    }
                    value = io.inb(port) & ~(1 << irq);
                    io.outb(port, value);  
                }

            };
        };
    };
};