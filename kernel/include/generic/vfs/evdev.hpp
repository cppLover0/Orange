
#include <cstdint>
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/devfs.hpp>
#include <etc/list.hpp>

#pragma once

#define EVDEV_TYPE_MOUSE 1
#define EVDEV_TYPE_KEYBOARD 2

typedef int __s32;

struct evdevtimeval {
        long long    tv_sec;         /* seconds */
        long long    tv_usec;        /* microseconds */
};

#include <etc/list.hpp>

struct input_event {
	struct evdevtimeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
	__u16 type;
	__u16 code;
	__s32 value;
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

#include <cstdint>
#include <atomic>
#include <generic/locks/spinlock.hpp>
#include <etc/libc.hpp>
#include <etc/etc.hpp>

#include <etc/logging.hpp>

#pragma once

typedef struct {
    std::uint32_t cycle;
    input_event evdev_ev;
    
} zring_obj_t;

typedef struct {
    zring_obj_t* objs;
    int bytelen;
    int tail;
    int size;
    int cycle;
} zring_buffer_t;

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

class evdev_modif_ring {
    private:
    public:
        zring_buffer_t ring;

        locks::spinlock ring_lock;

        std::uint64_t read_counter = 0;
        std::uint64_t write_counter = 0;

        void send(input_event ev) {
            ring_lock.lock();
            ring.objs[ring.tail].cycle = ring.cycle;
            ring.objs[ring.tail].evdev_ev = ev;

            read_counter++;

            if (++ring.tail == ring.size) {
                ring.tail = 0;
                ring.cycle = !ring.cycle;
            }
            ring_lock.unlock();
        }

        int setup_bytelen(int len) {
            ring.bytelen = len;
            return len;
        }

        int isnotempty(int* queue0, char* cycle0) {
            char cycle = *cycle0;
            int queue = *queue0;
            while (ring.objs[queue].cycle == cycle) {
                return 1;
                queue = queue + 1;
                if (queue == ring.size) {
                    queue = 0;
                    cycle = !(cycle);
                }
            }
            return 0;
        }

        int receivevals(void* out, int max, std::uint8_t* cycle, std::uint32_t* queue) {
            ring_lock.lock();
            int len = 0;
            while (ring.objs[*queue].cycle == *cycle && len * ring.bytelen < max) {

                ((input_event*)out)[len] = ring.objs[*queue].evdev_ev;
                *queue = *queue + 1;
                if (*queue == ring.size) {
                    *queue = 0;
                    *cycle = !(*cycle);
                }
                len++;
            }
            ring_lock.unlock();
            return len * ring.bytelen;
        }

        evdev_modif_ring(std::size_t size_elements) {
            ring.objs = new zring_obj_t[size_elements];
            ring.size = size_elements;
            ring.tail = 0;
            ring.cycle = 1;
            ring.bytelen = sizeof(input_event);
            ring_lock.unlock();
            memset(ring.objs, 0, sizeof(zring_obj_t) * size_elements);
        }

        ~evdev_modif_ring() {
            delete[] ring.objs;
        }

};

namespace vfs {

    struct evdev_node {
        int num;
        int type;

        Lists::Bitmap* ev_bitmap;
        evdev_modif_ring* main_ring;

        char path[64]; // internal build device
        char name[2048]; // device name

        evdev_node* next;
    };

    class evdev {
    public:
        static int create(char* name, int type);
        static void submit(int num, input_event event);
        static void mount(vfs_node_t* node);
    };  
};