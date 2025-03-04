#include <generic/VFS/vfs.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/assert.hpp>
#include <other/string.hpp>
#include <config.hpp>
#include <generic/locks/spinlock.hpp>
#include <generic/VFS/tmpfs.hpp>

mount_location_t mount_points[MAX_MOUNT_POINTS];
uint16_t mount_points_ptr = 0;

char vfs_spinlock = 0;

mount_location_t* vfs_find_the_nearest_mount(char* loc) {
    if (!loc) return NULL;

    spinlock_lock(&vfs_spinlock);

    mount_location_t* nearest = NULL;
    int max_match = 0;

    for(uint16_t i = 0;i < MAX_MOUNT_POINTS;i++) {
        if(mount_points[i].loc) {
            int temp_match = 0;
            int mount_length = String::strlen(mount_points[i].loc);
            int loc_length = String::strlen(loc);
            if(mount_length <= loc_length) {
                for(int b = 0;b < loc_length;b++) {
                    if(mount_points[i].loc[b]) {
                       if(mount_points[i].loc[b] == loc[b]) 
                        temp_match++; 
                    } else
                        break;
                }
                if(temp_match > max_match) {
                    max_match = temp_match;
                    nearest = &mount_points[i];
                }
            } 
        }
    }

    spinlock_unlock(&vfs_spinlock);

    return nearest;
}

void VFS::Init() {
    filesystem_t* tmpfs = new filesystem_t;
    mount_points[0].loc = "/";
    mount_points[0].fs = tmpfs;
    TMPFS::Init(tmpfs);
    Log("TmpFS initializied\n");
}