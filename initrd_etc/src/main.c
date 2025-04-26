
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

char* itoa(long value, char* str, int base ) {
    char * rc;
    char * ptr;
    char * low;
    if ( base < 2 || base > 36 )
    {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    if ( value < 0 && base == 10 )
    {
        *ptr++ = '-';
    }
    low = ptr;
    do
    {
        *ptr++ = "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[35 + value % base];
        value /= base;
    } while ( value );
    *ptr-- = '\0';
    while ( low < ptr )
    {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}


void _start() {

    int sw;
    long bw;

    print("Welcome to the \033[38;2;255;165;0mOrange\033[38;2;255;255;255m !");

    print("Now just type a some text !");

    char char_buffer = 0;

    char buf2[128];

    while(1) {
        read(0,&char_buffer,1,&sw);

        char_buffer = ps2_to_ascii(char_buffer);
        write(1,&char_buffer,1,&bw);

    }

}