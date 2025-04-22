
int strlen(char* str) {
    int len = 0; 
    while(str[len])
        len++;
    return len;
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

void _start() {

    print("Testing open syscall, opening (creating) file \"syscall_open_test.txt\"");

    int fd;
    open("syscall_open_test.txt",&fd);

    int bytes_read;
    long sw;
    char buf[1024];
    print("Reading \"/hello.txt\"");

    open("/hello.txt",&fd);

    read(fd,(void*)buf,1024,&bytes_read);

    print(buf);

    print("trying to seek right 2");
    seek(fd,2,0,&sw);

    read(fd,(void*)buf,1024,&bytes_read);

    print(buf);

    print("trying to seek right from current 3");
    seek(fd,3,1,&sw);

    read(fd,(void*)buf,1024,&bytes_read);

    print(buf);

    print("trying to seek left from end -2");
    seek(fd,-2,2,&sw);

    read(fd,(void*)buf,1024,&bytes_read);

    print(buf);

    tmpfs_dump();

    print("Welcome to the \033[38;2;255;165;0mOrange\033[38;2;255;255;255m !");

    exit(0);
    
    while(1) {
        print("I am alive ?");
    }

}