#include <generic/VFS/vfs.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/assert.hpp>
#include <other/string.hpp>
#include <config.hpp>

mount_location_t mount_points[MAX_MOUNT_POINTS];
uint16_t mount_points_ptr = 0;

int vfs_cmp(char* a,char* b) {

}

mount_location_t* vfs_find_the_nearest_mount(char* loc) {
    if (!loc) return NULL;

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

    return nearest;
}


void VFS::Init() {
    mount_points[0].loc = "/";
    mount_points[1].loc = "/mount";
    mount_points[2].loc = "/home/cpplover0";
    mount_points[3].loc = "/home";
    mount_points[4].loc = "/home/infernox";
    const char* test1 = "/home/cpplover0/zxc/mon/pon";
    Log("\"%s\": %s \n",test1,vfs_find_the_nearest_mount((char*)test1)->loc);
    test1 = "/home/infenox/564564/";
    Log("\"%s\": %s \n",test1,vfs_find_the_nearest_mount((char*)test1)->loc);
    test1 = "/mount/mcz";
    Log("\"%s\": %s \n",test1,vfs_find_the_nearest_mount((char*)test1)->loc);
    test1 = "/";
    Log("\"%s\": %s \n",test1,vfs_find_the_nearest_mount((char*)test1)->loc);
    test1 = "/mount";
    Log("\"%s\": %s \n",test1,vfs_find_the_nearest_mount((char*)test1)->loc);
}