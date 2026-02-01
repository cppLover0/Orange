
#include <cstdint>
#include <generic/vfs/vfs.hpp>
#include <etc/list.hpp>

#pragma once

#define DEVFS_PACKET_CREATE_DEV 1
#define DEVFS_PACKET_READ_READRING 2
#define DEVFS_PACKET_WRITE_READRING 3 
#define DEVFS_PACKET_READ_WRITERING 4 
#define DEVFS_PACKET_WRITE_WRITERING 5
#define DEVFS_PACKET_CREATE_IOCTL 6 
#define DEVFS_PACKET_SIZE_IOCTL 7
#define DEVFS_PACKET_IOCTL 8
#define DEVFS_ENABLE_PIPE 9
#define DEVFS_SETUP_MMAP 10
#define DEVFS_PACKET_CREATE_PIPE_DEV 11
#define DEVFS_PACKET_ISATTY 12 
#define DEVFS_PACKET_SETUPTTY 13
#define DEVFS_GETSLAVE_BY_MASTER 14
#define DEVFS_PACKET_SETUP_RING_SIZE 15
#define devfs_packet_get_num 16

typedef unsigned int __u32;
typedef unsigned short __u16;

struct fb_fix_screeninfo {
	char id[16];			/* identification string eg "TT Builtin" */
	unsigned long smem_start;	/* Start of frame buffer mem */
					/* (physical address) */
	__u32 smem_len;			/* Length of frame buffer mem */
	__u32 type;			/* see FB_TYPE_*		*/
	__u32 type_aux;			/* Interleave for interleaved Planes */
	__u32 visual;			/* see FB_VISUAL_*		*/ 
	__u16 xpanstep;			/* zero if no hardware panning  */
	__u16 ypanstep;			/* zero if no hardware panning  */
	__u16 ywrapstep;		/* zero if no hardware ywrap    */
	__u32 line_length;		/* length of a line in bytes    */
	unsigned long mmio_start;	/* Start of Memory Mapped I/O   */
					/* (physical address) */
	__u32 mmio_len;			/* Length of Memory Mapped I/O  */
	__u32 accel;			/* Indicate to driver which	*/
					/*  specific chip/card we have	*/
	__u16 capabilities;		/* see FB_CAP_*			*/
	__u16 reserved[2];		/* Reserved for future compatibility */
};

/* Interpretation of offset for color fields: All offsets are from the right,
 * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
 * can use the offset as right argument to <<). A pixel afterwards is a bit
 * stream and is written to video memory as that unmodified.
 *
 * For pseudocolor: offset and length should be the same for all color
 * components. Offset specifies the position of the least significant bit
 * of the palette index in a pixel value. Length indicates the number
 * of available palette entries (i.e. # of entries = 1 << length).
 */
struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */ 
					/* right */ 
};

#define FB_NONSTD_HAM		1	/* Hold-And-Modify (HAM)        */
#define FB_NONSTD_REV_PIX_IN_B	2	/* order of pixels in each byte is reversed */

#define FB_ACTIVATE_NOW		0	/* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN	1	/* activate on next open	*/
#define FB_ACTIVATE_TEST	2	/* don't set, round up impossible */
#define FB_ACTIVATE_MASK       15
					/* values			*/
#define FB_ACTIVATE_VBL	       16	/* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL     32	/* change colormap on vbl	*/
#define FB_ACTIVATE_ALL	       64	/* change all VCs on this fb	*/
#define FB_ACTIVATE_FORCE     128	/* force apply even when no change*/
#define FB_ACTIVATE_INV_MODE  256       /* invalidate videomode */
#define FB_ACTIVATE_KD_TEXT   512       /* for KDSET vt ioctl */

#define FB_ACCELF_TEXT		1	/* (OBSOLETE) see fb_info.flags and vc_mode */

#define FB_SYNC_HOR_HIGH_ACT	1	/* horizontal sync high active	*/
#define FB_SYNC_VERT_HIGH_ACT	2	/* vertical sync high active	*/
#define FB_SYNC_EXT		4	/* external sync		*/
#define FB_SYNC_COMP_HIGH_ACT	8	/* composite sync high active   */
#define FB_SYNC_BROADCAST	16	/* broadcast video timings      */
					/* vtotal = 144d/288n/576i => PAL  */
					/* vtotal = 121d/242n/484i => NTSC */
#define FB_SYNC_ON_GREEN	32	/* sync on green */

#define FB_VMODE_NONINTERLACED  0	/* non interlaced */
#define FB_VMODE_INTERLACED	1	/* interlaced	*/
#define FB_VMODE_DOUBLE		2	/* double scan */
#define FB_VMODE_ODD_FLD_FIRST	4	/* interlaced: top line first */
#define FB_VMODE_MASK		255

#define FB_VMODE_YWRAP		256	/* ywrap instead of panning     */
#define FB_VMODE_SMOOTH_XPAN	512	/* smooth xpan possible (internally used) */
#define FB_VMODE_CONUPDATE	512	/* don't update x/yoffset	*/

/*
 * Display rotation support
 */
#define FB_ROTATE_UR      0
#define FB_ROTATE_CW      1
#define FB_ROTATE_UD      2
#define FB_ROTATE_CCW     3

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

struct fb_var_screeninfo {
	__u32 xres;			/* visible resolution		*/
	__u32 yres;
	__u32 xres_virtual;		/* virtual resolution		*/
	__u32 yres_virtual;
	__u32 xoffset;			/* offset from virtual to visible */
	__u32 yoffset;			/* resolution			*/

	__u32 bits_per_pixel;		/* guess what			*/
	__u32 grayscale;		/* 0 = color, 1 = grayscale,	*/
					/* >1 = FOURCC			*/
	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/	

	__u32 nonstd;			/* != 0 Non standard pixel format */

	__u32 activate;			/* see FB_ACTIVATE_*		*/

	__u32 height;			/* height of picture in mm    */
	__u32 width;			/* width of picture in mm     */

	__u32 accel_flags;		/* (OBSOLETE) see fb_info.flags */

	/* Timing: All values in pixclocks, except pixclock (of course) */
	__u32 pixclock;			/* pixel clock in ps (pico seconds) */
	__u32 left_margin;		/* time from sync to picture	*/
	__u32 right_margin;		/* time from picture to sync	*/
	__u32 upper_margin;		/* time from sync to picture	*/
	__u32 lower_margin;
	__u32 hsync_len;		/* length of horizontal sync	*/
	__u32 vsync_len;		/* length of vertical sync	*/
	__u32 sync;			/* see FB_SYNC_*		*/
	__u32 vmode;			/* see FB_VMODE_*		*/
	__u32 rotate;			/* angle we rotate counter clockwise */
	__u32 colorspace;		/* colorspace for FOURCC-based modes */
	__u32 reserved[4];		/* Reserved for future compatibility */
};

struct	winsize {
 	unsigned short	 	ws_row;	 
 	unsigned short	 	ws_col;	 
 	unsigned short	 	ws_xpixel;	
/* unused */
 	unsigned short	 	ws_ypixel;	
/* unused */
};

#define TCGETS                   0x5401
#define TCSETS                   0x5402
#define TIOCGWINSZ               0x5413
#define TIOCSWINSZ               0x5414

namespace vfs {

    typedef struct {
        std::uint64_t is_pipe : 1;
        std::uint64_t is_pipe_rw : 1;
    } __attribute__((packed)) devfs_pipe_p_t;

    typedef struct {
        union {
            struct {
                std::uint8_t request;
                std::uint8_t* cycle;
                std::uint32_t* queue;
                std::uint32_t size;
                std::uint64_t value;
            };
            struct {
                std::uint8_t request;
                std::uint64_t ioctlreq;
                std::uint64_t arg;
            } ioctl;
            struct {
                std::uint8_t request;
                std::uint32_t writereg;
                std::uint32_t readreg;
                std::uint32_t size;
                std::uint64_t pointer;
            } create_ioctl;
            struct {
                std::uint8_t request;
                std::uint8_t pipe_target;
                std::uint64_t pipe_pointer;
            } enable_pipe;
            struct {
                std::uint8_t request;
                std::uint64_t dma_addr;
                std::uint64_t size;
                std::uint64_t flags;
            } setup_mmap;
        };
    } __attribute__((packed)) devfs_packet_t; /* User-Kernel interaction packet */

    typedef struct {
        std::uint32_t read_req;
        std::uint32_t write_req;
        std::uint32_t size;
        void* pointer_to_struct;
    } __attribute__((packed)) devfs_ioctl_packet_t;

    typedef struct devfs_node {
        devfs_pipe_p_t open_flags;
        union {
            struct {
                Lists::Ring* readring;
                Lists::Ring* writering;
            };
            struct {
                pipe* readpipe;
                pipe* writepipe;
            };
        };
        devfs_ioctl_packet_t ioctls[32];
        std::uint64_t pipe0;
        std::uint64_t mmap_base;
        std::uint64_t mmap_size;
        std::uint64_t mmap_flags;
        std::int32_t dev_num;
        std::int8_t is_tty;

        std::int64_t (*read)(userspace_fd_t* fd, void* buffer, std::uint64_t count);
        std::int64_t (*write)(userspace_fd_t* fd, void* buffer, std::uint64_t size);

        std::int64_t (*slave_write)(userspace_fd_t* fd, void* buffer, std::uint64_t size);

        std::int32_t (*ioctl)(userspace_fd_t* fd, unsigned long req, void *arg, int *res);
        std::int32_t (*open)(userspace_fd_t* fd, char* path);
        int mode;

        int pgrp;

        termios_t* term_flags;

        struct devfs_node* next;
        char masterpath[256];
        char slavepath[256];
    } devfs_node_t;

    static_assert(sizeof(devfs_node_t) < 4096,"devfs_node_t is higher than fucking page size");

    class devfs {
    public:
        static void mount(vfs_node_t* node);
        static std::int64_t send_packet(char* path,devfs_packet_t* packet);
    };
};
