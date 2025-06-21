
#include <arch/x86_64/scheduling/scheduling.hpp>

#pragma once

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS     32

typedef struct {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
	speed_t ibaud;
	speed_t obaud;
} __attribute__((packed)) termios_t;

typedef struct pipe {

    char* buffer;
    char* old_buffer;
    uint64_t buffer_size;
    
    uint64_t buffer_read_ptr;

    process_t* parent;

    int type;
    int is_used;
    char is_eof;
    
    uint8_t free_block;
    char connected_pipes;

    uint32_t reserved;

    termios_t termios;

    _Atomic char is_received;

} __attribute__((packed)) pipe_t;