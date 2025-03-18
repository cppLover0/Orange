
#include <stdint.h>
#include <generic/VFS/ustar.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <generic/VFS/vfs.hpp>
#include <other/assert.hpp>
#include <other/string.hpp>
#include <other/log.hpp>
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

void USTAR::ParseAndCopy() {
    LimineInfo info;
    pAssert(info.initrd,"Can't continue without initrd");

    ustar_t* current = (ustar_t*)info.initrd->modules[0]->address;
	limine_file* initrd = info.initrd->modules[0];

    pAssert(!String::strncmp(current->ustar,"ustar",5),"Invalid initrd");
    Log("Valid initrd !\n");

	uint64_t actual_tar_ptr_end = ((uint64_t)initrd->address + initrd->size) - 1024; //substract first header and his content

	while((uint64_t)current < actual_tar_ptr_end) {
		uint64_t type = oct2bin((uint8_t*)&current->type,1);
        uint64_t aligned_size;
		if(type == 0) {
            char* filename = (char*)((uint64_t)current->file_name + 1);

            int _2 = VFS::Create(filename,0);

            int size = oct2bin((uint8_t*)current->file_size,String::strlen(current->file_size));

            aligned_size  = CALIGNPAGEUP(oct2bin((uint8_t*)&current->file_size,String::strlen(current->file_size)),512);

            VFS::Write((char*)((uint64_t)current + 512),filename,size);

		} else {
            aligned_size = 512;
        }
        current = (ustar_t*)((uint64_t)current + aligned_size + 512);

    }

}