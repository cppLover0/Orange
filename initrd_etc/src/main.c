
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

void _start() {

    
    print("Hello, World #1 !");
    print("Hello, World #2 !");
    print("Hello, World #3 !");
    print("Hello, World #4 !");
    print("Hello, World #5 !");

    for(int i = 0;i < 15;i++) {
        print("I am alive !");
    }

    print("Okay i died");
    exit();

    while(1) {
        print("I am alive ?");
    }

}