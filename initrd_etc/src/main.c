
char i[8];

void _start() {
    unsigned long long* l = &i[0];
    unsigned long long m = *l;
    m += 10;

    m -= 10;
    
    int i = 0;
    while(1) {
        asm volatile("nop");
        asm volatile("syscall");
        i++;
    }
}