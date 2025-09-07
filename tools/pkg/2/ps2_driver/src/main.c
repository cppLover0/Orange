
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

    liborange_pic_create_t irq_create_request;
    irq_create_request.flags = 0;
    irq_create_request.irq = 1;
    
    write(pic,&irq_create_request,sizeof(liborange_pic_create_t));
    close(pic);

    int kbd_irq = open("/dev/irq1",O_RDWR);
    
    char val = 0;
    while(1) {
        int count = read(kbd_irq,&val,1);
        if(count && val == 1) {
            write(kbd_irq,&val,1); /* Ask kernel to unmask this irq */
            while((inb(0x64) & 1)) { 
                uint8_t scancode = inb(0x60);
                write(masterinput,&scancode,1);
            }
        }
    }
}