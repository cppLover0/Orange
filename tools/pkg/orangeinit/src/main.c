#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int main() {
    execl("/usr/bin/orangetty",NULL);
}