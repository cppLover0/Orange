
#include <stdint.h>
#include <generic/tty/tty.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/VFS/devfs.hpp>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <drivers/serial/serial.hpp>
#include <other/string.hpp>

// flanterm_context* ft_ctx;

void __tty_helper_write(const char* msg) {
    extern flanterm_context* ft_ctx;
    flanterm_write(ft_ctx,(char*)msg,String::strlen((char*)msg));
}

void __tty_receive_ipc(uint8_t keycode) {

}

int tty_write(char* buffer,uint64_t size) {
    extern flanterm_context* ft_ctx;
    flanterm_write(ft_ctx,buffer,size);
    Serial::WriteArray((uint8_t*)buffer,size);
    return 0;
}

void TTY::Init() {
    extern flanterm_context* ft_ctx;

    devfs_reg_device("/tty",tty_write,0,0,0,0);
    __tty_helper_write("\033[2J \033[1;1H");

}