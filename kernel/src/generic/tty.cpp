#include <generic/tty.hpp>
#include <generic/devfs.hpp>
#include <generic/vfs.hpp>
#include <generic/lock/spinlock.hpp>
#include <utils/linux.hpp>
#include <klibc/string.hpp>
#include <cstdint>
#include <utils/assert.hpp>
#include <utils/flanterm.hpp>
#include <utils/errno.hpp>
#include <generic/scheduling.hpp>
#include <atomic>
#include <utils/flanterm.hpp>

#define PTMX "/ptmx"
#define TCGETS                   0x5401
#define TCSETS                   0x5402
#define TIOCGWINSZ               0x5413
#define TIOCSWINSZ               0x5414
#define TCGETS2 0x802c542a
#define TCSETS2 0x402c542b
#define TCSETSW2 0x402c542c
#define TCSETSF2 0x402c542d

std::atomic<std::uint32_t> tty_ptr = 0;
locks::spinlock tty_lock;

inline static int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}


signed long tty_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    tty::tty_arg* arg = (tty::tty_arg*)node->arg;
    return fd->other.is_master ? arg->writepipe->read((char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0) : arg->readpipe->ttyread((char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0);
}

signed long tty_write(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    bool is_slave = !fd->other.is_master;
    tty::tty_arg* arg = (tty::tty_arg*)node->arg;
    if(!is_slave) { // handle stdio logic from /dev/pty writing 
        for(std::size_t i = 0;i < count;i++) {

            char c = ((char*)buffer)[i];

            if(!(arg->termios.c_lflag & ICANON)) {
                if(c == '\n') c = 13;
            }

            if((arg->termios.c_lflag & ECHO) && (is_printable(c) || c == 13)) {
                if(c == 13) {
                    arg->writepipe->write("\n",1);
                } else if(c == '\n') 
                    arg->writepipe->write("\r",1);
                asm volatile("cli");
                arg->writepipe->write(&((char*)buffer)[i],1);
                asm volatile("cli"); 
            }

            if(c == '\b' || c == '\x7f' || c == '\x08') {
                if(arg->readpipe->size > 0) {
                    if(arg->termios.c_lflag & ECHO) {
                        const char* back = "\b \b";
                        arg->writepipe->write(back,klibc::strlen(back));
                        asm volatile("cli");
                    }
                    bool state = arg->readpipe->lock.lock();
                    arg->readpipe->read_counter--;
                    arg->readpipe->buffer[arg->readpipe->size--] = '\0';
                    arg->readpipe->lock.unlock(state); 
                }
            }

            if(is_printable(c) || !(arg->termios.c_lflag & ICANON)) {
                arg->readpipe->write(&c,1);
                asm volatile("cli");
            } else if(c == 13) {
                arg->readpipe->write("\n",1);
                asm volatile("cli");
            }

            if((c == '\n' || c == 13) && (arg->termios.c_lflag & ICANON)) {
                arg->readpipe->set_tty_ret(); 
            }

            if(!(arg->termios.c_lflag & ICANON) && arg->readpipe->size >= arg->termios.c_cc[VMIN]) {
                arg->readpipe->set_tty_ret(); 
            }

        }

        return count;
    } else if(is_slave) {
        for(std::size_t i = 0;i < count;i++) {
            char c = ((char*)buffer)[i]; 
            if(c == '\n') {
                arg->writepipe->write("\r\n",2);
            } else
                arg->writepipe->write(&c,1);
        }
        return count;
    }
    return 0;
}

std::int32_t tty_ioctl(devfs_node* node, std::uint64_t req, void* arg) {
    tty_lock.lock();
    tty::tty_arg* arg2 = (tty::tty_arg*)node->arg;
    switch(req) {
        case TCSETSW:
        case TCSETS: 
            klibc::memcpy(&arg2->termios, arg, sizeof(termiosold_t));
            tty_lock.unlock();
            return 0;
        case TCGETS:
            klibc::memcpy(arg, &arg2->termios, sizeof(termiosold_t));
            tty_lock.unlock();
            return 0;
        case TCSETSF2:
        case TCSETSW2:
        case TCSETS2: 
            klibc::memcpy(&arg2->termios, arg, sizeof(termios_t));
            tty_lock.unlock();
            return 0;
        case TCGETS2:
            klibc::memcpy(arg, &arg2->termios, sizeof(termios_t));
            tty_lock.unlock();
            return 0;
        case TIOCGWINSZ:
            klibc::memcpy(arg, &arg2->winsz, sizeof(winsize));
            tty_lock.unlock();
            return 0;
        case TIOCSWINSZ:
            klibc::memcpy(&arg2->winsz, arg, sizeof(winsize));
            tty_lock.unlock();
            return 0;
        case set_pgrp:
            arg2->pgrp = *(int*)arg;
            tty_lock.unlock();
            return 0;
        case get_pgrp:
            *(int*)arg = arg2->pgrp;
            tty_lock.unlock();
            return 0;
        case 0x5603: {
            vt_stat* out = (vt_stat*)arg;
            out->v_active = 0;
            out->v_state = (1 << 0);
            tty_lock.unlock();
            return 0;
        }
        case 0x541c:
            tty_lock.unlock();
            return -ENOTTY;

    }
    assert(0,"tty shitfuck req %lli (0x%p), arg 0x%p", req, req, arg);
    tty_lock.unlock();
    return -EINVAL;
}

bool tty_poll(file_descriptor* fd, devfs_node* node, vfs_poll_type type) {
    tty::tty_arg* arg = (tty::tty_arg*)node->arg;
    bool ret = false;
    bool is_slave = !fd->other.is_master;
    if(type == vfs_poll_type::pollin) {
        if(!is_slave) {
            ret = arg->writepipe->size != 0 ? 1 : 0;
        } else {
            ret = (arg->readpipe->tty_ret && arg->readpipe->size != 0) != 0 ? 1 : 0;
        }
    } else if(type == vfs_poll_type::pollout) {
        if(!is_slave) {
            ret = (std::uint64_t)arg->writepipe->size != arg->writepipe->total_size ? 1 : 0;
        } else {
            ret = (std::uint64_t)arg->readpipe->size != arg->readpipe->total_size ? 1 : 0;
        }
    }
    return ret;
}

std::int32_t tty_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    return 0;
}

#include <flanterm_backends/fb.h>
#include <flanterm.h>

extern flanterm_context* ft_ctx0;

void ptmx_open(file_descriptor* fd) {
    fd->other.is_master = true;
    tty::tty_arg* new_tty = new tty::tty_arg;
    new_tty->readpipe = new tty::pipe(0);
    new_tty->writepipe = new vfs::pipe(0);
    
    new_tty->readpipe->ttyflags = (termios_t*)&new_tty->termios;
    
    new_tty->termios.c_lflag |= (ICANON | ECHO);

    new_tty->termios.c_iflag = IGNPAR | ICRNL;
    new_tty->termios.c_oflag = OPOST;         
    new_tty->termios.c_cflag |= (CS8); 

    std::uint32_t num = tty_ptr++;
    fd->other.is_a_tty = true;
    fd->other.tty_num = num;

    // ktty
    if(num == 0) {
        std::size_t cols = 0;
        std::size_t rows = 0;
        flanterm_get_dimensions(ft_ctx0,&cols,&rows);
        new_tty->winsz.ws_col = cols;
        new_tty->winsz.ws_row = rows;
    }

    char buffer[256];
    klibc::memset(buffer,0,256);

    klibc::__printfbuf(buffer, 256, "/pts/%d", num);

    devfs_node* node = devfs::create(true, buffer, new_tty, 0, 0, tty_open, tty_ioctl, tty_read, tty_write, tty_poll, nullptr);
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;
    //log("tty", "new ptmx open %s", buffer);
} 

std::int32_t ptmx_open(file_descriptor* fd, devfs_node* node) {
    (void)node;
    ptmx_open(fd);
    return 0;
}

file_descriptor ktty_fd = {};
file_descriptor slave_ktty_fd = {};

vfs::pipe* ktty_pipe = nullptr;

char is_shift_pressed = 0;
char is_ctrl_pressed = 0;

const char en_layout_translation[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

const char en_layout_translation_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

#define ASCII_CTRL_OFFSET 96

#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA
#define CTRL_PRESSED 29
#define CTRL_RELEASED 157

static bool is_e0_prefix = false; 

static void doKeyWork(uint8_t key) {

    if (key == 0xE0) {
        is_e0_prefix = true;
        return;
    }

    if(key == SHIFT_PRESSED) { is_shift_pressed = 1; return; }
    if(key == SHIFT_RELEASED) { is_shift_pressed = 0; return; }
    if(key == CTRL_PRESSED) { is_ctrl_pressed = 1; return; }
    if(key == CTRL_RELEASED) { is_ctrl_pressed = 0; return; }

    if(!(key & 0x80)) {
        if (is_e0_prefix) {
            is_e0_prefix = false; 
            
            const char* sequence = nullptr;
            switch (key) {
                case 0x48: sequence = "\e[A"; break; 
                case 0x50: sequence = "\e[B"; break; 
                case 0x4D: sequence = "\e[C"; break; 
                case 0x4B: sequence = "\e[D"; break;
            }

            if (sequence) {
                ktty_fd.vnode.write(&ktty_fd, (char*)sequence, 3);
                return;
            }
        }

        if (key < sizeof(en_layout_translation)) {
            char layout_key;
            if(!is_shift_pressed)
                layout_key = en_layout_translation[key];
            else
                layout_key = en_layout_translation_shift[key];
            
            if(is_ctrl_pressed && layout_key >= 64)
                layout_key -= ASCII_CTRL_OFFSET;

            if (layout_key != '\0') {
                ktty_fd.vnode.write(&ktty_fd, (char*)&layout_key, 1);
            }
        }
    } else {
        is_e0_prefix = false;
    }
}


void tty_work(void* arg) {
    (void)arg;
    arch::disable_interrupts();
    assert(ktty_fd.vnode.read, "DSFBmvfdnHJA124MFk!");
    while(true) {
        char buffer[512];
        klibc::memset(buffer,0,512);
        signed long c = ktty_fd.vnode.read(&ktty_fd, buffer, 512);
        if(c > 0) {
            utils::flanterm::write(buffer, c);
        }

        klibc::memset(buffer,0,512);
        signed long iz = ktty_pipe->read(buffer, 512, true);
        if(iz > 0) {
            for(int i = 0;i < iz;i++) {
                doKeyWork(buffer[i]);
            }
        }
        process::yield();
    }
}

void tty::init() {
    devfs::create(false, (char*)"/ptmx", nullptr, 0, 0, ptmx_open, nullptr, nullptr, nullptr, nullptr, nullptr);
    std::int32_t status = vfs::open(&ktty_fd, (char*)"/dev/ptmx", false, false);
    assert(status == 0, "failed to open /dev/ptmx device (status is %d)")
    devfs_node* nod = (devfs_node*)ktty_fd.fs_specific.tmpfs_pointer;
    log("tty", "kernel tty node is %s (0x%p)", nod->path, nod);
    status = vfs::open(&slave_ktty_fd, (char*)"/dev/pts/0", false, false);
    assert(status == 0, "failed to open slave tty device (status is %d)",status);
    ktty_pipe = new vfs::pipe(O_NONBLOCK);
    ktty_fd.flags |= O_NONBLOCK;
    thread* thr = process::kthread(tty_work, nullptr);
    process::wakeup(thr);
}