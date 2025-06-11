
#include <stdint.h>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/interrupts/syscalls/ipc/pipe.hpp>
#include <generic/VFS/vfs.hpp>

#pragma once

#define FD_NONE 0
#define FD_FILE 1
#define FD_PIPE 2

#define PIPE_WAIT 0
#define PIPE_INSTANT 1

/* indices for the c_cc array in struct termios */
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

/* bitwise flags for c_iflag in struct termios */
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IUCLC 0001000
#define IXON 0002000
#define IXANY 0004000
#define IXOFF 0010000
#define IMAXBEL 0020000
#define IUTF8 0040000

/* bitwise flags for c_oflag in struct termios */
#define OPOST 0000001
#define OLCUC 0000002
#define ONLCR 0000004
#define OCRNL 0000010
#define ONOCR 0000020
#define ONLRET 0000040
#define OFILL 0000100
#define OFDEL 0000200

#define NLDLY 0000400
#define NL0 0000000
#define NL1 0000400

#define CRDLY 0003000
#define CR0 0000000
#define CR1 0001000
#define CR2 0002000
#define CR3 0003000

#define TABDLY 0014000
#define TAB0 0000000
#define TAB1 0004000
#define TAB2 0010000
#define TAB3 0014000

#define BSDLY 0020000
#define BS0 0000000
#define BS1 0020000

#define FFDLY 0100000
#define FF0 0000000
#define FF1 0100000

#define VTDLY 0040000
#define VT0 0000000
#define VT1 0040000

/* bitwise constants for c_cflag in struct termios */
#define CSIZE 0000060
#define CS5 0000000
#define CS6 0000020
#define CS7 0000040
#define CS8 0000060

#define CSTOPB 0000100
#define CREAD 0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL 0002000
#define CLOCAL 0004000

/* bitwise constants for c_lflag in struct termios */
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define TOSTOP 0000400
#define IEXTEN 0100000

#define EXTA    0000016
#define EXTB    0000017
#define CBAUD   0010017
#define CBAUDEX 0010000
#define CIBAUD  002003600000
#define CMSPAR  010000000000
#define CRTSCTS 020000000000

#define XCASE   0000004
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define EXTPROC 0200000

#define TCGETS                   0x5401
#define TCSETS                   0x5402
#define TCSETSW                  0x5403
#define TCSETSF                  0x5404
#define TCGETA                   0x5405
#define TCSETA                   0x5406
#define TCSETAW                  0x5407
#define TCSETAF                  0x5408
#define TCSBRK                   0x5409
#define TCXONC                   0x540A
#define TCFLSH                   0x540B
#define TIOCEXCL                 0x540C
#define TIOCNXCL                 0x540D
#define TIOCSCTTY                0x540E
#define TIOCGPGRP                0x540F
#define TIOCSPGRP                0x5410
#define TIOCOUTQ                 0x5411
#define TIOCSTI                  0x5412
#define TIOCGWINSZ               0x5413
#define TIOCSWINSZ               0x5414
#define TIOCMGET                 0x5415
#define TIOCMBIS                 0x5416
#define TIOCMBIC                 0x5417
#define TIOCMSET                 0x5418
#define TIOCGSOFTCAR             0x5419
#define TIOCSSOFTCAR             0x541A
#define FIONREAD                 0x541B
#define TIOCLINUX                0x541C
#define TIOCCONS                 0x541D
#define TIOCGSERIAL              0x541E
#define TIOCSSERIAL              0x541F
#define TIOCPKT                  0x5420
#define FIONBIO                  0x5421
#define TIOCNOTTY                0x5422
#define TIOCSETD                 0x5423
#define TIOCGETD                 0x5424
#define TCSBRKP                  0x5425
#define TIOCSBRK                 0x5427
#define TIOCCBRK                 0x5428
#define TIOCGSID                 0x5429
#define TCGETS2                  3
#define TCSETS2                  3
#define TCSETSW2                 3
#define TCSETSF2                 3
#define TIOCGRS485               0x542E
#define TIOCSRS485               0x542F
#define TIOCGPTN                 3
#define TIOCSPTLCK               3
#define TIOCGDEV                 3
#define TCGETX                   0x5432
#define TCSETX                   0x5433
#define TCSETXF                  0x5434
#define TCSETXW                  0x5435
#define TIOCSIG                  0x36
#define TIOCVHANGUP              0x5437
#define TIOCGPKT                 3
#define TIOCGPTLCK               3
#define TIOCGEXCL                3
#define TIOCGPTPEER              3
#define TIOCGISO7816             3
#define TIOCSISO7816             3
#define FIONCLEX                 0x5450
#define FIOCLEX                  0x5451
#define FIOASYNC                 0x5452
#define TIOCSERCONFIG            0x5453
#define TIOCSERGWILD             0x5454
#define TIOCSERSWILD             0x5455
#define TIOCGLCKTRMIOS           0x5456
#define TIOCSLCKTRMIOS           0x5457
#define TIOCSERGSTRUCT           0x5458
#define TIOCSERGETLSR            0x5459
#define TIOCSERGETMULTI          0x545A
#define TIOCSERSETMULTI          0x545B
#define TIOCMIWAIT               0x545C
#define TIOCGICOUNT              0x545D
#define TIOCPKT_DATA             0
#define TIOCPKT_FLUSHREAD        1
#define TIOCPKT_FLUSHWRITE       2
#define TIOCPKT_STOP             4
#define TIOCPKT_START            8
#define TIOCPKT_NOSTOP           16
#define TIOCPKT_DOSTOP           32
#define TIOCPKT_IOCTL            64
#define TIOCSER_TEMT	            0x01

typedef struct {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
} __attribute__((packed)) winsize_t;

#define XTABS 0014000

#define PIPE_SIDE_WRITE 1
#define PIPE_SIDE_READ 2

typedef struct fd_struct {  
    int index;

    process_t* proc;
    struct fd_struct* parent;
    struct fd_struct* next;
    
    long seek_offset;

    char type;

    char pipe_side;
    
    uint8_t is_pipe_pointer;
    uint8_t old_is_pipe_pointer;
    pipe_t pipe;
    pipe_t* old_p_pipe;
    pipe_t* p_pipe;

    uint64_t flags;
    filestat_t reserved_stat;

    char is_pipe_dup2;

    char old_type;

    char path_point[1024];

} __attribute__((packed)) fd_t;

class FD {
public:
    static int Create(process_t* proc,char is_pipe);
    static fd_t* Search(process_t* proc,int index);
};