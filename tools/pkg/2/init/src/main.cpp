
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>

#include <orange/dev.h>
#include <orange/io.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <font.hpp>

#include <etc.hpp>

#include <sys/wait.h>

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void start_all_drivers() {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(DIR_PATH);
    if (!dir) {
        exit(0);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (ends_with(entry->d_name, ".sys")) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s%s", DIR_PATH, entry->d_name);

            log(LEVEL_MESSAGE_OK,"Starting %s",entry->d_name);

            pid_t pid = fork();
            if (pid < 0) {
                continue;
            } else if (pid == 0) {
                execl(filepath, filepath, (char *)NULL);
            } 
        } else if(ends_with(entry->d_name,".wsys")) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s%s", DIR_PATH, entry->d_name);

            log(LEVEL_MESSAGE_OK,"Starting %s",entry->d_name);

            pid_t pid = fork();
            if (pid != 0) {
                char success = 0;
                while(!success) {
                    int r_pid = wait(NULL);
                    if(pid == r_pid)
                        success = 1;
                }
            } else if (pid == 0) {
                execl(filepath, filepath, (char *)NULL);
            } 
        }
    }

    closedir(dir);
}

int main() {
    fbdev::init();
    tty::init();
    log(LEVEL_MESSAGE_OK,"Trying to start drivers");
    start_all_drivers();
    log(LEVEL_MESSAGE_OK,"Initialization done");
    printf("\n");
    execl("/usr/bin/bash",NULL);
} 