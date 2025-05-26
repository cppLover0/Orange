
#include <stdint.h>
#include <generic/VFS/ustar.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <generic/VFS/vfs.hpp>
#include <other/assert.hpp>
#include <other/string.hpp>
#include <other/log.hpp>
#include <generic/VFS/tmpfs.hpp>
#include <generic/memory/paging.hpp> // it have align macros

inline int oct2bin(unsigned char *str, int size) {
    uint64_t n = 0;
    volatile unsigned char *c = str;
    for(int i = 0;i < size;i++) {
        n *= 8;
        n += *c - '0';
        c++;
        
    }
    return n;
}

#define MAX_PATH 1024
#define MAX_PARTS 100

char* __ustar__strtok(char* str, const char* delim) {
    static char* next_token = 0;
    char* token;

    if (str != 0) {
        next_token = str;
    }

    if (next_token == 0 || *next_token == '\0') {
        return 0;
    }

    while (*next_token != '\0') {
        const char* d = delim;
        int is_delim = 0;
        while (*d != '\0') {
            if (*next_token == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (!is_delim) {
            break;
        }
        next_token++;
    }

    if (*next_token == '\0') {
        next_token = 0;
        return 0;
    }

    token = next_token;

    while (*next_token != '\0') {
        const char* d = delim;
        while (*d != '\0') {
            if (*next_token == *d) {
                *next_token = '\0';
                next_token++;
                return token;
            }
            d++;
        }
        next_token++;
    }

    next_token = 0;
    return token;
}

uint64_t resolve_count(char* str,uint64_t sptr,char delim) {
    char* current = str;
    uint16_t att = 0;
    uint64_t ptr = sptr;
    uint64_t ptr_count = 0;
    while(current[ptr] != delim) {
        if(att > 1024)
            return 0;
        att++;

        if(ptr != 0) {
            ptr_count++;
            ptr--;
        }
    }
    return ptr;
}

void resolve_path(const char* inter,const char* base, char *result, char spec) {
    char buffer_in_stack[1024];
    char buffer2_in_stack[1024];
    char* buffer = (char*)buffer_in_stack;
    char* final_buffer = (char*)buffer2_in_stack;
    uint64_t ptr = String::strlen((char*)base);
    char is_first = 1;
    char is_full = 0;

    String::memset(buffer_in_stack,0,1024);
    String::memset(buffer2_in_stack,0,1024);

    String::memcpy(final_buffer,base,String::strlen((char*)base));

    if(String::strlen((char*)inter) == 1 && inter[0] == '.') {
        String::memset(result,0,1024);
        String::memcpy(result,base,String::strlen((char*)base));
        return;
    }

    if(inter[0] == '/') {
        ptr = 0;
        String::memset(final_buffer,0,1024);
        is_full = 1;
    }

    if(spec)
        is_first = 0;

    buffer = __ustar__strtok((char*)inter,"/");
    while(buffer) {

        if(is_first && !is_full) {
            uint64_t mm = resolve_count(final_buffer,ptr,'/');

            if(ptr < mm) {
                final_buffer[0] = '/';
                final_buffer[1] = '\0';
                ptr = 1;
                continue;
            } 

            ptr = mm;
            final_buffer[ptr] = '\0';
            is_first = 0;
        }

        if(!String::strcmp(buffer,"..")) {
            uint64_t mm = resolve_count(final_buffer,ptr,'/');

            if(ptr < mm) {
                final_buffer[0] = '/';
                final_buffer[1] = '\0';
                ptr = 1;
                continue;
            } 

            ptr = mm;
            final_buffer[ptr] = '\0';
            


        } else if(String::strcmp(buffer,"./") && String::strcmp(buffer,".")) {

            final_buffer[ptr] = '/';
            ptr++;

            uint64_t mm = String::strlen(buffer);
            String::memcpy((char*)((uint64_t)final_buffer + ptr),buffer,mm);
            ptr += mm;
            final_buffer[ptr] = '\0';
        } 
        
        buffer = __ustar__strtok(0,"/");
    }
    
    String::memset(result,0,1024);
    String::memcpy(result,final_buffer,String::strlen(final_buffer));
    //Log("F %s\n",result);

}

void USTAR::ParseAndCopy() {
    LimineInfo info;
    pAssert(info.initrd,"Can't continue without initrd");

    ustar_t* current = (ustar_t*)info.initrd->modules[0]->address;
	limine_file* initrd = info.initrd->modules[0];

    pAssert(!String::strncmp(current->ustar,"ustar",5),"Invalid initrd");
    Log(LOG_LEVEL_INFO,"Valid initrd !\n");

	uint64_t actual_tar_ptr_end = ((uint64_t)initrd->address + initrd->size) - 1024; //substract first header and his content

	while((uint64_t)current < actual_tar_ptr_end) {
		uint8_t type = oct2bin((uint8_t*)&current->type,1);
        uint64_t aligned_size;
		if(type == 0) {

            char* filename = (char*)((uint64_t)current->file_name + 1);

            int _2 = VFS::Create(filename,0);

            int size = oct2bin((uint8_t*)current->file_size,String::strlen(current->file_size));

            aligned_size  = CALIGNPAGEUP(oct2bin((uint8_t*)&current->file_size,String::strlen(current->file_size)),512);

            VFS::Write((char*)((uint64_t)current + 512),filename,size,0,0);

		} else if(type == 5) {

            char* filename = (char*)((uint64_t)current->file_name + 1);

            int _2 = VFS::Create(filename,1);

            int size = oct2bin((uint8_t*)current->file_size,String::strlen(current->file_size));

            aligned_size  = CALIGNPAGEUP(oct2bin((uint8_t*)&current->file_size,String::strlen(current->file_size)),512);

        } else if(type == 2) { 
            char* filename = (char*)((uint64_t)current->file_name + 1);

            int _2 = VFS::Create(filename,2);

            int size = oct2bin((uint8_t*)current->file_size,String::strlen(current->file_size));

            aligned_size  = CALIGNPAGEUP(oct2bin((uint8_t*)&current->file_size,String::strlen(current->file_size)),512);

            char result_path[MAX_PATH];
            String::memset(result_path,0,MAX_PATH);

            resolve_path(current->name_linked,filename,result_path,0);

            //Log("PATH: %s\n",result_path);

            VFS::Write((char*)result_path,filename,String::strlen(result_path),1,0);
        } else {
            aligned_size = 512;
        }
        current = (ustar_t*)((uint64_t)current + aligned_size + 512);

    }

    //tmpfs_dump();

}