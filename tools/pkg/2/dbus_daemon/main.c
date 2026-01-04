
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <orange/dev.h>
#include <orange/io.h>
#include <orange/log.h>

int main() {
    log(LEVEL_MESSAGE_INFO,"Starting DBus\n");
    system("dbus-daemon --system --fork");
    exit(0);
}