
#include <cstdint>
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/devfs.hpp>
#include <etc/list.hpp>

#pragma once

#define UDEV_TYPE_MOUSE 1
#define UDEV_TYPE_KEYBOARD 2

typedef int __s32;

struct evdevtimeval {
        long long    tv_sec;         /* seconds */
        long long    tv_usec;        /* microseconds */
};


struct input_event {
	struct evdevtimeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
	__u16 type;
	__u16 code;
	__s32 value;
};

namespace vfs {

    struct evdev_node {
        int num;
        int type;

        Lists::Ring* readring;
        Lists::Ring* writering;

        char path[64]; // internal build device
        char name[2048]; // device name

        evdev_node* next;
    };

    class evdev {
    public:
        static int create(char* name, int type);
        static void submit(int num);
        static void mount(vfs_node_t* node);
    };  
};