#pragma once

#include <cstdint>
#include <atomic>
#include <utils/bitmap.hpp>
#include <utils/ringbuffer.hpp>
#include <generic/vfs.hpp>

#define EVDEV_TYPE_MOUSE 1
#define EVDEV_TYPE_KEYBOARD 2

struct evdevtimeval {
    long long    tv_sec;         /* seconds */
    long long    tv_usec;        /* microseconds */

};
struct input_event {
	struct evdevtimeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
	std::uint16_t type;
	std::uint16_t code;
	std::int32_t value;
};

#define BTN_MOUSE		0x110
#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_SIDE		0x113
#define BTN_EXTRA		0x114
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116
#define BTN_TASK		0x117

#define _IOC_NONE       0U
#define _IOC_WRITE      1U
#define _IOC_READ       2U

#define _IOC_NRBITS     8
#define _IOC_TYPEBITS   8
#define _IOC_SIZEBITS   14
#define _IOC_DIRBITS    2
#define _IOC_NRMASK     ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK   ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK   ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK    ((1 << _IOC_DIRBITS) - 1)
#define _IOC_NRSHIFT    0
#define _IOC_TYPESHIFT  (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT  (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT   (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC(dir, type, nr, size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))

#define _IO(type, nr)           _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, size)    _IOC(_IOC_READ, (type), (nr), sizeof(size))
#define _IOW(type, nr, size)    _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size)   _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))

#define _IOC_DIR(nr)            (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr)           (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)             (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr)           (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

#define EVIOC_MASK_SIZE(nr)     ((nr) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))
#define EVIOCGVERSION           _IOR('E', 0x01, int)                   
#define EVIOCGID                _IOR('E', 0x02, struct input_id)    
#define EVIOCGREP               _IOR('E', 0x03, int[2])             
#define EVIOCSREP               _IOW('E', 0x03, int[2])               
#define EVIOCGKEYCODE           _IOR('E', 0x04, int[2])               
#define EVIOCSKEYCODE           _IOW('E', 0x04, int[2])                
#define EVIOCGNAME(len)         _IOC(_IOC_READ, 'E', 0x06, len)       
#define EVIOCGPHYS(len)         _IOC(_IOC_READ, 'E', 0x07, len)        
#define EVIOCGUNIQ(len)         _IOC(_IOC_READ, 'E', 0x08, len)       
#define EVIOCGPROP(len)         _IOC(_IOC_READ, 'E', 0x09, len)        
#define EVIOCGMTSLOTS(len)      _IOC(_IOC_READ, 'E', 0x0a, len) 
#define EVIOCGKEY(len)          _IOC(_IOC_READ, 'E', 0x18, len)       
#define EVIOCGLED(len)          _IOC(_IOC_READ, 'E', 0x19, len)     
#define EVIOCGSND(len)          _IOC(_IOC_READ, 'E', 0x1a, len)       
#define EVIOCGSW(len)           _IOC(_IOC_READ, 'E', 0x1b, len)      
#define EVIOCGBIT(ev, len)      _IOC(_IOC_READ, 'E', 0x20 + (ev), len) 
#define EVIOCGABS(abs)          _IOR('E', 0x40 + (abs), struct input_absinfo) 
#define EVIOCSABS(abs)          _IOW('E', 0x40 + (abs), struct input_absinfo) 
#define EVIOCSFF                _IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect)) 
#define EVIOCRMFF               _IOW('E', 0x81, int)                  
#define EVIOCGEFFECTS           _IOR('E', 0x84, int)                  
#define EVIOCGRAB               _IOW('E', 0x90, int)                
#define EVIOCREVOKE             _IOW('E', 0x91, int)                 
#define EVIOCSMASK              _IOW('E', 0x92, struct input_mask)    
#define EV_MAX          0x1f
#define KEY_MAX         0x2ff
#define REL_MAX         0x0f
#define ABS_MAX         0x3f
#define MSC_MAX         0x07
#define LED_MAX         0x0f
#define SND_MAX         0x07
#define FF_MAX          0x7f
#define SW_MAX          0x0f

#define EV_SYN			0x00
#define EV_KEY			0x01
#define EV_REL			0x02
#define EV_ABS			0x03
#define EV_MSC			0x04
#define EV_SW			0x05
#define EV_LED			0x11
#define EV_SND			0x12
#define EV_REP			0x14
#define EV_FF			0x15
#define EV_PWR			0x16
#define EV_FF_STATUS		0x17
#define EV_MAX			0x1f
#define EV_CNT			(EV_MAX+1)

#define BTN_MOUSE		0x110
#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_SIDE		0x113
#define BTN_EXTRA		0x114
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116
#define BTN_TASK		0x117

namespace evdev {
    struct evdev_node {
        int num;
        int type;
        bool is_root;

        utils::bitmap* ev_bitmap;
        utils::ring_buffer<input_event>* main_ring;

        char path[64];
        char name[2048];

        evdev_node* next;
    };

    static_assert(sizeof(evdev_node) < 4096, "stuf");

    int create(char* name, int type);
    void submit(int num, input_event event);
    void init_default(vfs::node* node);

}