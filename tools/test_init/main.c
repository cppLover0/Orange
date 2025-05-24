
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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

char __ps2_read(char keycodeps2) {

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

void forkandexec(const char* name) {
    int ia = fork();
    printf("Fokr %d\n",ia);
    if(ia == 0) {
        printf("Execin\n");
        execl(name,name,NULL);
    } 

    return;

}

#include <fcntl.h>

int main() {
    
    printf("Executing bash\n");
    execl("/usr/bin/bash","/usr/bin/bash",NULL);


}
