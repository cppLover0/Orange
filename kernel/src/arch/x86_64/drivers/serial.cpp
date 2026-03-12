#include <arch/x86_64/drivers/io.hpp>
#include <arch/x86_64/drivers/serial.hpp>

#define PORT 0x3f8
int is_success_init_serial = 0;

void x86_64::serial::init() {
    x86_64::io::outb(PORT + 1, 0x00);   
    x86_64::io::outb(PORT + 3, 0x80);    
    x86_64::io::outb(PORT + 0, 0x03);   
    x86_64::io::outb(PORT + 1, 0x00);    
    x86_64::io::outb(PORT + 3, 0x03); 
    x86_64::io::outb(PORT + 2, 0xC7);   
    x86_64::io::outb(PORT + 4, 0x0B);    
    x86_64::io::outb(PORT + 4, 0x1E);    
    x86_64::io::outb(PORT + 0, 0xAE);    

    if(x86_64::io::inb(PORT + 0) != 0xAE) {
        is_success_init_serial = 0;
        return;
    }

    is_success_init_serial = 1;
    x86_64::io::outb(PORT + 4, 0x0F);
}

int is_transmit_empty() {
   return x86_64::io::inb(PORT + 5) & 0x20;
}

void x86_64::serial::write(char c) {
    if(!is_success_init_serial)
        return;
    while (is_transmit_empty() == 0);
    x86_64::io::outb(PORT,c);
}

int serial_received() {
    return x86_64::io::inb(PORT + 5) & 1;
}

char x86_64::serial::read() {
    if(!is_success_init_serial)
        return 0;
    while (serial_received() == 0);

    return x86_64::io::inb(PORT);
}
