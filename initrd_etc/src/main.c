
int strlen(char* str) {
    int len = 0; 
    while(str[len])
        len++;
    return len;
}

char kmap[255] = {
    '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 
    '\0', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
    '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ', '*', 
    '\0', ' ', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', '\0', 
    '\0'
};

char __shift_pressed = 0;
char __last_key = 0;

char __ps_2_to_upper(char c) {
    
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    } 
    switch (c) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '`': return '~';
        default: break; 
    }
    if (c == '\'') {
        return '\"'; 
    }
    return c; 

}

#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA

char ps2_to_ascii(char keycodeps2) {

        unsigned char keycode = keycodeps2;

        if(keycode == SHIFT_PRESSED)
 
            __shift_pressed = 1;

        if(keycode == SHIFT_RELEASED)
            __shift_pressed = 0;

        if(keycode <= 0x58) {
 
            keycode = kmap[keycode];

            if(__shift_pressed)
                keycode = __ps_2_to_upper(keycode);
        }

        if(keycode & (1 << 7))
            return '\0';

        return keycode;
 
    return 0;
 
}

void print(char* str) {
    asm volatile("syscall" : : "a"(2), "D"(str), "S"(strlen(str)) : "rcx", "r11");
}

void exit(int code) {
    asm volatile("syscall" : : "a"(1), "D"(code) : "rcx", "r11");
}

void open(const char* filename,int* fd) {
    asm volatile("syscall" : : "a"(7), "D"(filename), "S"(fd) : "rcx", "r11");
}

void tmpfs_dump() {
    asm volatile("syscall" : : "a"(6) : "rcx", "r11");
}

void seek(int fd,long off,int wrencle,long* off_new) {
    asm volatile("syscall" : "=D"(off_new) : "a"(8), "D"(fd), "S"(off), "d"(wrencle): "rcx", "r11");
}

void read(int fd, void* buf, unsigned long count, long* bytes_read) {
    asm volatile("syscall" : "=D"(bytes_read) : "a"(9), "D"(fd), "S"(buf), "d"(count): "rcx", "r11");
}

void write(int fd, void* buf, unsigned long count, long* bytes_written) {
    asm volatile("syscall" : "=D"(bytes_written) : "a"(10), "D"(fd), "S"(buf), "d"(count): "rcx", "r11");
}

void close(int fd) {
    asm volatile("syscall" : : "a"(11) : "rcx", "r11");
}


void _start() {

    print("Testing open syscall, opening (creating) file \"syscall_open_test.txt\"");

    int fd;
    open("syscall_open_test.txt",&fd);

    int bytes_read;
    long sw;
    long bw;
    char buf[1024];
    print("Reading \"/hello.txt\" with seek 2 left");

    open("../hello.txt",&fd);

    seek(fd,-2,2,&sw);
    read(fd,(void*)buf,512,&bytes_read);

    print(buf);

    print("writing \"hello, world\" to file and resetting seek");

    seek(fd,0,0,&sw);

    write(fd,"hello, world",12,&bw);

    print("Reading \"/hello.txt\" again");

    read(fd,(void*)buf,512,&bytes_read);

    print(buf);

    close(fd);

    print("Closing and creating new file \"new_file.txt\"");

    open("new_file.txt",&fd);

    print("Writing \"hello\" and reading");

    write(fd,"hello",5,&bw);

    read(fd,buf,512,&sw);

    print(buf);

    print("closing, after open and reading again");

    close(fd);

    open("new_file.txt",&fd);
    read(fd,buf,512,&sw);

    print(buf);

    close(fd);

    tmpfs_dump();

    print("Welcome to the \033[38;2;255;165;0mOrange\033[38;2;255;255;255m !");

    print("Now just type a some text !");

    char char_buffer = 0;
    while(1) {
        read(0,&char_buffer,1,&sw);

        char_buffer = ps2_to_ascii(char_buffer);
        write(1,&char_buffer,1,&sw);

    }

}