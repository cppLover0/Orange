
#include <orange/dev.h>
#include <orange/io.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdint.h>

#include <string.h>

int main() {
    int masterinput = open("/dev/masterinput0",O_RDWR);
    
    liborange_setup_iopl_3();

    int pic = open("/dev/pic",O_RDWR);

    short buf[5];
    memset(buf,0,sizeof(buf));
    buf[0] = 1;
    
    write(pic,buf,10);
    close(pic);

    int kbd_irq = open("/dev/irq1",O_RDWR);
    
    char val = 0;
    while(1) {
        val = 0;
        int count = read(kbd_irq,&val,1);
        if(count && val == 1) {
            while(!(inb(0x64) & 1)) asm volatile ("pause"); 
            uint8_t scancode = inb(0x60);

            write(masterinput,&scancode,1);
        }
    }
}