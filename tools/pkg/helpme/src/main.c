
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

struct termios orig_termios;

void disableRawMode() {
    printf("got atexit\n");
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON);
    raw.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
}

int main() {
    enableRawMode();
    printf("enter any key to continue\n");
    char key;
    read(STDIN_FILENO,&key,1);
}