
#include <orange/dev.h>
#include <orange/io.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdint.h>

int main() {
    int masterinput = open("/dev/masterinput0",O_RDWR);
    
    liborange_setup_iopl_3();

    /* Just start polling ps/2 keyboard */
    while(1) {
        
        while(!(inb(0x64) & 1)) asm volatile ("pause"); 
        uint8_t scancode = inb(0x60);

        write(masterinput,&scancode,1);

    }
}