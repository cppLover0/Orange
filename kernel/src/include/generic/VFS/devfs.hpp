
#include <stdint.h>
#include <generic/VFS/vfs.hpp>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>

#pragma once

#define __u32 uint32_t
#define __u16 uint16_t

// copied from my headers :^)

#define FBIOGET_VSCREENINFO	0x4600
#define FBIOGET_FSCREENINFO	0x4602

struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */ 
					/* right */ 
};

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

typedef struct {
	union {
		struct {
			char cycle;
			char reserved[3];
		};
		struct {
			char reserved0[2];
			char x;
			char y;
		} mousepacket;
		struct {
			char reserved0[1];
			char key;
			char reserved1[2];
		} keyboardpacket;
		int raw;
	};
} __attribute__((packed)) inputring_t;

typedef struct {
    inputring_t input[512];
    int tail;
	int cycle;
} ring_buffer_t;

typedef struct devfs_dev {
    
    int (*write)(char* buffer,uint64_t size,uint64_t offset);
    int (*read)(char* buffer,long hint_size);
    int (*askforpipe)(pipe_t* pipe);
    int (*instantreadpipe)(pipe_t* pipe);
    int (*ioctl)(unsigned long request, void* arg, void* result);
	int (*stat)(char* buffer);

    struct devfs_dev* next;

    char loc[2048];
    

} __attribute__((packed)) devfs_dev_t;

typedef struct {
	termios_t term;
	winsize_t winsz;
} tty_dev_t;

void input_send(char byte);

void devfs_reg_device(const char* name,int (*write)(char* buffer,uint64_t size,uint64_t offset),int (*read)(char* buffer,long hint_size),int (*askforpipe)(pipe_t* pipe),int (*instantreadpipe)(pipe_t* pipe),int (*ioctl)(unsigned long request, void* arg, void* result));
void devfs_advanced_configure(const char* name,int (*stat)(char* buffer)); // i need this cuz i dont want to rewrite all code which uses devfs_reg_device

void devfs_init(filesystem_t* fs);