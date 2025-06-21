#include <generic/VFS/vfs.hpp>
#include <stdint.h>
#include <other/log.hpp>
#include <other/assert.hpp>
#include <other/string.hpp>
#include <config.hpp>
#include <generic/locks/spinlock.hpp>
#include <generic/VFS/tmpfs.hpp>
#include <generic/VFS/devfs.hpp>
#include <generic/VFS/procfs.hpp>

mount_location_t mount_points[MAX_MOUNT_POINTS];
uint16_t mount_points_ptr = 0;

char vfs_spinlock = 0;

mount_location_t* vfs_find_the_nearest_mount(char* loc) {
    if (!loc) return 0;

    mount_location_t* nearest = 0;
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
                        else
                            break; 
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

int VFS::Read(char* buffer,char* filename,long hint_size) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->readfile) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); // /somemount/file to /file 
    int status = fs->fs->readfile(buffer,filename_as_fs,hint_size);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Write(char* buffer,char* filename,uint64_t size,char is_symlink_path,uint64_t offset) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->writefile) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->writefile(buffer,filename_as_fs,size,is_symlink_path,offset);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Touch(char* filename) {
    if(!filename) return -1;

    //spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->touch) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->touch(filename_as_fs);
    //spinlock_unlock(&vfs_spinlock);
    return status;
}
    
int VFS::Create(char* filename,int type) {
    if(!filename) return -1;
 
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    //Log(LOG_LEVEL_DEBUG,"Creating %s\n",filename);

    if(!fs) return -1;
    if(!fs->fs->create) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->create(filename_as_fs,type);
    return status;
}

int VFS::Remove(char* filename) {
    if(!filename) return -1;

    spinlock_lock(&vfs_spinlock);
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->rm) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1)); 
    int status = fs->fs->rm(filename_as_fs);
    spinlock_unlock(&vfs_spinlock);
    return status;
}

char VFS::Exists(char* filename) {
    if(!filename) return -1;

    //spinlock_lock(&vfs_spinlock);
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->exists) return 0;
    
    char* filename_as_fs = filename;
    if(String::strcmp(filename,"/"))
        filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->exists(filename_as_fs);
    //spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::Stat(char* filename, char* buffer,char follow_symlinks) {
    if(!filename) return -1;

    //spinlock_lock(&vfs_spinlock);
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->stat) return -15;

    char* filename_as_fs = filename;
    if(String::strcmp(filename,"/"))
        filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    String::memset(buffer,0,sizeof(filestat_t));
    int status = fs->fs->stat(filename_as_fs,buffer,follow_symlinks);
    //spinlock_unlock(&vfs_spinlock);
    return status;
}

int VFS::AskForPipe(char* filename,pipe_t* pipe) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    if(!fs->fs->askforpipe) return -15;
    
    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->askforpipe(filename_as_fs,pipe);
    return status;
}

int VFS::InstantPipeRead(char* filename,pipe_t* pipe) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    if(!fs->fs->instantreadpipe) return -15;
    
    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->instantreadpipe(filename_as_fs,pipe);
    return status;
}

int VFS::Ioctl(char* filename,unsigned long request, void *arg, int *result) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;

    if(!fs->fs->ioctl) return 0;
    
    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->ioctl(filename_as_fs,request,arg,result);
    return status;
}

int VFS::Iterate(char* filename,filestat_t* stat) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->iterate) return -15;
    if(!stat) return 0; 

    int status = fs->fs->iterate(stat);
    return status;
}

int VFS::Chmod(char* filename,uint64_t mode) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->chmod) return -15;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->chmod(filename_as_fs,mode);

    vfs_spinlock = 0;

    return status;
}

int VFS::Count(char* filename,int idx,int count) {
    if(!filename) return -1;
    
    mount_location_t* fs = vfs_find_the_nearest_mount(filename);

    if(!fs) return -1;
    if(!fs->fs->count) return 0;

    char* filename_as_fs = (char*)((uint64_t)filename + (String::strlen(fs->loc) - 1));
    int status = fs->fs->count(filename,idx,count);
    return status;
}

void VFS::Init() {
    filesystem_t* tmpfs = new filesystem_t;
    String::memset(tmpfs,0,sizeof(filesystem_t));
    mount_points[0].loc = "/";
    mount_points[0].fs = tmpfs;
    TMPFS::Init(tmpfs);
    INFO("TmpFS initializied\n");

    filesystem_t* devfs = new filesystem_t;
    String::memset(devfs,0,sizeof(filesystem_t));
    mount_points[1].loc = "/dev/";
    mount_points[1].fs = devfs;
    devfs_init(devfs);
    INFO("DevFS initializied\n");

    filesystem_t* procfs = new filesystem_t;
    String::memset(procfs,0,sizeof(filesystem_t));
    mount_points[2].loc = "/proc/";
    mount_points[2].fs = procfs;
    ProcFS::Init(procfs);
    INFO("ProcFS initializied\n");

    

}