
#include <stdint.h>
#include <generic/tty/tty.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/VFS/devfs.hpp>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <drivers/serial/serial.hpp>
#include <other/string.hpp>
#include <other/log.hpp>
#include <drivers/ps2keyboard/ps2keyboard.hpp>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>

// flanterm_context* ft_ctx;

void __tty_helper_write(const char* msg) {
    extern flanterm_context* ft_ctx;
    flanterm_write(ft_ctx,(char*)msg,String::strlen((char*)msg));
}


termios_t tty_termios;

ps2keyboard_pipe_struct_t* head_ttypipe;
ps2keyboard_pipe_struct_t* last_ttypipe;

ps2keyboard_pipe_struct_t* __tty_allocate_pipe() {

    if(!head_ttypipe) {
        head_ttypipe = new ps2keyboard_pipe_struct_t;
        head_ttypipe->is_used_anymore = 0;
        last_ttypipe = head_ttypipe;
    }

    ps2keyboard_pipe_struct_t* current = head_ttypipe;
    while(current) {
        if(!current->is_used_anymore)
            break;
        current = current->next;
    }

    if(!current) {
        current = new ps2keyboard_pipe_struct_t;
        last_ttypipe->next = current;
        last_ttypipe = current;
    }

    return current;


}

void __tty_send_ipc(uint8_t keycode) {

    if(!head_ttypipe) {
        head_ttypipe = new ps2keyboard_pipe_struct_t;
        head_ttypipe->is_used_anymore = 0;
    }
    
    ps2keyboard_pipe_struct_t* current = head_ttypipe;
    while(current) { 

        if(current->is_used_anymore && current->pipe) {
            current->pipe->buffer[current->pipe->buffer_size] = keycode;
            current->pipe->buffer_size++;
        }
        current = current->next;
    }

}

void __tty_remove_ipc() {
    if(!head_ttypipe) {
        head_ttypipe = new ps2keyboard_pipe_struct_t;
        head_ttypipe->is_used_anymore = 0;
    }
    
    ps2keyboard_pipe_struct_t* current = head_ttypipe;
    while(current) { 

        if(current->is_used_anymore && current->pipe) {
            current->pipe->buffer[current->pipe->buffer_size] = '\0';
            current->pipe->buffer_size--;
        }
        current = current->next;
    }
}

void __tty_end_ipc() {
    //Log(LOG_LEVEL_DEBUG,"Ending ipc \n");
    ps2keyboard_pipe_struct_t* current = head_ttypipe;
    while(current) { 

        if(current->is_used_anymore && current->pipe) {
            if(current->pipe->type == PIPE_WAIT)
                current->is_used_anymore = 0;

            //Log(LOG_LEVEL_DEBUG,"Bufer size %d\n",current->pipe->buffer_size);

            current->pipe->is_received = 0; 
        }
        current = current->next;
    }

}


int tty_askforpipe(pipe_t* pipe) {

    ps2keyboard_pipe_struct_t* pip = __tty_allocate_pipe();

    pip->pipe = pipe;
    pip->is_used_anymore = 1;
    if(tty_termios.c_lflag & ICANON)
        pip->pipe->type = PIPE_WAIT;
    else
        pip->pipe->type = PIPE_INSTANT;    
    pip->pipe->is_used = 1;

    return 0;

}

int tty_write(char* buffer,uint64_t size) {
    extern flanterm_context* ft_ctx;
    flanterm_write(ft_ctx,buffer,size);
    Serial::WriteArray((uint8_t*)buffer,size);
    return 0;
}

int tty_ioctl(unsigned long request, void* arg, void* result) {

    switch(request) {
        case TCGETS:
            String::memcpy(arg,&tty_termios,sizeof(termios_t));
            break;
        case TCSETS:
            String::memcpy(&tty_termios,arg,sizeof(termios_t));
            break;
        case TIOCGWINSZ:
            winsize_t buf;
            extern flanterm_context* ft_ctx;
            flanterm_get_dimensions(ft_ctx,(size_t*)&buf.ws_col,(size_t*)&buf.ws_row);
            String::memcpy(arg,&buf,sizeof(winsize_t));
            break;
        default:
            return 38;
    }

    return 0;

}

int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}

int p = 0;

void __tty_receive_ipc(uint8_t keycode) {
    extern flanterm_context* ft_ctx;

    if(keycode != 127) {
        if(keycode == 13)
            keycode = '\n';

        if((tty_termios.c_lflag & ECHO) && is_printable((char)keycode)) {
            flanterm_write(ft_ctx,(char*)&keycode,1);
            Serial::Write(keycode);
            p++;
        }

        if(tty_termios.c_lflag & ICANON) {
            if(is_printable((char)keycode))
                __tty_send_ipc(keycode);
            if((char)keycode == '\n') {
                __tty_end_ipc();
                p = 0;
            }
        } else {
            p = 0;
            __tty_send_ipc(keycode);
            __tty_end_ipc();
        }
    } else {
        if(p != 0) {
            __tty_remove_ipc();
            __tty_helper_write("\b \b");
            Serial::WriteArray((uint8_t*)"\b \b",String::strlen("\b \b"));
            p--;
        }
    }

}

void TTY::Init() {
    extern flanterm_context* ft_ctx;

    devfs_reg_device("/tty",tty_write,0,tty_askforpipe,0,tty_ioctl);
    __tty_helper_write("\033[2J \033[1;1H");
    String::memset(&tty_termios,0,sizeof(termios_t));
    tty_termios.c_cc[VMIN] = 1;
    tty_termios.c_lflag = ECHO | ICANON;
    tty_termios.c_cflag = CS8 | CREAD | HUPCL;
    tty_termios.c_oflag = OPOST | ONLCR;
    tty_termios.c_iflag = ICRNL | IXON;

}