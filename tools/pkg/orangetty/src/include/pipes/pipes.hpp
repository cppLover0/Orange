
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>


#ifndef ORANGE_PIPES
#define ORANGE_PIPES

#include <graphics/framebuffer.hpp>

class Pipes {
private:
    int read_fd;
    int write_fd;
    int stderr_fd;
    int read_id;
    int write_id;
    int stderr_id;
    int serial_fd;
    FBDev* fb;

    void (*key_handler)(int fd,uint8_t key);

    void start_reading_pipe() { /* Fetching events from pipe */
        char buffer[1024];
        serial_fd = open("/dev/serial0",O_WRONLY); /* Opening serial device to write */
        if(serial_fd < 0)
            fb->WriteString("Failed to open serial device\n");
        memset(buffer,0,sizeof(buffer));
        while(1) {
            int c = read(read_fd,buffer,16);
            if(c) {
                write(serial_fd,buffer,c);
                fb->WriteData(buffer,c);
            }
           
        }
    }

    void start_writing_pipe() {
        char buffer[1024];
        int inputfd = open("/dev/input0",O_RDWR);
        if(inputfd < 0) {
            fb->WriteString("Failed to open input device\n");
            exit(-1);
        }
        memset(buffer,0,sizeof(buffer));
        while(1) {
            int c = read(inputfd,buffer,16);
            if(c) {
                for(int i = 0;i < c;i++) {
                    key_handler(write_fd,buffer[i]);
                }
            }
            memset(buffer,0,sizeof(buffer));
        }
    }

    void start_stderr() { 
        char buffer[1024];
        memset(buffer,0,sizeof(buffer));
        while(1) {
            int c = read(stderr_fd,buffer,16);
            if(c) {
                write(STDOUT_FILENO,buffer,c);
            }
            memset(buffer,0,sizeof(buffer));
        }
    }

public:

    void Ready() {
        int stdoutfd[2];
        pipe(stdoutfd);

        int stdinfd[2];
        pipe(stdinfd);

        int stderrfd[2];
        pipe(stderrfd);

        read_fd = stdoutfd[0];
        write_fd = stdinfd[1];

        stderr_fd = stderrfd[0];
        dup2(stdoutfd[1], STDOUT_FILENO);
        dup2(stdinfd[0],STDIN_FILENO);
        dup2(stderrfd[1],STDERR_FILENO);

        close(stdoutfd[1]);
        close(stdinfd[0]);
        close(stderrfd[1]);

    }

    void Start(FBDev* fbdev,void (*handler)(int fd,uint8_t key)) {
        key_handler = handler;
        fb = fbdev;
        int pid = fork();

        if(pid == 0) {
            read_id = getpid();
            start_reading_pipe();
            exit(0);
        }

        pid = fork();

        if(pid == 0) {
            write_id = getpid();
            start_writing_pipe();
            exit(0);
        }

        pid = fork();

        if(pid == 0) {
            stderr_id = getpid();
            start_stderr();
            exit(0);
        }

    }

    int get_read() {
        return read_fd;
    }

    int get_write() {
        return write_fd;
    }

    int get_read_proc() {
        return read_id;
    }

    int get_write_proc() {
        return write_id;
    }

    Pipes() {
        read_fd = 0;
        write_fd = 0;
        read_id = 0;
        write_id = 0;
    }
};

#endif