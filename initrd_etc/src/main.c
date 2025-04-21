
int strlen(char* str) {
    int len = 0; 
    while(str[len])
        len++;
    return len;
}

void print(char* str) {
    asm volatile("syscall" : : "a"(2), "D"(str), "S"(strlen(str)) : "rcx", "r11");
}

void exit() {
    asm volatile("syscall" : : "a"(1), "D"(0) : "rcx", "r11");
}

void open(const char* filename,int* fd) {
    asm volatile("syscall" : : "a"(7), "D"(filename), "S"(fd) : "rcx", "r11");
}

void tmpfs_dump() {
    asm volatile("syscall" : : "a"(6) : "rcx", "r11");
}

void _start() {

    print("Testing open syscall, opening (creating) file \"syscall_open_test.txt\"");

    int fd;
    open("syscall_open_test.txt",&fd);

    tmpfs_dump();

    print("Welcome to the \033[38;2;255;165;0mOrange\033[38;2;255;255;255m !");

    exit(0);
    
    while(1) {
        print("I am alive ?");
    }

}