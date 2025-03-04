
#include <stdint.h>
#include <generic/VFS/tmpfs.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/memory/heap.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/paging.hpp> // just because it have align up macro, why not ?
#include <other/string.hpp>
#include <other/log.hpp>
#include <other/unixlike.hpp>
#include <drivers/hpet/hpet.hpp>
#include <config.hpp>


data_file_t* root;
data_file_t* last;

// i dont need spinlocks because all calls going from vfs, which already have spinlock

data_file_t* tmpfs_scan_for_file(char* name) {
    data_file_t* current = root;
    
    while(current) {
        if(!String::strcmp(name,current->name))
            return current;
        current = current->next;
    }

    return 0;

}

void tmpfs_free_file_content(data_file_t* file) {

    if(!file->content) return;
    if(!(file->type == TMPFS_TYPE_FILE)) return;

    uint64_t aligned_size = ALIGNPAGEUP(file->size_of_content);
    PMM::VirtualBigFree(file->content,aligned_size); // the min file size is 4k cuz i have 4k pages

}

int tmpfs_create(char* name,int type) {
    if(!name) return 2;
    if(name[0] != '/') return 3; //it should be full path
    if(type > 1) return 4;

    if(!String::strcmp(name,"/")) return 5; // why not

    if(tmpfs_scan_for_file(name)) return 6;

    data_file_t* new_data = new data_file_t;

    String::memset(new_data,0,sizeof(data_file_t));

    new_data->name = name;
    new_data->parent = last;
    last->next = new_data;
    last = new_data;

    new_data->file_change_date = convertToUnixTime();
    new_data->file_create_date = convertToUnixTime();

    return 0;

}

int tmpfs_rm(char* filename) {

    if(!filename) return 2;
    if(filename[0] != '/') return 2; //it should be full path

    if(!String::strcmp(filename,"/")) return 2; // wtf you dont need to remove root :sob:

    if(!tmpfs_scan_for_file(filename)) return -1;

    data_file_t* data = tmpfs_scan_for_file(filename);
    if(data->protection) return 8;
    data_file_t* parent = data->parent;
    data_file_t* next = data->next;
    parent->next = next;

    tmpfs_free_file_content(data); // TODO: clear all directory's files

    delete (void*)data;

    return 0;

}

int tmpfs_writefile(char* buffer,char* filename,uint64_t size) {

    if(!size) return -2;
    if(size > TMPFS_MAX_SIZE) return -1;
    if(!buffer) return 1;
    if(!filename) return 2;
    if(filename[0] != '/') return 3; 

    if(!String::strcmp(filename,"/")) return 4;

    if(!tmpfs_scan_for_file(filename)) return 5;

    data_file_t* file = tmpfs_scan_for_file(filename);

    if(file->type != TMPFS_TYPE_FILE) return 6;
    if(file->protection) return 8;

    tmpfs_free_file_content(file);

    file->size_of_content = size;
    file->content = (char*)PMM::VirtualBigAlloc(SIZE_TO_PAGES(size));
    file->file_change_date = convertToUnixTime();

    if(!file->content)
        return 7;

    String::memcpy(file->content,buffer,size);

    return 0;

}

int tmpfs_readfile(char* buffer,char* filename) {
    if(!buffer) return 1;
    if(!filename) return 2;
    if(filename[0] != '/') return 3; 

    if(!String::strcmp(filename,"/")) return 4;

    if(!tmpfs_scan_for_file(filename)) return 5;

    data_file_t* file = tmpfs_scan_for_file(filename);

    if(file->type != TMPFS_TYPE_FILE) return 6;

    if(!file->content)
        return 7;

    String::memcpy(buffer,file->content,file->size_of_content);

    return 0;
}

char tmpfs_exists(char* filename) {
    if(!filename) return 0;
    return tmpfs_scan_for_file(filename) ? 1 : 0;
}

int tmpfs_touch(char* filename) {
    if(!filename) return -1;

    data_file_t* data = tmpfs_scan_for_file(filename);

    if(!data) {
        if(tmpfs_create(filename,TMPFS_TYPE_FILE) == 0) {
            data = tmpfs_scan_for_file(filename);
        } else {
            return 1;
        }
    }

    if(data->protection) return 2;

    data->file_change_date = convertToUnixTime();

    return 0;

}

void TMPFS::Init(filesystem_t* fs) {
    data_file_t* root_d = new data_file_t;
    String::memset(root_d,0,sizeof(data_file_t));
    root_d->type = TMPFS_TYPE_DIRECTORY;
    root_d->name = "/";
    root_d->protection = 1;
    root = root_d;
    last = root_d;
    fs->is_in_ram = 1;

    fs->create = tmpfs_create;
    fs->exists = tmpfs_exists;
    fs->readfile = tmpfs_readfile;
    fs->writefile = tmpfs_writefile;
    fs->exists = tmpfs_exists;
    fs->touch = tmpfs_touch;
    
}