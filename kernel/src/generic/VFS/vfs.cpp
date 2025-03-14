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

int VFS::Read(char* buffer,char* filename) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); // /somemount/file to /file 
    int status = fs->fs->readfile(buffer,filename_as_fs);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Write(char* buffer,char* filename,uint64_t size) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->writefile(buffer,filename_as_fs,size);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Touch(char* filename) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->touch(filename_as_fs);
    spinlock_unlock(&vfs_spinlock);
    return status;
}
    
int VFS::Create(char* filename,int type) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->create(filename_as_fs,type);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Remove(char* filename) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->rm(filename_as_fs);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

char VFS::Exists(char* filename) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    
    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->exists(filename_as_fs);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Stat(char* filename, char* buffer) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    
    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->stat(filename_as_fs,buffer);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

void VFS::Init() {
    filesystem_t* tmpfs = new filesystem_t;
    mount_points[0].loc = "/";
    mount_points[0].fs = tmpfs;
    TMPFS::Init(tmpfs);
    Log("TmpFS initializied\n");
}