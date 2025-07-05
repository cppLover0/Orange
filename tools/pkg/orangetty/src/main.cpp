
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <pty.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <graphics/framebuffer.hpp>
#include <orangettyconfig.hpp>
#include <pipes/pipes.hpp>
#include <font/layout.hpp>
#include <font/font.hpp>
#include <tty/tty.hpp>

FBDev* main_framebuffer;
OrangeTTY_Config* cfg;
Pipes* piping;
TTY* tty;

char is_shift_pressed = 0;
char is_ctrl_pressed = 0;

char* keybuffer;
int key_ptr = 0;

int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}

void keyhandler(int fd,uint8_t key) {
    struct termios tty_termios;
    tcgetattr(0,&tty_termios);

    if(key == SHIFT_PRESSED) {
        is_shift_pressed = 1;
        return;
    } else if(key == SHIFT_RELEASED) {
        is_shift_pressed = 0;
        return;
    }

    if(key == CTRL_PRESSED) {
        is_ctrl_pressed = 1;
        return;
    } else if(key == CTRL_RELEASED) {
        is_ctrl_pressed = 0;
        return;
    }

    if(!(key & (1 << 7))) {
        char layout_key;

        if(!is_shift_pressed)
            layout_key = en_layout_translation[key];
        else
            layout_key = en_layout_translation_shift[key];

        if(is_ctrl_pressed)
            layout_key = en_layout_translation[key] - ASCII_CTRL_OFFSET;

        if((tty_termios.c_lflag & ECHO) && is_printable(layout_key))
            write(STDOUT_FILENO,&layout_key,1);

        if((tty_termios.c_lflag & ICANON) && (is_printable(layout_key) || layout_key == '\b') && layout_key != 0) {
            
            if(layout_key != '\b') {
                keybuffer[key_ptr++] = layout_key;
                if(layout_key == '\n') {
                    write(fd,keybuffer,key_ptr);
                    key_ptr = 0;
                }
            } else {
                if(key_ptr) {
                    write(STDOUT_FILENO,"\b \b",sizeof("\b \b"));
                    keybuffer[key_ptr--] = '\0';
                }
            }

        } else {
            
            if(layout_key == '\n')
                layout_key = 13;

            keybuffer[key_ptr++] = layout_key;

            if(key_ptr >= tty_termios.c_cc[VMIN]) {
                write(fd,keybuffer,key_ptr);
                key_ptr = 0;
            }

        }
    }
} 

extern "C" void* __flanterm_malloc(size_t size) {
    return malloc(size);
}

extern "C" void __flanterm_free(void* ptr,size_t size) {
    free(ptr);
}

int main() {
    main_framebuffer = new FBDev(); /* Just creating variables and calling constructors */
    cfg = new OrangeTTY_Config();
    piping = new Pipes();
    tty = new TTY();
    keybuffer = (char*)malloc(256);

    memset(main_framebuffer->get_address(),0,main_framebuffer->get_finfo().line_length * main_framebuffer->get_vinfo().yres); /* Clearing screen */

    struct flanterm_context *ft_ctx = flanterm_fb_init(
        __flanterm_malloc,
        __flanterm_free,
        (uint32_t*)main_framebuffer->get_address(), main_framebuffer->get_vinfo().xres,main_framebuffer->get_vinfo().yres,
        main_framebuffer->get_finfo().line_length,
        main_framebuffer->get_vinfo().red.length,main_framebuffer->get_vinfo().red.offset,
        main_framebuffer->get_vinfo().green.length, main_framebuffer->get_vinfo().green.offset,
        main_framebuffer->get_vinfo().blue.length, main_framebuffer->get_vinfo().blue.offset,
        NULL,
        NULL, NULL,
        cfg->BackGround(), cfg->ForeGround(),
        cfg->BackGround(), cfg->BackGround(),
        unifont_arr, FONT_WIDTH, FONT_HEIGHT, 0,
        //0, 0, 0, 0,
        1,1,
        0
    );
    main_framebuffer->set_ft_ctx(ft_ctx); /* Now we can display text */
    piping->Ready(); /* Initialize some variables and create pipes */

    main_framebuffer->CreateWinSizeInfo(); /* Create winsize information */

    tty->Fetch(); /* Creating new tty */
    tty->SetTTY(STDIN_FILENO); /* Telling to OS what stdin is tty */
    tty->SetTTY(STDOUT_FILENO); /* Telling to OS what stdout is tty */
    tty->SetTTY(STDERR_FILENO); /* Telling to OS what stderr is tty */
    TTY::DisableOSHelp(); /* Now enabling isatty syscall */

    piping->Start(main_framebuffer,keyhandler); /* Starting receiving events from pipes */

    tty->SetupTermios(); /* Setup tty structs */

    main_framebuffer->SendWinSizeInfo(); /* Send winsize information to kernel */

    char buffer[2048];

    printf("Current directory %s\n",getcwd(buffer,2048));
    std::cout << "Executing bash\n";
    execl("/usr/bin/bash","/usr/bin/bash",NULL); 

}